//! The backup and device-replacement flows as state machines the core drives and the platform only feeds
//! (`SWR-MG-02…10`; issue #569 package 6e, `CLAUDE.md` §20).
//!
//! The rules of `migration` and `bundle_key` answer one question each. The ORDER in which they are asked is also a rule:
//! open the bundle before taking a claim, verify before confirming, confirm before the old phone clears, key first when it
//! clears. If each app wrote that order itself, five apps (iOS, Android, Windows, macOS, web) would hold five copies of the
//! safety logic. So the order lives here, once, and an app implements only what the core cannot do for it: touching its own
//! storage, its route, its Argon2 and its randomness.
//!
//! # Protocol
//! `{"op":"flow","flow":"backup"|"inspect"|"restore"|"clear","params":{…}}` starts a flow; `{"op":"flow","flow":…,
//! "state":<from the last answer>,"result":<what the effect returned>}` continues it. The answer is either
//! `{"effect":{"type":…,…},"state":…}`, meaning "do this and call me again with the outcome", or `{"done":{…}}`. The state
//! is opaque to the platform, is never persisted by it, and holds the flow's working data (items, the bundle) for the
//! length of the flow only. **No secret and no key is ever in a state**: the driver holds the secret and asks
//! `deriveBundleKey` for the key; the key arrives in a `result`, is used in the same call and is dropped.
//!
//! # Effects
//! | type | args | result |
//! |------|------|--------|
//! | `random` | `bytes` | `{bytes: hex}` from the platform's CSPRNG |
//! | `deriveKey` | `method`, `kdf` | `{key: hex}`, answered by the DRIVER with the core's own `deriveBundleKey` over the secret the driver holds; a host never implements it |
//! | `readKeys` | `keys` | `{values: {key: text}}`, a key this phone holds nothing for left out |
//! | `writeKeys` | `remove`, `set` | `{ok}`: remove every key in `remove`, then write every `key: text` in `set` |
//! | `readItems` | `names` | `{items: {name: value}}`, a name this phone holds nothing for left out |
//! | `applyItem` | `name`, `value` | `{ok: true}` |
//! | `getBundle` / `putBundle` | `bundle` (hex) | `{bundle: hex or null}` / `{ok}` |
//! | `readClaim` / `createClaim` | `claim` | `{claim: object or null}` / `{created: bool}` |
//! | `writeConfirmation` / `readConfirmation` | `confirmation` | `{ok}` / `{confirmation: object or null}` |
//! | `loadClearProgress` / `runClearStep` / `markClearStep` | `step` (and, for `clearAppHeldState`, `keys`) | `{done: [step]}` / `{ok: bool}` / `{ok}` |
//!
//! # Two ways an item is stored
//! An item named in the host's `keyItems` is stored as plain keys in the app's key-value store: the core knows which keys
//! make it up (`common/npps/migration-keys.json`), asks for them with `readKeys` and `writeKeys`, refuses a bundle item
//! that names a key it does not own, and reads back what it wrote. A host implements only get, put and remove. An item
//! named in `hostItems` (consent, and any item an app stores another way) is read and written whole with `readItems` and
//! `applyItem`. An item in neither is not applied, and the report says so.
//!
//! Releasing a claim is the person's explicit act and is not a flow: the platform calls its route directly (`SWR-MG-07`).

use crate::migration::{all_keys, dispatch, keys_of};
use serde_json::{json, Value};

type R<T> = Result<T, String>;

const CONFIRM_ATTEMPTS: u64 = 3;
const TOKEN_BYTES: u64 = 16;
const SALT_AND_NONCE_BYTES: u64 = 32 + 12;

fn effect(state: Value, e: Value) -> R<Value> {
    Ok(json!({ "effect": e, "state": state }))
}

fn done(v: Value) -> R<Value> {
    Ok(json!({ "done": v }))
}

fn refused(reason: &str) -> R<Value> {
    done(json!({ "ok": false, "reason": reason }))
}

fn s<'a>(v: &'a Value, key: &str) -> R<&'a str> {
    v[key].as_str().ok_or_else(|| format!("'{key}' is required"))
}

pub fn flow(req: &Value) -> R<Value> {
    let name = s(req, "flow")?;
    // A start carries `params` and no `state`; a continuation carries both `state` and `result`.
    match name {
        "backup" => backup(req),
        "inspect" => inspect(req),
        "restore" => restore(req),
        "clear" => clear(req),
        other => Err(format!("unknown flow '{other}'")),
    }
}

fn step_of(req: &Value) -> &str {
    req["state"]["step"].as_str().unwrap_or("start")
}

/// Ask for what the backup still needs to read, then for the randomness to seal it.
fn gather(mut st: Value) -> R<Value> {
    let keyed: Vec<String> = st["keyed"].as_array().into_iter().flatten().filter_map(Value::as_str).map(String::from).collect();
    if !keyed.is_empty() {
        let keys: Vec<String> = keyed.iter().flat_map(|i| keys_of(i)).collect();
        st["step"] = json!("keys");
        return effect(st, json!({ "type": "readKeys", "keys": keys }));
    }
    let adapter = st["adapter"].clone();
    if adapter.as_array().is_some_and(|a| !a.is_empty()) {
        st["step"] = json!("items");
        return effect(st, json!({ "type": "readItems", "names": adapter }));
    }
    st["step"] = json!("random");
    effect(st, json!({ "type": "random", "bytes": SALT_AND_NONCE_BYTES }))
}

// ─── backup ─────────────────────────────────────────────────────────────────────────────────────────────────────────

