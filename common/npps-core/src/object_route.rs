//! A route over a plain object store, and the check of one (`SWR-MG-10`, `SWR-MG-11`, `SWR-MG-12`; issue #569 package 6h,
//! `CLAUDE.md` §20).
//!
//! Every cloud provider, the local computer and a shared folder come down to the same few calls on named objects. If each
//! app wrote its own route (which object holds the bundle, how a claim is encoded, how a missing object reads) five apps
//! would hold five copies of the rules that keep one bundle off two phones. So the route is here, once, and a provider
//! implements only the object calls of `SWR-MG-11`. This is the whole of what an app writes for a route:
//!
//! | type | args | result |
//! |------|------|--------|
//! | `objectGet` | `name` | `{data: hex or null}`, null when no such object |
//! | `objectPut` | `name`, `data` (hex) | `{ok}`: write, replacing any earlier object |
//! | `objectCreate` | `name`, `data` (hex) | `{created}`: write if and only if no such object exists, arbitrated by the store itself (`SWR-MG-07`) |
//! | `objectDelete` | `name` | `{ok}`, true also when there was no such object |
//! | `objectList` | `prefix` | `{names: [text]}` |
//! | `objectSpace` | none | `{bytes}`: free space, or null when the provider cannot say |
//!
//! **Any of these may instead return `{failed: true}`** (no network, no permission, a refused write): the flows then end
//! `{ok: false, reason: "route_unavailable"}` and the probe counts the operation as failed. A host never throws.
//!
//! # Two uses
//! 1. **The backup, inspect and restore flows over an object store.** Start a flow with `objectStore: true` in its params
//!    (and the route's descriptor, `id` and `atomicClaim`, as before). The flow's route effects (`getBundle`, `putBundle`,
//!    `readClaim`, `createClaim`, `writeConfirmation`, `readConfirmation`) then arrive as the object calls above, with the
//!    claim and the confirmation as JSON text in hex. A host answers no route effect of its own.
//! 2. **The probe flow** (`SWR-MG-12`), `flow: "probe"` with `bundleBytes`, runs the check on throwaway objects that hold
//!    no user data and ends `{result, failedOperation?, ops}` (`probeResult` classifies it). It always runs every
//!    operation and always deletes both throwaway objects at the end, so it leaves nothing behind, including when an
//!    operation fails part-way. It is a capability check and does not prove a claim safe under two simultaneous writers
//!    (that is the provider's contract test, `SWR-MG-11`).
//!
//! The object names are the core's (`objectNames`), so a host that releases a claim on the person's explicit act
//! (`SWR-MG-07`) deletes the claim object by the name the core gives.

use crate::migration::{dispatch, hex, unhex};
use serde_json::{json, Value};

type R<T> = Result<T, String>;

pub(crate) const BUNDLE_OBJECT: &str = "np-migration-bundle.npbk";
pub(crate) const CLAIM_OBJECT: &str = "np-migration-claim.json";
pub(crate) const CONFIRMATION_OBJECT: &str = "np-migration-confirmation.json";
pub(crate) const PROBE_PREFIX: &str = "np-probe-";
const PROBE_BYTES: &str = "probe";

pub(crate) fn names() -> Value {
    json!({ "bundle": BUNDLE_OBJECT, "claim": CLAIM_OBJECT, "confirmation": CONFIRMATION_OBJECT, "probePrefix": PROBE_PREFIX })
}

fn failed(result: &Value) -> bool {
    result["failed"] == true
}

/// A flow, over the route the host's params name. `inner` is the flow itself; this layer only renames its route effects.
pub(crate) fn flow(req: &Value, inner: fn(&Value) -> R<Value>) -> R<Value> {
    if req["flow"] == "probe" {
        return probe(req);
    }
    let started = req.get("state").is_none_or(Value::is_null);
    if started && req["params"]["objectStore"] != true {
        return inner(req);
    }
    if !started && req["state"]["objRoute"] != true {
        return inner(req);
    }
    let mut next = req.clone();
    if !started {
        let pending = req["state"]["pending"].as_str().unwrap_or("");
        let result = &req["result"];
        if !pending.is_empty() && failed(result) {
            return Ok(json!({ "done": { "ok": false, "reason": "route_unavailable" } }));
        }
        next["state"] = req["state"]["inner"].clone();
        next["result"] = match from_object(pending, result) {
            Ok(r) => r,
            Err(_) => return Ok(json!({ "done": { "ok": false, "reason": "route_unavailable" } })),
        };
    }
    let answer = inner(&next)?;
    let Some(e) = answer.get("effect") else { return Ok(answer) };
    let (translated, pending) = to_object(e);
    Ok(json!({ "effect": translated, "state": { "objRoute": true, "inner": answer["state"], "pending": pending } }))
}