fn backup(req: &Value) -> R<Value> {
    let mut st = req["state"].clone();
    match step_of(req) {
        "start" => {
            let p = &req["params"];
            let kind = s(p, "kind")?;
            let method = s(p, "method")?;
            let inv = dispatch("inventory", &json!({ "kind": kind }))?;
            let offered = dispatch("offerRoutes", &json!({ "kind": kind, "routes": [p["route"].clone()] }))?;
            let id = p["route"]["id"].as_str().unwrap_or_default();
            if !offered["offered"].as_array().is_some_and(|a| a.iter().any(|x| x == id)) {
                return refused("route_not_offered");
            }
            let names = |key: &str| -> Vec<String> { p[key].as_array().into_iter().flatten().filter_map(Value::as_str).map(String::from).collect() };
            let (key_items, host_items) = (names("keyItems"), names("hostItems"));
            let carried: Vec<String> = inv["carried"].as_array().into_iter().flatten().filter_map(Value::as_str).map(String::from).collect();
            let keyed: Vec<String> = carried.iter().filter(|n| key_items.contains(n) && !keys_of(n).is_empty()).cloned().collect();
            let adapter: Vec<String> = carried.iter().filter(|n| host_items.contains(n) && !keyed.contains(n)).cloned().collect();
            let st = json!({ "step": "keys", "kind": kind, "method": method, "omitted": inv["omitted"], "keyed": keyed, "adapter": adapter, "items": {} });
            gather(st)
        }
        "keys" => {
            // Group the stored keys into their items; an item with no stored key is left out of the bundle.
            let values = &req["result"]["values"];
            let keyed: Vec<String> = st["keyed"].as_array().into_iter().flatten().filter_map(Value::as_str).map(String::from).collect();
            for item in keyed {
                let mut o = serde_json::Map::new();
                for k in keys_of(&item) {
                    if let Some(v) = values.get(&k).filter(|v| v.is_string()) {
                        o.insert(k, v.clone());
                    }
                }
                if !o.is_empty() {
                    st["items"][&item] = Value::Object(o);
                }
            }
            st["keyed"] = json!([]);
            gather(st)
        }
        "items" => {
            if let Some(found) = req["result"]["items"].as_object() {
                for (k, v) in found {
                    st["items"][k] = v.clone();
                }
            }
            st["adapter"] = json!([]);
            gather(st)
        }
        "random" => {
            let bytes = s(&req["result"], "bytes")?;
            if bytes.len() != (SALT_AND_NONCE_BYTES as usize) * 2 {
                return Err("random gave the wrong number of bytes".into());
            }
            let (salt, nonce) = bytes.split_at(64);
            let kdf = dispatch("bundleKdf", &json!({ "method": st["method"], "salt": salt }))?["kdf"].clone();
            st["nonce"] = json!(nonce);
            st["kdf"] = kdf.clone();
            let method = st["method"].clone();
            st["step"] = json!("key");
            effect(st, json!({ "type": "deriveKey", "method": method, "kdf": kdf }))
        }
        "key" => {
            let sealed = dispatch(
                "sealBundle",
                &json!({ "kind": st["kind"], "key": req["result"]["key"], "nonce": st["nonce"], "kdf": st["kdf"], "items": st["items"] }),
            )?;
            let written: Vec<String> = st["items"].as_object().map(|o| o.keys().cloned().collect()).unwrap_or_default();
            effect(
                json!({ "step": "put", "kind": st["kind"], "bundleHash": sealed["bundleHash"], "written": written, "omitted": st["omitted"] }),
                json!({ "type": "putBundle", "bundle": sealed["bundle"] }),
            )
        }
        "put" => done(json!({
            "ok": true, "kind": st["kind"], "bundleHash": st["bundleHash"], "written": st["written"], "omitted": st["omitted"],
        })),
        other => Err(format!("unknown backup step '{other}'")),
    }
}

// ─── inspect ────────────────────────────────────────────────────────────────────────────────────────────────────────

fn inspect(req: &Value) -> R<Value> {
    match step_of(req) {
        "start" => effect(json!({ "step": "bundle" }), json!({ "type": "getBundle" })),
        "bundle" => {
            let Some(bundle) = req["result"]["bundle"].as_str() else { return refused("no_bundle") };
            let h = dispatch("readBundleHeader", &json!({ "bundle": bundle }))?;
            if h["ok"] != true {
                return done(json!({ "ok": false, "reason": h["reason"] }));
            }
            done(json!({ "ok": true, "kind": h["kind"], "method": h["method"], "formatVersion": h["formatVersion"], "bundleHash": h["bundleHash"] }))
        }
        other => Err(format!("unknown inspect step '{other}'")),
    }
}

// ─── restore ────────────────────────────────────────────────────────────────────────────────────────────────────────

/// Work through the bundle's items in order. `st.queue` is what is left; each item is applied, read back and compared.
fn next_item(mut st: Value) -> R<Value> {
    loop {
        let Some(name) = st["queue"].as_array_mut().filter(|q| !q.is_empty()).map(|q| q.remove(0)) else {
            return finish(st);
        };
        let name = name.as_str().unwrap_or_default().to_string();
        let in_list = |list: &str| st[list].as_array().is_some_and(|a| a.iter().any(|x| x == name.as_str()));
        let keyed = in_list("keyItems") && !keys_of(&name).is_empty();
        if !keyed && !in_list("hostItems") {
            if let Some(a) = st["notApplied"].as_array_mut() {
                a.push(json!(name));
            }
            continue;
        }
        st["cur"] = json!(name);
        st["expect"] = st["items"][&name].clone();
        if keyed {
            st["curKind"] = json!("keys");
            st["step"] = json!("applied");
            let (remove, set) = (keys_of(&name), st["expect"].clone());
            return effect(st, json!({ "type": "writeKeys", "remove": remove, "set": set }));
        }
        st["curKind"] = json!("item");
        if name == "withdrawalState" {
            // SWR-MG-04: the withdrawal state is the union of this phone's and the bundle's.
            st["step"] = json!("wlocal");
            return effect(st, json!({ "type": "readItems", "names": ["withdrawalState"] }));
        }
        st["step"] = json!("applied");
        let value = st["expect"].clone();
        return effect(st, json!({ "type": "applyItem", "name": name, "value": value }));
    }
}

fn finish(st: Value) -> R<Value> {
    let kind = s(&st, "kind")?;
    let applied = st["applied"].clone();
    let report = dispatch("restoreReport", &json!({ "kind": kind, "present": applied }))?;
    let has_consent = applied.as_array().is_some_and(|a| a.iter().any(|x| x == "consentGrants"));
    let consent = dispatch("consentAfterRestore", &json!({ "kind": kind, "bundleHasConsentRecord": has_consent }))?;
    let result = json!({
        "ok": true, "kind": kind, "bundleHash": st["hash"], "restored": report["restored"], "missing": report["missing"],
        "omitted": report["omitted"], "notApplied": st["notApplied"], "askConsent": consent["ask"],
        "tellConsentNotRestored": consent["tellChoicesNotRestored"], "confirmed": false,
    });
    if kind == "migrate" {
        // SWR-MG-08: the confirmation is the last act, carries the hash and nothing else, and is read back.
        let hash = st["hash"].clone();
        return effect(
            json!({ "step": "confirmW", "result": result, "hash": hash, "attempts": 1 }),
            json!({ "type": "writeConfirmation", "confirmation": { "bundleHash": hash } }),
        );
    }
    done(result)
}

fn restore(req: &Value) -> R<Value> {
    let mut st = req["state"].clone();
    match step_of(req) {
        "start" => effect(
            json!({ "step": "bundle", "route": req["params"]["route"], "hostItems": req["params"]["hostItems"], "keyItems": req["params"]["keyItems"] }),
            json!({ "type": "getBundle" }),
        ),
        "bundle" => {
            let Some(bundle) = req["result"]["bundle"].as_str() else { return refused("no_bundle") };
            let h = dispatch("readBundleHeader", &json!({ "bundle": bundle }))?;
            if h["ok"] != true {
                return done(json!({ "ok": false, "reason": h["reason"] }));
            }
            st["bundle"] = json!(bundle);
            st["kind"] = h["kind"].clone();
            st["hash"] = h["bundleHash"].clone();
            st["step"] = json!("key");
            effect(st, json!({ "type": "deriveKey", "method": h["method"], "kdf": h["kdf"] }))
        }
        "key" => {
            // The bundle is opened BEFORE any claim, so a wrong passphrase never costs the person a claim only they can
            // release (SWR-MG-07). The key is used here and not kept.
            let opened = dispatch("openBundle", &json!({ "bundle": st["bundle"], "key": req["result"]["key"] }))?;
            if opened["ok"] != true {
                return done(json!({ "ok": false, "reason": opened["reason"] }));
            }
            let items = opened["items"].clone();
            let queue: Vec<Value> = items.as_object().map(|o| o.keys().map(|k| json!(k)).collect()).unwrap_or_default();
            // A keyed item may name only keys it owns, and only text: checked before any claim is taken or key written.
            let owned = |name: &str| st["keyItems"].as_array().is_some_and(|a| a.iter().any(|x| x == name)) && !keys_of(name).is_empty();
            for (name, value) in items.as_object().into_iter().flatten() {
                if owned(name) {
                    let own = keys_of(name);
                    let ok = value.as_object().is_some_and(|o| o.iter().all(|(k, v)| own.contains(k) && v.is_string()));
                    if !ok {
                        return refused("malformed");
                    }
                }
            }
            let bundle_dropped = json!({
                "kind": st["kind"], "hash": st["hash"], "route": st["route"], "hostItems": st["hostItems"], "keyItems": st["keyItems"],
                "items": items, "queue": queue, "applied": [], "notApplied": [],
            });
            if st["kind"] == "migrate" {
                let mut st2 = bundle_dropped;
                st2["step"] = json!("token");
                return effect(st2, json!({ "type": "random", "bytes": TOKEN_BYTES }));
            }
            next_item(bundle_dropped)
        }
        "token" => {
            st["token"] = req["result"]["bytes"].clone();
            st["step"] = json!("claim0");
            effect(st, json!({ "type": "readClaim" }))
        }
        "claim0" => {
            let d = dispatch(
                "claimDecision",
                &json!({
                    "kind": st["kind"], "route": { "atomicClaim": st["route"]["atomicClaim"] }, "token": st["token"],
                    "bundleHash": st["hash"], "existingClaim": req["result"]["claim"],
                }),
            )?;
            match d["action"].as_str().unwrap_or_default() {
                "already_ours" | "no_claim_needed" => next_item(st),
                "route_not_offered" => refused("route_not_offered"),
                "refuse_claimed" => refused("claimed"),
                _ => {
                    st["step"] = json!("claimW");
                    let claim = json!({ "token": st["token"], "bundleHash": st["hash"] });
                    effect(st, json!({ "type": "createClaim", "claim": claim }))
                }
            }
        }
        "claimW" => {
            st["step"] = json!("claimBack");
            effect(st, json!({ "type": "readClaim" }))
        }
        "claimBack" => {
            let own = dispatch("claimReadback", &json!({ "token": st["token"], "bundleHash": st["hash"], "readBack": req["result"]["claim"] }))?;
            if own["owned"] == true {
                next_item(st)
            } else {
                refused("claim_not_owned")
            }
        }
        "wlocal" => {
            let local = req["result"]["items"]["withdrawalState"].clone();
            let local = if local.is_null() { json!({}) } else { local };
            let name = st["cur"].as_str().unwrap_or_default().to_string();
            let merged = dispatch("mergeWithdrawal", &json!({ "local": local, "incoming": st["items"][&name] }))?;
            st["expect"] = merged.clone();
            st["step"] = json!("applied");
            effect(st, json!({ "type": "applyItem", "name": name, "value": merged }))
        }
        "applied" => {
            let name = st["cur"].clone();
            if st["curKind"] == "keys" {
                st["step"] = json!("verifyKeys");
                let keys = keys_of(name.as_str().unwrap_or_default());
                return effect(st, json!({ "type": "readKeys", "keys": keys }));
            }
            st["step"] = json!("verify");
            effect(st, json!({ "type": "readItems", "names": [name] }))
        }
        "verifyKeys" => {
            // Every key written is read back and compared before the confirmation can be written (SWR-MG-08).
            if req["result"]["values"] != st["expect"] {
                return refused("verify_failed");
            }
            let name = st["cur"].clone();
            if let Some(a) = st["applied"].as_array_mut() {
                a.push(name);
            }
            next_item(st)
        }
        "verify" => {
            let name = st["cur"].as_str().unwrap_or_default().to_string();
            // Every applied item is read back and compared before the confirmation can be written (SWR-MG-08).
            if req["result"]["items"][&name] != st["expect"] {
                return refused("verify_failed");
            }
            if let Some(a) = st["applied"].as_array_mut() {
                a.push(json!(name));
            }
            next_item(st)
        }
        "confirmW" => {
            st["step"] = json!("confirmR");
            effect(st, json!({ "type": "readConfirmation" }))
        }
        "confirmR" => {
            let hash = st["hash"].clone();
            let landed = req["result"]["confirmation"]["bundleHash"] == hash;
            let attempts = st["attempts"].as_u64().unwrap_or(1);
            if landed || attempts >= CONFIRM_ATTEMPTS {
                let mut result = st["result"].clone();
                result["confirmed"] = json!(landed);
                return done(result);
            }
            st["attempts"] = json!(attempts + 1);
            st["step"] = json!("confirmW");
            effect(st, json!({ "type": "writeConfirmation", "confirmation": { "bundleHash": hash } }))
        }
        other => Err(format!("unknown restore step '{other}'")),
    }
}