/// The object call that stands for a route effect, and the name of the effect it stands for (empty for any other).
fn to_object(e: &Value) -> (Value, &'static str) {
    let text = |v: &Value| hex(v.to_string().as_bytes());
    match e["type"].as_str().unwrap_or("") {
        "getBundle" => (json!({ "type": "objectGet", "name": BUNDLE_OBJECT }), "getBundle"),
        "putBundle" => (json!({ "type": "objectPut", "name": BUNDLE_OBJECT, "data": e["bundle"] }), "putBundle"),
        "readClaim" => (json!({ "type": "objectGet", "name": CLAIM_OBJECT }), "readClaim"),
        "createClaim" => (json!({ "type": "objectCreate", "name": CLAIM_OBJECT, "data": text(&e["claim"]) }), "createClaim"),
        "writeConfirmation" => (json!({ "type": "objectPut", "name": CONFIRMATION_OBJECT, "data": text(&e["confirmation"]) }), "writeConfirmation"),
        "readConfirmation" => (json!({ "type": "objectGet", "name": CONFIRMATION_OBJECT }), "readConfirmation"),
        _ => (e.clone(), ""),
    }
}

/// What the flow expects back from the route effect `pending` stood for, from what the object call returned.
fn from_object(pending: &str, r: &Value) -> R<Value> {
    let json_of = |r: &Value| -> R<Value> {
        match r["data"].as_str() {
            None => Ok(Value::Null),
            Some(h) => serde_json::from_slice(&unhex(h, "object")?).map_err(|e| e.to_string()),
        }
    };
    Ok(match pending {
        "getBundle" => json!({ "bundle": r["data"] }),
        "putBundle" | "writeConfirmation" => {
            if r["ok"] != true {
                return Err("write refused".into());
            }
            json!({ "ok": true })
        }
        "readClaim" => json!({ "claim": json_of(r)? }),
        "readConfirmation" => json!({ "confirmation": json_of(r)? }),
        "createClaim" => json!({ "created": r["created"] == true }),
        _ => r.clone(),
    })
}

// ─── the probe (`SWR-MG-12`) ───────────────────────────────────────────────────────────────────────────────────────────