// ─── clear (the old phone) ─────────────────────────────────────────────────────────────────────────────────────────

fn plan_next(st: Value) -> R<Value> {
    let plan = dispatch("clearPlan", &json!({ "done": st["done"], "canRemoveOsBond": st["canRemoveOsBond"] }))?;
    if plan["complete"] == true {
        return done(json!({ "ok": true, "removeOsBond": plan["osBondInstruction"] }));
    }
    let step = plan["remaining"][0].clone();
    let mut st = st;
    st["cur"] = step.clone();
    st["step"] = json!("ran");
    // The old phone forgets every stored key it knows (SWR-MG-09): the core names them, so no app keeps its own list.
    if step == "clearAppHeldState" {
        return effect(st, json!({ "type": "runClearStep", "step": step, "keys": all_keys() }));
    }
    effect(st, json!({ "type": "runClearStep", "step": step }))
}

fn clear(req: &Value) -> R<Value> {
    let mut st = req["state"].clone();
    match step_of(req) {
        "start" => {
            let p = &req["params"];
            effect(
                json!({ "step": "progress", "bundleHash": s(p, "bundleHash")?, "canRemoveOsBond": p["canRemoveOsBond"].as_bool().unwrap_or(false) }),
                json!({ "type": "loadClearProgress" }),
            )
        }
        "progress" => {
            st["done"] = req["result"]["done"].clone();
            if st["done"].as_array().is_some_and(|d| !d.is_empty()) {
                // A clear already under way resumes without asking again: by now the old state is half gone (SWR-MG-08).
                return plan_next(st);
            }
            st["step"] = json!("conf");
            effect(st, json!({ "type": "readConfirmation" }))
        }
        "conf" => {
            let d = dispatch("clearDecision", &json!({ "bundleHash": st["bundleHash"], "confirmation": req["result"]["confirmation"] }))?;
            if d["clear"] != true {
                return done(json!({ "ok": false, "reason": d["reason"] }));
            }
            plan_next(st)
        }
        "ran" => {
            let step = st["cur"].clone();
            if req["result"]["ok"] != true {
                return done(json!({ "ok": false, "reason": "step_failed", "step": step }));
            }
            st["step"] = json!("marked");
            effect(st, json!({ "type": "markClearStep", "step": step }))
        }
        "marked" => {
            let step = st["cur"].clone();
            if let Some(a) = st["done"].as_array_mut() {
                a.push(step);
            }
            plan_next(st)
        }
        other => Err(format!("unknown clear step '{other}'")),
    }
}

#[cfg(test)]
mod tests {
    use crate::migration::migration_json;
    use serde_json::{json, Value};
    use std::collections::BTreeMap;

    // A route and a phone as an app would implement them: dumb storage that performs the effect it is asked for. The
    // flows are tested here once; each app's own tests only prove its driver and its storage reach the same effects.
    #[derive(Default)]
    struct Route {
        bundle: Option<String>,
        claim: Option<Value>,
        confirmation: Option<Value>,
        drop_confirmations: u32,
    }

    #[derive(Default)]
    struct Phone {
        /// The key-value store: the four items that are plain keys live here.
        kv: BTreeMap<String, String>,
        /// Items this phone stores another way (consent), read and written whole.
        items: BTreeMap<String, Value>,
        adapter: Vec<String>,
        key_items: Vec<String>,
        progress: Vec<String>,
        ran: Vec<String>,
        clear_keys: Vec<String>,
        fail_on: Option<String>,
        corrupt_on_write: bool,
        counter: u8,
        last_key: String,
    }

    const KEYED: [&str; 4] = ["protocols", "sessionHistory", "limitsProfiles", "characterisationEnrolment"];

    fn phone(adapter: &[&str], seed: u8) -> Phone {
        Phone {
            adapter: adapter.iter().map(|s| s.to_string()).collect(),
            key_items: KEYED.iter().map(|s| s.to_string()).collect(),
            counter: seed,
            ..Default::default()
        }
    }

    const ADAPTER: [&str; 2] = ["consentGrants", "withdrawalState"];
    const SECRET: &str = "a long enough passphrase";
    const ATOMIC: fn() -> Value = || json!({ "id": "memory", "atomicClaim": true, "direct": false, "authenticatedPairing": true });

    fn ask(v: Value) -> Value {
        serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
    }

    fn keys_of(item: &str) -> Vec<String> {
        ask(json!({ "op": "itemKeys" }))["carried"][item].as_array().unwrap().iter().map(|k| k.as_str().unwrap().to_string()).collect()
    }

    fn perform(e: &Value, route: &mut Route, p: &mut Phone, secret: &str) -> Value {
        match e["type"].as_str().unwrap() {
            "random" => {
                let n = e["bytes"].as_u64().unwrap() as usize;
                let bytes: Vec<u8> = (0..n).map(|_| { p.counter = p.counter.wrapping_mul(31).wrapping_add(7); p.counter }).collect();
                json!({ "bytes": bytes.iter().map(|b| format!("{b:02x}")).collect::<String>() })
            }
            "deriveKey" => {
                // What every driver does: the core's own derivation over the secret the driver holds.
                let secret_hex: String = secret.bytes().map(|b| format!("{b:02x}")).collect();
                let k = ask(json!({ "op": "deriveBundleKey", "secret": secret_hex, "kdf": e["kdf"] }));
                assert_eq!(k["ok"], true, "{k}");
                p.last_key = k["key"].as_str().unwrap().to_string();
                json!({ "key": p.last_key })
            }
            "readKeys" => {
                let mut m = serde_json::Map::new();
                for k in e["keys"].as_array().unwrap() {
                    if let Some(v) = p.kv.get(k.as_str().unwrap()) {
                        m.insert(k.as_str().unwrap().into(), json!(v));
                    }
                }
                json!({ "values": m })
            }
            "writeKeys" => {
                for k in e["remove"].as_array().unwrap() {
                    p.kv.remove(k.as_str().unwrap());
                }
                for (k, v) in e["set"].as_object().unwrap() {
                    let text = if p.corrupt_on_write { "garbled".to_string() } else { v.as_str().unwrap().to_string() };
                    p.kv.insert(k.clone(), text);
                }
                json!({ "ok": true })
            }
            "readItems" => {
                let mut m = serde_json::Map::new();
                for n in e["names"].as_array().unwrap() {
                    if let Some(v) = p.items.get(n.as_str().unwrap()) {
                        m.insert(n.as_str().unwrap().into(), v.clone());
                    }
                }
                json!({ "items": m })
            }
            "applyItem" => {
                p.items.insert(e["name"].as_str().unwrap().into(), e["value"].clone());
                json!({ "ok": true })
            }
            "getBundle" => json!({ "bundle": route.bundle }),
            "putBundle" => { route.bundle = e["bundle"].as_str().map(String::from); json!({ "ok": true }) }
            "readClaim" => json!({ "claim": route.claim }),
            "createClaim" => {
                let created = route.claim.is_none();
                if created { route.claim = Some(e["claim"].clone()); }
                json!({ "created": created })
            }
            "writeConfirmation" => {
                if route.drop_confirmations > 0 { route.drop_confirmations -= 1; } else { route.confirmation = Some(e["confirmation"].clone()); }
                json!({ "ok": true })
            }
            "readConfirmation" => json!({ "confirmation": route.confirmation }),
            "loadClearProgress" => json!({ "done": p.progress }),
            "runClearStep" => {
                let step = e["step"].as_str().unwrap().to_string();
                if p.fail_on.as_deref() == Some(step.as_str()) { return json!({ "ok": false }); }
                if step == "clearAppHeldState" {
                    p.clear_keys = e["keys"].as_array().unwrap().iter().map(|k| k.as_str().unwrap().to_string()).collect();
                    for k in &p.clear_keys { p.kv.remove(k); }
                }
                p.ran.push(step);
                json!({ "ok": true })
            }
            "markClearStep" => { p.progress.push(e["step"].as_str().unwrap().into()); json!({ "ok": true }) }
            other => panic!("unknown effect {other}"),
        }
    }

    /// Drive a flow to its end, and check on every turn that no state holds the secret or the key (SWR-MG-02).
    fn run(flow: &str, params: Value, route: &mut Route, p: &mut Phone, secret: &str) -> Value {
        let mut a = ask(json!({ "op": "flow", "flow": flow, "params": params }));
        for _ in 0..400 {
            if let Some(d) = a.get("done") {
                return d.clone();
            }
            let state = a["state"].clone();
            let text = state.to_string();
            assert!(!text.contains(secret), "the secret is in a flow state");
            assert!(p.last_key.is_empty() || !text.contains(&p.last_key), "the key is in a flow state");
            let result = perform(&a["effect"], route, p, secret);
            a = ask(json!({ "op": "flow", "flow": flow, "state": state, "result": result }));
        }
        panic!("a flow did not end");
    }