fn probe(req: &Value) -> R<Value> {
    let mut st = req["state"].clone();
    let r = &req["result"];
    let step = st["step"].as_str().unwrap_or("start").to_string();
    let call = |st: &mut Value, next: &str, e: Value| -> R<Value> {
        st["step"] = json!(next);
        Ok(json!({ "effect": e, "state": st.clone() }))
    };
    let set = |st: &mut Value, op: &str, v: bool| st["ops"][op] = json!(v);
    let (n, c) = (st["name"].as_str().unwrap_or("").to_string(), st["claim"].as_str().unwrap_or("").to_string());
    match step.as_str() {
        "start" => {
            let bytes = req["params"]["bundleBytes"].as_u64().ok_or("'bundleBytes' is required")?;
            let st = json!({ "step": "named", "bundleBytes": bytes, "ops": {} });
            Ok(json!({ "effect": { "type": "random", "bytes": 8 }, "state": st }))
        }
        "named" => {
            let name = format!("{PROBE_PREFIX}{}", r["bytes"].as_str().ok_or("random bytes are required")?);
            st["name"] = json!(name);
            st["claim"] = json!(format!("{name}-claim"));
            call(&mut st, "wrote", json!({ "type": "objectPut", "name": name, "data": hex(PROBE_BYTES.as_bytes()) }))
        }
        "wrote" => {
            set(&mut st, "write", !failed(r) && r["ok"] == true);
            call(&mut st, "read", json!({ "type": "objectGet", "name": n }))
        }
        "read" => {
            set(&mut st, "read", !failed(r) && r["data"].as_str() == Some(hex(PROBE_BYTES.as_bytes()).as_str()));
            call(&mut st, "listed", json!({ "type": "objectList", "prefix": PROBE_PREFIX }))
        }
        "listed" => {
            let found = r["names"].as_array().is_some_and(|a| a.iter().any(|x| x == n.as_str()));
            set(&mut st, "list", !failed(r) && found);
            call(&mut st, "created1", json!({ "type": "objectCreate", "name": c, "data": hex(PROBE_BYTES.as_bytes()) }))
        }
        "created1" => {
            set(&mut st, "createIfAbsentFirst", !failed(r) && r["created"] == true);
            call(&mut st, "created2", json!({ "type": "objectCreate", "name": c, "data": hex(PROBE_BYTES.as_bytes()) }))
        }
        "created2" => {
            // The second create must be REFUSED. A failure is not a refusal: only `created: false` proves the store arbitrates.
            set(&mut st, "createIfAbsentSecondRefused", !failed(r) && r["created"] == false);
            call(&mut st, "released", json!({ "type": "objectDelete", "name": c }))
        }
        "released" => {
            set(&mut st, "release", !failed(r) && r["ok"] == true);
            call(&mut st, "deleted", json!({ "type": "objectDelete", "name": n }))
        }
        "deleted" => {
            set(&mut st, "delete", !failed(r) && r["ok"] == true);
            call(&mut st, "spaced", json!({ "type": "objectSpace" }))
        }
        "spaced" => {
            let enough = r["bytes"].as_u64().is_some_and(|b| b >= st["bundleBytes"].as_u64().unwrap_or(u64::MAX));
            set(&mut st, "space", !failed(r) && enough);
            // Whatever failed above, both throwaway objects go: the probe leaves nothing behind.
            call(&mut st, "clean1", json!({ "type": "objectDelete", "name": n }))
        }
        "clean1" => call(&mut st, "clean2", json!({ "type": "objectDelete", "name": c })),
        "clean2" => {
            let mut out = dispatch("probeResult", &json!({ "ops": st["ops"] }))?;
            out["ops"] = st["ops"].clone();
            Ok(json!({ "done": out }))
        }
        other => Err(format!("unknown probe step '{other}'")),
    }
}

#[cfg(test)]
mod tests {
    use crate::migration::migration_json;
    use serde_json::{json, Value};
    use std::collections::BTreeMap;

    fn ask(v: Value) -> Value {
        serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
    }

    /// A provider as an app would implement it: named objects, a conditional create, and a way to break one call.
    #[derive(Default)]
    struct Provider {
        objects: BTreeMap<String, String>,
        /// Effect types that answer `failed`.
        failing: Vec<&'static str>,
        /// A store with no conditional write: its create always succeeds.
        no_conditional: bool,
        /// A store that reads back something other than what was written.
        corrupts: bool,
        space: Option<u64>,
        asked: Vec<String>,
    }

    impl Provider {
        fn perform(&mut self, e: &Value) -> Value {
            let t = e["type"].as_str().unwrap().to_string();
            self.asked.push(t.clone());
            if self.failing.contains(&t.as_str()) {
                return json!({ "failed": true });
            }
            let name = e["name"].as_str().unwrap_or("").to_string();
            match t.as_str() {
                "random" => json!({ "bytes": "0102030405060708" }),
                "objectPut" => { self.objects.insert(name, e["data"].as_str().unwrap().into()); json!({ "ok": true }) }
                "objectGet" => {
                    let d = self.objects.get(&name).cloned().map(|d| if self.corrupts { "00".to_string() } else { d });
                    json!({ "data": d })
                }
                "objectCreate" => {
                    let created = self.no_conditional || !self.objects.contains_key(&name);
                    if created { self.objects.insert(name, e["data"].as_str().unwrap().into()); }
                    json!({ "created": created })
                }
                "objectDelete" => { self.objects.remove(&name); json!({ "ok": true }) }
                "objectList" => {
                    let prefix = e["prefix"].as_str().unwrap();
                    json!({ "names": self.objects.keys().filter(|k| k.starts_with(prefix)).collect::<Vec<_>>() })
                }
                "objectSpace" => json!({ "bytes": self.space }),
                other => panic!("unknown effect {other}"),
            }
        }