    fn fill(p: &mut Phone) {
        for item in KEYED {
            for k in keys_of(item) {
                p.kv.insert(k.clone(), format!("value-of-{k}"));
            }
        }
    }

    fn params(p: &Phone, extra: Value) -> Value {
        let mut v = json!({ "route": ATOMIC(), "hostItems": p.adapter, "keyItems": p.key_items });
        for (k, e) in extra.as_object().into_iter().flatten() {
            v[k] = e.clone();
        }
        v
    }

    fn backup(kind: &str, route: &mut Route, p: &mut Phone, route_json: Value) -> Value {
        let params = params(p, json!({ "kind": kind, "method": "passphrase", "route": route_json }));
        run("backup", params, route, p, SECRET)
    }

    fn restore(route: &mut Route, p: &mut Phone, secret: &str) -> Value {
        let params = params(p, json!({}));
        run("restore", params, route, p, secret)
    }

    // @verifies SWR-MG-02
    // @verifies SWR-MG-08
    #[test]
    fn a_migration_moves_every_carried_item_and_the_route_holds_ciphertext_a_claim_and_a_confirmation() {
        let (mut route, mut old, mut fresh) = (Route::default(), phone(&ADAPTER, 1), phone(&ADAPTER, 50));
        fill(&mut old);
        let done = backup("migrate", &mut route, &mut old, ATOMIC());
        assert_eq!(done["ok"], true, "{done}");
        let bytes = route.bundle.clone().unwrap();
        assert!(!bytes.contains(&"value-of-np.session.history".bytes().map(|b| format!("{b:02x}")).collect::<String>()), "plaintext on the route");

        let r = restore(&mut route, &mut fresh, SECRET);
        assert_eq!(r["ok"], true, "{r}");
        assert_eq!(fresh.kv, old.kv);
        assert_eq!(r["confirmed"], true);
        assert_eq!(route.confirmation, Some(json!({ "bundleHash": done["bundleHash"] })), "the confirmation carries the hash and nothing else");
        let claim = route.claim.clone().unwrap();
        assert_eq!(claim.as_object().unwrap().keys().map(String::as_str).collect::<Vec<_>>(), ["bundleHash", "token"]);
    }

    // @verifies SWR-MG-01
    #[test]
    fn only_the_keys_of_a_carried_item_move_and_per_install_keys_never_do() {
        let (mut route, mut old, mut fresh) = (Route::default(), phone(&ADAPTER, 1), phone(&ADAPTER, 50));
        fill(&mut old);
        for k in ["np.research.device-token", "np.warranty.consent-granted", "ble.lastHubId", "np.consumable.snooze-counts"] {
            old.kv.insert(k.into(), "secret-to-this-phone".into());
        }
        backup("migrate", &mut route, &mut old, ATOMIC());
        assert_eq!(restore(&mut route, &mut fresh, SECRET)["ok"], true);
        for k in ["np.research.device-token", "np.warranty.consent-granted", "ble.lastHubId", "np.consumable.snooze-counts"] {
            assert!(!fresh.kv.contains_key(k), "{k} must not move");
        }
        assert_eq!(fresh.kv.len(), old.kv.len() - 4);
    }

    // @verifies SWR-MG-03
    // @verifies SWR-MG-07
    #[test]
    fn the_nightly_backup_leaves_out_protocols_and_limits_and_is_never_claimed() {
        let (mut route, mut old, mut fresh) = (Route::default(), phone(&ADAPTER, 1), phone(&ADAPTER, 50));
        fill(&mut old);
        let rj = json!({ "id": "usb", "atomicClaim": false, "direct": false, "authenticatedPairing": true });
        let done = backup("uhdr", &mut route, &mut old, rj);
        assert_eq!(done["ok"], true, "{done}");
        let written: Vec<&str> = done["written"].as_array().unwrap().iter().map(|v| v.as_str().unwrap()).collect();
        assert_eq!(written, ["characterisationEnrolment", "sessionHistory"]);
        assert!(done["omitted"].as_array().unwrap().iter().any(|v| v == "protocols"));

        let r = restore(&mut route, &mut fresh, SECRET);
        assert_eq!(r["ok"], true, "{r}");
        assert!(!fresh.kv.contains_key("np.user-protocols.npps") && fresh.kv.contains_key("np.session.history"));
        assert!(route.claim.is_none(), "a UHDR-only bundle is not claimed");
        assert_eq!((r["confirmed"].clone(), r["askConsent"].clone(), r["tellConsentNotRestored"].clone()), (json!(false), json!(true), json!(true)));
        assert!(route.confirmation.is_none());
    }

    // @verifies SWR-MG-10
    #[test]
    fn a_route_without_an_atomic_claim_cannot_carry_a_migration_and_nothing_is_written() {
        let (mut route, mut old) = (Route::default(), phone(&ADAPTER, 1));
        fill(&mut old);
        let r = backup("migrate", &mut route, &mut old, json!({ "id": "usb", "atomicClaim": false }));
        assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("route_not_offered")));
        assert!(route.bundle.is_none());
    }

    // @verifies SWR-MG-07
    #[test]
    fn a_wrong_secret_opens_nothing_and_does_not_take_the_claim() {
        let (mut route, mut old, mut fresh) = (Route::default(), phone(&ADAPTER, 1), phone(&ADAPTER, 50));
        fill(&mut old);
        backup("migrate", &mut route, &mut old, ATOMIC());
        let r = restore(&mut route, &mut fresh, "a different passphrase");
        assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("authentication_failed")));
        assert!(route.claim.is_none(), "a typing slip must not leave a claim only the person can release");
        assert!(fresh.kv.is_empty());
    }

    // @verifies SWR-MG-07
    #[test]
    fn a_second_phone_cannot_restore_a_claimed_bundle_until_the_person_releases_it() {
        let (mut route, mut old, mut first, mut second) = (Route::default(), phone(&ADAPTER, 1), phone(&ADAPTER, 50), phone(&ADAPTER, 90));
        fill(&mut old);
        backup("migrate", &mut route, &mut old, ATOMIC());
        assert_eq!(restore(&mut route, &mut first, SECRET)["ok"], true);
        let r = restore(&mut route, &mut second, SECRET);
        assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("claimed")));
        assert!(second.kv.is_empty());
        route.claim = None; // the person's explicit release
        assert_eq!(restore(&mut route, &mut second, SECRET)["ok"], true);
    }

    // @verifies SWR-MG-08
    #[test]
    fn a_confirmation_that_does_not_land_is_written_again_and_an_unconfirmed_restore_says_so() {
        let (mut route, mut old) = (Route::default(), phone(&ADAPTER, 1));
        fill(&mut old);
        backup("migrate", &mut route, &mut old, ATOMIC());
        route.drop_confirmations = 2;
        let r = restore(&mut route, &mut phone(&ADAPTER, 50), SECRET);
        assert_eq!(r["confirmed"], true, "the third write lands");

        let (mut route2, mut old2) = (Route::default(), phone(&ADAPTER, 1));
        fill(&mut old2);
        backup("migrate", &mut route2, &mut old2, ATOMIC());
        route2.drop_confirmations = 3;
        let r2 = restore(&mut route2, &mut phone(&ADAPTER, 50), SECRET);
        assert_eq!((r2["ok"].clone(), r2["confirmed"].clone()), (json!(true), json!(false)));
        assert!(route2.confirmation.is_none(), "no confirmation, so the old phone clears nothing");
    }

    // @verifies SWR-MG-08
    #[test]
    fn a_key_that_does_not_read_back_as_written_stops_the_restore_before_any_confirmation() {
        let (mut route, mut old) = (Route::default(), phone(&ADAPTER, 1));
        fill(&mut old);
        backup("migrate", &mut route, &mut old, ATOMIC());
        let mut fresh = phone(&ADAPTER, 50);
        fresh.corrupt_on_write = true;
        let r = restore(&mut route, &mut fresh, SECRET);
        assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("verify_failed")));
        assert!(route.confirmation.is_none());
    }

    // @verifies SWR-MG-01
    #[test]
    fn a_bundle_item_that_names_a_key_it_does_not_own_is_refused_before_any_claim_or_write() {
        // Seal a bundle by hand whose sessionHistory item carries another item's key: the core refuses it on restore.
        let mut route = Route::default();
        let salt = "5a".repeat(32);
        let kdf = ask(json!({ "op": "bundleKdf", "method": "passphrase", "salt": salt }))["kdf"].clone();
        let secret_hex: String = SECRET.bytes().map(|b| format!("{b:02x}")).collect();
        let key = ask(json!({ "op": "deriveBundleKey", "secret": secret_hex, "kdf": kdf }))["key"].clone();
        let items = json!({ "sessionHistory": { "np.session.history": "ok", "np.warranty.consent-granted": "true" } });
        let sealed = ask(json!({ "op": "sealBundle", "kind": "migrate", "key": key, "nonce": "11".repeat(12), "kdf": kdf, "items": items }));
        route.bundle = sealed["bundle"].as_str().map(String::from);
        let mut fresh = phone(&ADAPTER, 50);
        let r = restore(&mut route, &mut fresh, SECRET);
        assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("malformed")));
        assert!(route.claim.is_none() && fresh.kv.is_empty());
    }

    // @verifies SWR-MG-04
    #[test]
    fn a_restore_never_resets_a_withdrawal_and_an_item_with_nowhere_to_go_is_reported_not_applied() {
        let (mut route, mut old) = (Route::default(), phone(&ADAPTER, 1));
        fill(&mut old);
        old.items.insert("withdrawalState".into(), json!({ "blanketConsentWithdrawnAt": "2026-03-01T00:00:00Z", "studyWithdrawals": ["study-a"], "categoryWithdrawals": [] }));
        old.items.insert("consentGrants".into(), json!({ "grants": "g" }));
        backup("migrate", &mut route, &mut old, ATOMIC());

        // The new phone stores withdrawal state but has no consent-grants store, and already holds a LATER withdrawal.
        let mut fresh = phone(&["withdrawalState"], 50);
        fresh.items.insert("withdrawalState".into(), json!({ "blanketConsentWithdrawnAt": "2026-06-01T00:00:00Z", "studyWithdrawals": ["study-b"], "categoryWithdrawals": [] }));
        let r = restore(&mut route, &mut fresh, SECRET);
        assert_eq!(r["ok"], true, "{r}");
        let merged = &fresh.items["withdrawalState"];
        assert_eq!(merged["blanketConsentWithdrawnAt"], "2026-06-01T00:00:00Z");
        assert_eq!(merged["studyWithdrawals"], json!(["study-a", "study-b"]));
        assert_eq!(r["notApplied"], json!(["consentGrants"]));
        assert!(!fresh.items.contains_key("consentGrants"));
        assert_eq!((r["askConsent"].clone(), r["tellConsentNotRestored"].clone()), (json!(true), json!(true)));
    }

    // @verifies SWR-MG-05
    #[test]
    fn the_kind_and_how_the_key_was_made_are_read_before_any_secret_is_asked() {
        let (mut route, mut old) = (Route::default(), phone(&ADAPTER, 1));
        let none = run("inspect", json!({}), &mut route, &mut old, SECRET);
        assert_eq!(none["reason"], "no_bundle");
        fill(&mut old);
        backup("uhdr", &mut route, &mut old, ATOMIC());
        let found = run("inspect", json!({}), &mut route, &mut old, SECRET);
        assert_eq!((found["kind"].clone(), found["method"].clone(), found["formatVersion"].clone()), (json!("uhdr"), json!("passphrase"), json!(1)));
    }

    fn confirmed_route() -> (Route, Phone, String) {
        let (mut route, mut old) = (Route::default(), phone(&ADAPTER, 1));
        fill(&mut old);
        let hash = backup("migrate", &mut route, &mut old, ATOMIC())["bundleHash"].as_str().unwrap().to_string();
        (route, old, hash)
    }

    // @verifies SWR-MG-06
    // @verifies SWR-MG-08
    // @verifies SWR-MG-09
    #[test]
    fn the_old_phone_clears_only_after_the_matching_confirmation_key_first_and_pairing_included() {
        let (mut route, mut old, hash) = confirmed_route();
        let none = run("clear", json!({ "bundleHash": hash, "canRemoveOsBond": false }), &mut route, &mut old, SECRET);
        assert_eq!((none["ok"].clone(), none["reason"].clone()), (json!(false), json!("no_confirmation")));
        assert!(old.ran.is_empty(), "nothing is cleared without a confirmation");

        assert_eq!(restore(&mut route, &mut phone(&ADAPTER, 50), SECRET)["ok"], true);
        let other = run("clear", json!({ "bundleHash": "not-this-bundle", "canRemoveOsBond": false }), &mut route, &mut old, SECRET);
        assert_eq!(other["reason"], "other_bundle");
        assert!(old.ran.is_empty());

        let ok = run("clear", json!({ "bundleHash": hash, "canRemoveOsBond": false }), &mut route, &mut old, SECRET);
        assert_eq!((ok["ok"].clone(), ok["removeOsBond"].clone()), (json!(true), json!(true)));
        assert_eq!(old.ran[0], "destroyEncryptionKey", "the key goes first");
        assert!(old.ran.contains(&"forgetPairingRecord".to_string()) && old.ran.contains(&"forgetLastPairedHelmetId".to_string()));
        // The core names every stored key, the per-install ones included, so no app keeps a list of its own to forget.
        assert!(old.clear_keys.contains(&"ble.lastHubId".to_string()) && old.clear_keys.contains(&"np.session.history".to_string()));
        assert!(old.kv.is_empty(), "everything stored is forgotten");
    }

    // @verifies SWR-MG-08
    #[test]
    fn a_clear_that_stops_part_way_resumes_from_the_step_it_reached_without_asking_again() {
        let (mut route, mut old, hash) = confirmed_route();
        assert_eq!(restore(&mut route, &mut phone(&ADAPTER, 50), SECRET)["ok"], true);
        old.fail_on = Some("forgetPairingRecord".into());
        let r1 = run("clear", json!({ "bundleHash": hash, "canRemoveOsBond": true }), &mut route, &mut old, SECRET);
        assert_eq!((r1["ok"].clone(), r1["step"].clone()), (json!(false), json!("forgetPairingRecord")));
        assert_eq!(old.ran, ["destroyEncryptionKey", "overwriteAndDeleteFiles"]);

        // Next launch: the route no longer answers, but the clear is already under way.
        old.fail_on = None;
        old.ran.clear();
        let mut gone = Route::default();
        let r2 = run("clear", json!({ "bundleHash": hash, "canRemoveOsBond": true }), &mut gone, &mut old, SECRET);
        assert_eq!((r2["ok"].clone(), r2["removeOsBond"].clone()), (json!(true), json!(false)));
        assert_eq!(old.ran[0], "forgetPairingRecord");
        assert!(!old.ran.contains(&"destroyEncryptionKey".to_string()));
    }

    // @verifies SWR-MG-01
    #[test]
    fn the_key_map_names_each_key_once_and_every_carried_item_is_one_the_core_knows() {
        let m = ask(json!({ "op": "itemKeys" }));
        let mut all: Vec<String> = vec![];
        for (_, ks) in m["carried"].as_object().unwrap() {
            all.extend(ks.as_array().unwrap().iter().map(|k| k.as_str().unwrap().to_string()));
        }
        for list in ["consent", "perInstall", "unsorted"] {
            all.extend(m[list].as_array().unwrap().iter().map(|k| k.as_str().unwrap().to_string()));
        }
        let mut sorted = all.clone();
        sorted.sort();
        sorted.dedup();
        assert_eq!(sorted.len(), all.len(), "a key in two lists would move and not move at once");
        let carried = ask(json!({ "op": "inventory", "kind": "migrate" }))["carried"].clone();
        for item in m["carried"].as_object().unwrap().keys() {
            assert!(carried.as_array().unwrap().iter().any(|c| c == item.as_str()), "{item} is not an item of the migrate kind");
        }
    }

    // @verifies SWR-MG-02
    #[test]
    fn an_unknown_flow_or_step_is_refused() {
        assert!(migration_json(&json!({ "op": "flow", "flow": "teleport" }).to_string()).is_err());
        assert!(migration_json(&json!({ "op": "flow", "flow": "backup", "state": { "step": "nope" }, "result": {} }).to_string()).is_err());
    }
}