        fn probe(&mut self, bundle_bytes: u64) -> Value {
            let mut a = ask(json!({ "op": "flow", "flow": "probe", "params": { "bundleBytes": bundle_bytes } }));
            for _ in 0..40 {
                if let Some(d) = a.get("done") {
                    return d.clone();
                }
                let result = self.perform(&a["effect"]);
                a = ask(json!({ "op": "flow", "flow": "probe", "state": a["state"], "result": result }));
            }
            panic!("the probe did not end");
        }
    }

    fn good() -> Provider {
        Provider { space: Some(1 << 30), ..Default::default() }
    }

    // @verifies SWR-MG-12
    #[test]
    fn a_provider_that_can_do_everything_is_sufficient_and_the_probe_leaves_nothing_behind() {
        let mut p = good();
        p.objects.insert("someone-elses-object".into(), "00".into());
        let r = p.probe(1_000_000);
        assert_eq!(r["result"], "sufficient");
        assert_eq!(p.objects.keys().collect::<Vec<_>>(), ["someone-elses-object"], "only the throwaway objects are touched, and they are gone");
    }

    // @verifies SWR-MG-12
    #[test]
    fn a_store_whose_second_create_succeeds_is_backup_only_because_it_cannot_arbitrate_a_claim() {
        let mut p = Provider { no_conditional: true, ..good() };
        let r = p.probe(1_000);
        assert_eq!((r["result"].clone(), r["failedOperation"].clone()), (json!("backup_only"), json!("createIfAbsentSecondRefused")));
        assert!(p.objects.is_empty());
    }

    // @verifies SWR-MG-12
    #[test]
    fn each_failed_operation_is_named_and_the_probe_still_cleans_up() {
        for (failing, result, op) in [
            ("objectPut", "insufficient", "write"),
            ("objectList", "insufficient", "list"),
            ("objectSpace", "insufficient", "space"),
            ("objectCreate", "backup_only", "createIfAbsentFirst"),
        ] {
            let mut p = Provider { failing: vec![failing], ..good() };
            let r = p.probe(1_000);
            assert_eq!((r["result"].clone(), r["failedOperation"].clone()), (json!(result), json!(op)), "{failing}");
            assert!(p.objects.is_empty(), "{failing} left an object behind");
            assert!(p.asked.iter().filter(|t| *t == "objectDelete").count() >= 2, "{failing}: cleanup always runs");
        }
        // A delete that fails is a failed delete, and the probe says so.
        let mut p = Provider { failing: vec!["objectDelete"], ..good() };
        assert_eq!(p.probe(1_000)["failedOperation"], "delete");
    }

    // @verifies SWR-MG-12
    #[test]
    fn a_store_that_reads_back_something_else_or_has_too_little_room_is_not_sufficient() {
        assert_eq!(Provider { corrupts: true, ..good() }.probe(1_000)["failedOperation"], "read");
        assert_eq!(Provider { space: Some(999), ..good() }.probe(1_000)["failedOperation"], "space");
        assert_eq!(Provider { space: None, ..good() }.probe(1_000)["failedOperation"], "space", "a provider that cannot say is not assumed to have room");
    }

    // @verifies SWR-MG-12
    #[test]
    fn the_probe_needs_to_know_how_big_a_bundle_it_must_hold() {
        assert!(migration_json(&json!({ "op": "flow", "flow": "probe", "params": {} }).to_string()).is_err());
    }

    // @verifies SWR-MG-07
    #[test]
    fn the_core_names_the_objects_so_no_host_chooses_where_a_claim_lives() {
        let n = ask(json!({ "op": "objectNames" }));
        let all = [&n["bundle"], &n["claim"], &n["confirmation"]];
        assert!(all.iter().all(|v| v.as_str().is_some_and(|s| s.starts_with("np-migration-"))));
        assert_ne!(n["claim"], n["confirmation"]);
        assert!(!n["probePrefix"].as_str().unwrap().starts_with("np-migration-"), "a probe can never touch a real object");
    }
}
