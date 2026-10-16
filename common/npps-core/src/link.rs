//! A direct link between two phones, as bytes (`SWR-MG-10`; issue #569 package 6l, `CLAUDE.md` §20).
//!
//! A direct phone-to-phone migration is the same six object calls as any other route (`object_route`), carried over a pipe
//! instead of a folder or a cloud account. The pipe may be Bluetooth, a local Wi-Fi socket or a cable, and each has its own
//! code on each platform. What is the same on all of them is how a call becomes bytes, how a stream of bytes becomes calls
//! again, which calls a peer may make, and what the serving phone does with them. That is here, once, so an app writes only
//! the pipe (the `Link` role: send some bytes, receive some bytes).
//!
//! # Wire
//! A message is a JSON object, framed by a 4-byte big-endian length. A request is `{id, call}` where `call` is one of the
//! `object*` effects of `object_route`; a response is `{id, result}` with that effect's result (or `{failed: true}`). A frame
//! may arrive in any number of chunks and several may arrive in one; the receiver feeds chunks to `linkFeed` and gets whole
//! messages back.
//!
//! # The two ends
//! * **The phone that restores** (the client) starts its flow with `link: true`, `objectStore: true` and `pairing: {code}`,
//!   the code the person typed. It pairs first; then its `object*` effects are turned by this layer into `linkSend` and
//!   `linkRecv`, so the flow runs unchanged over the link and a driver answers only those two, the same on every platform.
//! * **The phone that holds the bundle** (the server) runs `flow: "serve"` with `pairing: {code, expiresAt, triesLeft}`. It
//!   receives requests, asks its driver to apply each to its own object store, and sends the responses. It serves nothing
//!   until the other phone has proved it knows the code (`SWR-MG-10`: a bystander's phone must not be served), and it answers
//!   only the migration objects (`np-migration-*`) and the probe's (`np-probe-*`): a paired peer cannot read or write
//!   anything else the phone stores. The server arbitrates the claim, because it alone applies `objectCreate`.
//!
//! # The pairing (`SWR-MG-10`, ruled in issue #569)
//! The phone that holds the bundle makes an 8-digit code (`pairingCode`) and shows it; the person types it on the new phone,
//! so **the code decides the roles**. The code is low-entropy, so it is never sent or hashed in the open: the two phones run
//! SPAKE2 (RFC 9382, the `spake2` crate) with the code as the password. A wrong guess then gives an attacker one guess per
//! session and nothing to test offline, and a session counts against the code whether or not it succeeds: **3 sessions, 2
//! minutes** (`pairingPolicy`), after which the code is spent. Each phone proves it holds the shared key with a MAC over the
//! two SPAKE2 messages; the restoring phone checks the server's first, so a wrong code is found on the phone the person is
//! holding. Everything after the pairing is sealed with AES-256-GCM under keys derived from the shared key, one per direction,
//! with the message count as the nonce, so a phone on the same network can neither read, alter, replay nor reorder it.
//!
//! The server's driver answers `now` (`{seconds}`, Unix time) for the expiry, as well as `random`; the host keeps the code,
//! its expiry and the tries left across connections and passes them to each `serve`, whose result says what is left.
//!
//! | type | args | result |
//! |------|------|--------|
//! | `now` | none | `{seconds}` the platform clock, for the code's expiry |
//!
//! | `linkSend` | `frame` (hex) | `{ok}`: the bytes are on their way |
//! | `linkRecv` | none | `{chunk: hex}` the next bytes, or `{chunk: null}` when the link is closed; `{failed: true}` if it broke |

use crate::migration::{hex, unhex};
use aes_gcm::aead::{Aead, KeyInit, Payload};
use aes_gcm::{Aes256Gcm, Nonce};
use hkdf::Hkdf;
use hmac::{Hmac, Mac};
use rand_core::{CryptoRng, RngCore};
use sha2::Sha256;
use spake2::{Ed25519Group, Identity, Password, Spake2};
use crate::object_route::{BUNDLE_OBJECT, CLAIM_OBJECT, CONFIRMATION_OBJECT, PROBE_PREFIX};
use serde_json::{json, Value};

type R<T> = Result<T, String>;

/// The largest frame a peer may send (`UC-090`): a bundle is sent hex-encoded in one frame, so this bounds the bundle a link
/// can carry, and stops a peer from making the receiver buffer without end.
pub(crate) const MAX_FRAME: usize = 32 * 1024 * 1024;

const OBJECT_CALLS: [&str; 6] = ["objectGet", "objectPut", "objectCreate", "objectDelete", "objectList", "objectSpace"];
const MIGRATION_PREFIX: &str = "np-migration-";

/// The migration's own objects and the probe's, and nothing else a phone stores (`SWR-MG-10`).
fn allowed(call: &Value) -> bool {
    let kind = call["type"].as_str().unwrap_or("");
    if !OBJECT_CALLS.contains(&kind) {
        return false;
    }
    match kind {
        "objectSpace" => true,
        "objectList" => call["prefix"].as_str().is_some_and(|p| p.starts_with(MIGRATION_PREFIX) || p.starts_with(PROBE_PREFIX)),
        _ => call["name"].as_str().is_some_and(|n| {
            n == BUNDLE_OBJECT || n == CLAIM_OBJECT || n == CONFIRMATION_OBJECT || n.starts_with(PROBE_PREFIX)
        }),
    }
}

// ─── the pairing code ──────────────────────────────────────────────────────────────────────────────────────────────────

/// The code's length, its life and the sessions it allows (`UC-092`, ruled in issue #569): the app reads these from here.
pub(crate) const CODE_DIGITS: usize = 8;
pub(crate) const CODE_LIFETIME_SECONDS: u64 = 120;
pub(crate) const CODE_TRIES: u64 = 3;
/// The random bytes the code is made from, and for one SPAKE2 session. Digits are taken by rejection (a byte under 250),
/// so 32 bytes make 8 digits unless more than 24 bytes are rejected, which is not a case worth a retry.
const CODE_RANDOM: u64 = 32;
const PAIR_RANDOM: u64 = 96;
const SERVER_ID: &[u8] = b"np-migration-old";
const CLIENT_ID: &[u8] = b"np-migration-new";
const AAD: &[u8] = b"np-link-v1";

/// The digits of what was typed, in any grouping; exactly `CODE_DIGITS` of them or nothing.
fn parse_code(text: &str) -> R<String> {
    let digits: String = text.chars().filter(|c| !c.is_whitespace() && *c != '-').collect();
    if digits.len() == CODE_DIGITS && digits.chars().all(|c| c.is_ascii_digit()) {
        Ok(digits)
    } else {
        Err("malformed".into())
    }
}

fn make_code(random: &[u8]) -> R<String> {
    let digits: String = random.iter().filter(|b| **b < 250).take(CODE_DIGITS).map(|b| char::from(b'0' + b % 10)).collect();
    if digits.len() == CODE_DIGITS {
        Ok(digits)
    } else {
        Err("not enough random bytes for a code".into())
    }
}

/// The pure operations: the code's policy, a new code, and reading one a person typed.
pub(crate) fn op(name: &str, req: &Value) -> Option<R<Value>> {
    Some(match name {
        "pairingPolicy" => Ok(json!({ "digits": CODE_DIGITS, "lifetimeSeconds": CODE_LIFETIME_SECONDS, "tries": CODE_TRIES, "randomBytes": CODE_RANDOM })),
        "pairingCode" => unhex(req["bytes"].as_str().unwrap_or(""), "bytes").and_then(|b| make_code(&b)).map(|code| json!({ "code": code })),
        "pairingParse" => Ok(match parse_code(req["text"].as_str().unwrap_or("")) {
            Ok(code) => json!({ "ok": true, "code": code }),
            Err(reason) => json!({ "ok": false, "reason": reason }),
        }),
        _ => return None,
    })
}

// ─── SPAKE2 and the sealed channel ─────────────────────────────────────────────────────────────────────────────────────

/// The bytes the driver gave, handed to SPAKE2 as its randomness, so a session can be re-made from the state the flow keeps
/// (the flow is sans-IO: it cannot keep a `Spake2` between calls, and the driver owns the random, `UC-089`).
struct Given(Vec<u8>, usize);

impl RngCore for Given {
    fn next_u32(&mut self) -> u32 {
        let mut b = [0u8; 4];
        self.fill_bytes(&mut b);
        u32::from_le_bytes(b)
    }
    fn next_u64(&mut self) -> u64 {
        let mut b = [0u8; 8];
        self.fill_bytes(&mut b);
        u64::from_le_bytes(b)
    }
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        for d in dest {
            *d = self.0[self.1 % self.0.len()];
            self.1 += 1;
        }
    }
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), rand_core::Error> {
        self.fill_bytes(dest);
        Ok(())
    }
}

impl CryptoRng for Given {}

/// One side of SPAKE2: its message, and a finish that makes the shared key from the other side's.
fn spake(server: bool, code: &str, seed: &[u8]) -> (Spake2<Ed25519Group>, Vec<u8>) {
    let (pw, a, b) = (Password::new(code.as_bytes()), Identity::new(SERVER_ID), Identity::new(CLIENT_ID));
    if server {
        Spake2::<Ed25519Group>::start_a_with_rng(&pw, &a, &b, Given(seed.to_vec(), 0))
    } else {
        Spake2::<Ed25519Group>::start_b_with_rng(&pw, &a, &b, Given(seed.to_vec(), 0))
    }
}

fn mac(key: &[u8], label: &str, a: &[u8], b: &[u8]) -> Hmac<Sha256> {
    let mut m = <Hmac<Sha256> as Mac>::new_from_slice(key).expect("HMAC takes a key of any length");
    for part in [label.as_bytes(), a, b] {
        m.update(&(part.len() as u32).to_be_bytes());
        m.update(part);
    }
    m
}

fn tag(key: &[u8], label: &str, a: &[u8], b: &[u8]) -> String {
    hex(&mac(key, label, a, b).finalize().into_bytes())
}

fn tag_ok(key: &[u8], label: &str, a: &[u8], b: &[u8], given: &Value) -> bool {
    given.as_str().and_then(|t| unhex(t, "tag").ok()).is_some_and(|t| mac(key, label, a, b).verify_slice(&t).is_ok())
}

/// The two directions' keys: client to server, server to client.
fn channel_keys(shared: &[u8]) -> ([u8; 32], [u8; 32]) {
    let h = Hkdf::<Sha256>::new(None, shared);
    let (mut c2s, mut s2c) = ([0u8; 32], [0u8; 32]);
    h.expand(b"np-link-c2s", &mut c2s).expect("32 bytes is a valid HKDF length");
    h.expand(b"np-link-s2c", &mut s2c).expect("32 bytes is a valid HKDF length");
    (c2s, s2c)
}

fn nonce(counter: u64) -> [u8; 12] {
    let mut n = [0u8; 12];
    n[4..].copy_from_slice(&counter.to_be_bytes());
    n
}

/// The sealed channel one phone holds after pairing: a key and a message count for each direction.
fn ctx(send: [u8; 32], recv: [u8; 32]) -> Value {
    json!({ "sk": hex(&send), "rk": hex(&recv), "sn": 0, "rn": 0 })
}

fn seal(st: &mut Value, msg: &Value) -> R<Vec<u8>> {
    let body = msg.to_string().into_bytes();
    let c = &st["ctx"];
    if c.is_null() {
        return Ok(framed(&body));
    }
    let (key, n) = (unhex(c["sk"].as_str().unwrap_or(""), "key")?, c["sn"].as_u64().unwrap_or(0));
    let ct = Aes256Gcm::new_from_slice(&key)
        .map_err(|e| e.to_string())?
        .encrypt(Nonce::from_slice(&nonce(n)), Payload { msg: &body, aad: AAD })
        .map_err(|_| "seal failed".to_string())?;
    st["ctx"]["sn"] = json!(n + 1);
    Ok(framed(&ct))
}

fn unseal(st: &mut Value, body: &[u8]) -> R<Value> {
    let c = &st["ctx"];
    let plain = if c.is_null() {
        body.to_vec()
    } else {
        let (key, n) = (unhex(c["rk"].as_str().unwrap_or(""), "key")?, c["rn"].as_u64().unwrap_or(0));
        let p = Aes256Gcm::new_from_slice(&key)
            .map_err(|e| e.to_string())?
            .decrypt(Nonce::from_slice(&nonce(n)), Payload { msg: body, aad: AAD })
            .map_err(|_| "unseal failed".to_string())?;
        st["ctx"]["rn"] = json!(n + 1);
        p
    };
    serde_json::from_slice(&plain).map_err(|e| format!("malformed frame: {e}"))
}

fn framed(body: &[u8]) -> Vec<u8> {
    let mut out = (body.len() as u32).to_be_bytes().to_vec();
    out.extend_from_slice(body);
    out
}

/// Add `chunk` to the state's buffer and take whole frame bodies off the front: one, or all.
fn pull(st: &mut Value, chunk: &Value, all: bool) -> R<Vec<Vec<u8>>> {
    let mut buf = unhex(st["buf"].as_str().unwrap_or(""), "buffer")?;
    buf.extend(unhex(chunk.as_str().unwrap_or(""), "chunk")?);
    let mut out = vec![];
    while buf.len() >= 4 && (all || out.is_empty()) {
        let len = u32::from_be_bytes([buf[0], buf[1], buf[2], buf[3]]) as usize;
        if len > MAX_FRAME {
            return Err("frame too large".into());
        }
        if buf.len() < 4 + len {
            break;
        }
        out.push(buf[4..4 + len].to_vec());
        buf.drain(..4 + len);
    }
    st["buf"] = json!(hex(&buf));
    Ok(out)
}

/// The messages in a chunk once paired: every whole frame, opened.
fn messages(st: &mut Value, chunk: &Value) -> R<Vec<Value>> {
    pull(st, chunk, true)?.iter().map(|b| unseal(st, b)).collect()
}

fn effect(state: Value, e: Value) -> R<Value> {
    Ok(json!({ "effect": e, "state": state }))
}

fn send(mut st: Value, step: &str, msg: &Value) -> R<Value> {
    let f = seal(&mut st, msg)?;
    st["step"] = json!(step);
    effect(st, json!({ "type": "linkSend", "frame": hex(&f) }))
}

fn recv(mut st: Value, step: &str) -> R<Value> {
    st["step"] = json!(step);
    effect(st, json!({ "type": "linkRecv" }))
}

fn random_bytes(result: &Value) -> R<Vec<u8>> {
    let b = unhex(result["bytes"].as_str().unwrap_or(""), "bytes")?;
    if b.len() as u64 != PAIR_RANDOM {
        return Err("random gave the wrong number of bytes".into());
    }
    Ok(b)
}

fn done(v: Value) -> R<Value> {
    Ok(json!({ "done": v }))
}

fn refuse(reason: &str) -> R<Value> {
    done(json!({ "ok": false, "reason": reason }))
}

// ─── the restoring phone ───────────────────────────────────────────────────────────────────────────────────────────────

/// A flow over a link: `serve` on the phone that holds the bundle; on the other, `inner` run with its object calls carried
/// over the link when its params say `link: true`, once the pairing has succeeded.
pub(crate) fn flow(req: &Value, inner: fn(&Value) -> R<Value>) -> R<Value> {
    if req["flow"] == "serve" {
        return serve(req);
    }
    let started = req.get("state").is_none_or(Value::is_null);
    if started && req["params"]["link"] != true {
        return inner(req);
    }
    if !started && req["state"]["link"] != true {
        return inner(req);
    }
    if started {
        // A link with no pairing is not offered: nothing is exchanged with a phone that has not proved it holds the code.
        let Ok(code) = parse_code(req["params"]["pairing"]["code"].as_str().unwrap_or("")) else {
            return refuse("no_authenticated_pairing");
        };
        let st = json!({ "link": true, "buf": "", "id": 0, "phase": "pair_rand", "code": code, "origin": req });
        return effect(st, json!({ "type": "random", "bytes": PAIR_RANDOM }));
    }
    let mut st = req["state"].clone();
    let phase = st["phase"].as_str().unwrap_or("").to_string();
    if phase.starts_with("pair_") {
        return client_pair(req, st, &phase, inner);
    }
    let down = |st: &Value, result: Value| -> R<Value> {
        let next = json!({ "flow": req["flow"], "state": st["inner"], "result": result });
        let answer = inner(&next)?;
        intercept(answer, carry(st))
    };
    match phase.as_str() {
        // The driver answered an effect that is not a link call: hand it down.
        "call" => down(&st, req["result"].clone()),
        // The request left. Now wait for its answer.
        "sent" => {
            if req["result"]["failed"] == true || req["result"]["ok"] != true {
                return down(&st, json!({ "failed": true }));
            }
            st["phase"] = json!("recv");
            effect(st, json!({ "type": "linkRecv" }))
        }
        "recv" => {
            let chunk = &req["result"]["chunk"];
            if req["result"]["failed"] == true || chunk.is_null() {
                return down(&st, json!({ "failed": true }));
            }
            let Ok(msgs) = messages(&mut st, chunk) else { return down(&st, json!({ "failed": true })) };
            match msgs.iter().find(|m| m["id"] == st["id"]) {
                Some(m) => down(&st, m["result"].clone()),
                None => effect(st, json!({ "type": "linkRecv" })),
            }
        }
        other => Err(format!("unknown link phase '{other}'")),
    }
}

/// What a link keeps between the inner flow's calls: the buffer, the call count and the sealed channel.
fn carry(st: &Value) -> Value {
    json!({ "link": true, "buf": st["buf"], "id": st["id"], "ctx": st["ctx"] })
}

/// The restoring phone's half of the pairing, then the start of the inner flow.
fn client_pair(req: &Value, mut st: Value, phase: &str, inner: fn(&Value) -> R<Value>) -> R<Value> {
    let result = &req["result"];
    let step = |mut st: Value, phase: &str, e: Value| -> R<Value> {
        st["phase"] = json!(phase);
        effect(st, e)
    };
    let wrong = || refuse("wrong_code");
    match phase {
        "pair_rand" => {
            let seed = random_bytes(result)?;
            let (_, mine) = spake(false, st["code"].as_str().unwrap_or(""), &seed);
            st["seed"] = json!(hex(&seed));
            st["b"] = json!(hex(&mine));
            let f = seal(&mut st, &json!({ "pair": "hello", "msg": hex(&mine) }))?;
            step(st, "pair_hello_sent", json!({ "type": "linkSend", "frame": hex(&f) }))
        }
        "pair_hello_sent" => {
            if result["failed"] == true || result["ok"] != true {
                return refuse("link_failed");
            }
            step(st, "pair_reply", json!({ "type": "linkRecv" }))
        }
        "pair_reply" => {
            let chunk = &result["chunk"];
            if result["failed"] == true || chunk.is_null() {
                return refuse("link_failed");
            }
            let Ok(bodies) = pull(&mut st, chunk, false) else { return refuse("malformed_frame") };
            let Some(body) = bodies.first() else { return step(st, "pair_reply", json!({ "type": "linkRecv" })) };
            let Ok(m) = unseal(&mut st, body) else { return refuse("malformed_frame") };
            match m["pair"].as_str() {
                Some("refused") => refuse(match m["reason"].as_str() { Some("code_expired") => "code_expired", Some("code_spent") => "code_spent", _ => "wrong_code" }),
                Some("reply") => {
                    let (Ok(a), Ok(seed)) = (unhex(m["msg"].as_str().unwrap_or(""), "msg"), unhex(st["seed"].as_str().unwrap_or(""), "seed")) else { return refuse("malformed_frame") };
                    let (state, _) = spake(false, st["code"].as_str().unwrap_or(""), &seed);
                    let Ok(key) = state.finish(&a) else { return wrong() };
                    let b = unhex(st["b"].as_str().unwrap_or(""), "b")?;
                    // The server's proof first: a wrong code is found here, on the phone the person is holding.
                    if !tag_ok(&key, "server", &a, &b, &m["tag"]) {
                        return wrong();
                    }
                    let (c2s, s2c) = channel_keys(&key);
                    st["keys"] = json!({ "c2s": hex(&c2s), "s2c": hex(&s2c) });
                    let f = seal(&mut st, &json!({ "pair": "confirm", "tag": tag(&key, "client", &a, &b) }))?;
                    step(st, "pair_confirm_sent", json!({ "type": "linkSend", "frame": hex(&f) }))
                }
                _ => refuse("malformed_frame"),
            }
        }
        "pair_confirm_sent" => {
            if result["failed"] == true || result["ok"] != true {
                return refuse("link_failed");
            }
            step(st, "pair_ok", json!({ "type": "linkRecv" }))
        }
        "pair_ok" => {
            let chunk = &result["chunk"];
            if result["failed"] == true || chunk.is_null() {
                return refuse("link_failed");
            }
            let Ok(bodies) = pull(&mut st, chunk, false) else { return refuse("malformed_frame") };
            let Some(body) = bodies.first() else { return step(st, "pair_ok", json!({ "type": "linkRecv" })) };
            let Ok(m) = unseal(&mut st, body) else { return refuse("malformed_frame") };
            if m["pair"] != "ok" {
                return wrong();
            }
            let (c2s, s2c) = (unhex(st["keys"]["c2s"].as_str().unwrap_or(""), "key")?, unhex(st["keys"]["s2c"].as_str().unwrap_or(""), "key")?);
            let (mut a, mut b) = ([0u8; 32], [0u8; 32]);
            a.copy_from_slice(&c2s);
            b.copy_from_slice(&s2c);
            let mut next = json!({ "link": true, "buf": st["buf"], "id": 0, "ctx": ctx(a, b) });
            // The inner flow starts now, with its object calls on the sealed channel.
            let answer = inner(&st["origin"])?;
            next["inner"] = Value::Null;
            intercept(answer, next)
        }
        other => Err(format!("unknown pairing phase '{other}'")),
    }
}

/// The inner flow's next effect: an object call goes over the link; anything else is the driver's.
fn intercept(answer: Value, mut st: Value) -> R<Value> {
    let Some(e) = answer.get("effect") else { return Ok(answer) };
    st["inner"] = answer["state"].clone();
    if OBJECT_CALLS.contains(&e["type"].as_str().unwrap_or("")) {
        let id = st["id"].as_u64().unwrap_or(0) + 1;
        st["id"] = json!(id);
        st["phase"] = json!("sent");
        let f = seal(&mut st, &json!({ "id": id, "call": e }))?;
        return effect(st, json!({ "type": "linkSend", "frame": hex(&f) }));
    }
    st["phase"] = json!("call");
    effect(st, e.clone())
}

// ─── the serving phone ─────────────────────────────────────────────────────────────────────────────────────────────────

fn sdone(st: &Value, ok: bool, reason: &str) -> R<Value> {
    done(json!({ "ok": ok, "reason": reason, "paired": !st["ctx"].is_null(), "triesLeft": st["pairing"]["triesLeft"] }))
}

fn serve(req: &Value) -> R<Value> {
    let mut st = req["state"].clone();
    let step = st["step"].as_str().unwrap_or("start").to_string();
    let result = &req["result"];
    if step == "start" {
        let p = &req["params"]["pairing"];
        let (code, expires, tries) = (parse_code(p["code"].as_str().unwrap_or("")), p["expiresAt"].as_u64(), p["triesLeft"].as_u64());
        let (Ok(code), Some(expires), Some(tries)) = (code, expires, tries) else { return refuse("not_authenticated") };
        let st = json!({ "step": "hello", "buf": "", "queue": [], "pairing": { "code": code, "expiresAt": expires, "triesLeft": tries } });
        return recv(st, "hello");
    }
    let broke = |st: &Value| -> Option<R<Value>> {
        (result["failed"] == true).then(|| sdone(st, false, "link_failed"))
    };
    match step.as_str() {
        // Waiting for the other phone to speak. It has not asked anything yet, so nothing is counted.
        "hello" | "confirm" => {
            if let Some(r) = broke(&st) {
                return r;
            }
            let chunk = &result["chunk"];
            if chunk.is_null() {
                return sdone(&st, true, "closed");
            }
            let Ok(bodies) = pull(&mut st, chunk, false) else { return sdone(&st, false, "malformed_frame") };
            let Some(body) = bodies.first() else { return recv(st, &step) };
            let Ok(m) = unseal(&mut st, body) else { return sdone(&st, false, "malformed_frame") };
            if step == "hello" {
                if m["pair"] != "hello" || unhex(m["msg"].as_str().unwrap_or(""), "msg").is_err() {
                    return sdone(&st, false, "malformed_frame");
                }
                st["hello"] = m["msg"].clone();
                st["step"] = json!("now");
                return effect(st, json!({ "type": "now" }));
            }
            // A confirmation. A wrong one is the try the hello already cost.
            let (a, b, key) = (st["a"].as_str().unwrap_or(""), st["hello"].as_str().unwrap_or(""), st["key"].as_str().unwrap_or(""));
            let (Ok(a), Ok(b), Ok(key)) = (unhex(a, "a"), unhex(b, "b"), unhex(key, "key")) else { return sdone(&st, false, "malformed_frame") };
            if m["pair"] != "confirm" || !tag_ok(&key, "client", &a, &b, &m["tag"]) {
                return send(st, "fail_sent", &json!({ "pair": "fail" }));
            }
            let (c2s, s2c) = channel_keys(&key);
            let mut next = st.clone();
            // `ok` goes out plain, and the sealed channel starts after it.
            let ok_frame = framed(&json!({ "pair": "ok" }).to_string().into_bytes());
            next["ctx"] = Value::Null;
            next["after"] = ctx(s2c, c2s);
            next["step"] = json!("ok_sent");
            effect(next, json!({ "type": "linkSend", "frame": hex(&ok_frame) }))
        }
        "now" => {
            let seconds = result["seconds"].as_u64().ok_or("now gave no seconds")?;
            let (expires, tries) = (st["pairing"]["expiresAt"].as_u64().unwrap_or(0), st["pairing"]["triesLeft"].as_u64().unwrap_or(0));
            let why = if seconds > expires { Some("code_expired") } else if tries == 0 { Some("code_spent") } else { None };
            if let Some(why) = why {
                st["why"] = json!(why);
                return send(st, "refused_sent", &json!({ "pair": "refused", "reason": why }));
            }
            // The session is counted now, whether or not it succeeds: a wrong guess costs a try.
            st["pairing"]["triesLeft"] = json!(tries - 1);
            st["step"] = json!("rand");
            effect(st, json!({ "type": "random", "bytes": PAIR_RANDOM }))
        }
        "rand" => {
            let seed = random_bytes(result)?;
            let code = st["pairing"]["code"].as_str().unwrap_or("").to_string();
            let b = unhex(st["hello"].as_str().unwrap_or(""), "msg")?;
            let (state, a) = spake(true, &code, &seed);
            let Ok(key) = state.finish(&b) else { return send(st, "fail_sent", &json!({ "pair": "fail" })) };
            st["a"] = json!(hex(&a));
            st["key"] = json!(hex(&key));
            let t = tag(&key, "server", &a, &b);
            send(st, "reply_sent", &json!({ "pair": "reply", "msg": hex(&a), "tag": t }))
        }
        "reply_sent" => {
            if result["failed"] == true || result["ok"] != true {
                return sdone(&st, false, "link_failed");
            }
            recv(st, "confirm")
        }
        "refused_sent" => {
            let reason = if result["failed"] == true { "link_failed" } else { st["why"].as_str().unwrap_or("code_expired") };
            sdone(&st, false, reason)
        }
        "fail_sent" => sdone(&st, false, "wrong_code"),
        "ok_sent" => {
            if result["failed"] == true || result["ok"] != true {
                return sdone(&st, false, "link_failed");
            }
            st["ctx"] = st["after"].clone();
            st["after"] = Value::Null;
            // Frames that came after the confirmation are the sealed channel's.
            recv(st, "recv")
        }
        "recv" => {
            if let Some(r) = broke(&st) {
                return r;
            }
            let chunk = &result["chunk"];
            if chunk.is_null() {
                return sdone(&st, true, "closed");
            }
            let Ok(msgs) = messages(&mut st, chunk) else { return sdone(&st, false, "malformed_frame") };
            let mut queue = st["queue"].as_array().cloned().unwrap_or_default();
            queue.extend(msgs);
            st["queue"] = json!(queue);
            serve_next(st)
        }
        "served" => {
            let reply = json!({ "id": st["cur"], "result": result });
            send(st, "sent", &reply)
        }
        "sent" => {
            if result["failed"] == true || result["ok"] != true {
                return sdone(&st, false, "link_failed");
            }
            serve_next(st)
        }
        other => Err(format!("unknown serve step '{other}'")),
    }
}

/// The next queued request: apply it, or refuse it; with none left, wait for more bytes.
fn serve_next(mut st: Value) -> R<Value> {
    let mut queue = st["queue"].as_array().cloned().unwrap_or_default();
    if queue.is_empty() {
        return recv(st, "recv");
    }
    let msg = queue.remove(0);
    st["queue"] = json!(queue);
    if !allowed(&msg["call"]) {
        return send(st, "sent", &json!({ "id": msg["id"], "result": { "failed": true } }));
    }
    st["cur"] = msg["id"].clone();
    st["step"] = json!("served");
    effect(st, msg["call"].clone())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::migration::migration_json;

    const CODE: &str = "12345678";

    fn ask(v: Value) -> Value {
        serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
    }

    fn server(params: Value) -> Value {
        ask(json!({ "op": "flow", "flow": "serve", "params": params }))
    }

    fn step(a: &Value, result: Value) -> Value {
        ask(json!({ "op": "flow", "flow": "serve", "state": a["state"], "result": result }))
    }

    fn offer(tries: u64) -> Value {
        json!({ "pairing": { "code": CODE, "expiresAt": 1120, "triesLeft": tries } })
    }

    /// The frame an effect sends, as a body: the length prefix off.
    fn body_of(effect: &Value) -> Vec<u8> {
        unhex(effect["frame"].as_str().unwrap(), "frame").unwrap()[4..].to_vec()
    }

    fn chunk(f: &[u8]) -> Value {
        json!({ "chunk": hex(f) })
    }

    /// A hand-driven client that pairs with `code`, to be taken to the point the server answers with `ok` (or `fail`).
    /// Returns the server's answer after the confirmation and the client's channel.
    fn pair(code: &str, tries: u64) -> (Value, Value) {
        let mut a = server(offer(tries));
        assert_eq!(a["effect"]["type"], "linkRecv");
        let seed = vec![7u8; 96];
        let (_, mine) = spake(false, code, &seed);
        a = step(&a, chunk(&framed(&json!({ "pair": "hello", "msg": hex(&mine) }).to_string().into_bytes())));
        assert_eq!(a["effect"]["type"], "now");
        a = step(&a, json!({ "seconds": 1000 }));
        assert_eq!(a["effect"]["type"], "random");
        a = step(&a, json!({ "bytes": hex(&[9u8; 96]) }));
        assert_eq!(a["effect"]["type"], "linkSend");
        let reply: Value = serde_json::from_slice(&body_of(&a["effect"])).unwrap();
        let theirs = unhex(reply["msg"].as_str().unwrap(), "msg").unwrap();
        let (state, _) = spake(false, code, &seed);
        let key = state.finish(&theirs).unwrap();
        a = step(&a, json!({ "ok": true }));
        assert_eq!(a["effect"]["type"], "linkRecv");
        let confirm = json!({ "pair": "confirm", "tag": tag(&key, "client", &theirs, &mine) });
        a = step(&a, chunk(&framed(&confirm.to_string().into_bytes())));
        let (c2s, s2c) = channel_keys(&key);
        (a, json!({ "ctx": ctx(c2s, s2c) }))
    }

    fn paired_server() -> (Value, Value) {
        let (a, client) = pair(CODE, 3);
        let ok: Value = serde_json::from_slice(&body_of(&a["effect"])).unwrap();
        assert_eq!(ok, json!({ "pair": "ok" }));
        let a = step(&a, json!({ "ok": true }));
        assert_eq!(a["effect"]["type"], "linkRecv");
        (a, client)
    }

    // @verifies SWR-MG-10
    #[test]
    fn a_serving_phone_with_no_valid_offer_serves_nothing() {
        for params in [json!({}), json!({ "pairing": null }), json!({ "pairing": { "code": "1234", "expiresAt": 5, "triesLeft": 3 } }),
                       json!({ "pairing": { "code": CODE, "triesLeft": 3 } }), json!({ "pairing": { "code": CODE, "expiresAt": 5 } }), json!({ "authenticated": true })] {
            assert_eq!(server(params.clone())["done"], json!({ "ok": false, "reason": "not_authenticated" }), "{params}");
        }
    }

    // @verifies SWR-MG-10
    #[test]
    fn a_serving_phone_answers_only_the_migration_objects_and_the_probe_s() {
        let refused = [
            json!({ "type": "objectGet", "name": "secret.db" }),
            json!({ "type": "objectPut", "name": "../np-migration-bundle.npbk", "data": "00" }),
            json!({ "type": "objectDelete", "name": "np-migrationX" }),
            json!({ "type": "objectList", "prefix": "" }),
            json!({ "type": "objectList", "prefix": "secret" }),
            json!({ "type": "readKeys", "keys": ["protocols"] }),
            json!({ "type": "runClearStep", "step": "destroyEncryptionKey" }),
        ];
        for call in refused {
            let (a, mut client) = paired_server();
            let f = seal(&mut client, &json!({ "id": 9, "call": call })).unwrap();
            let a = step(&a, chunk(&f));
            assert_eq!(a["effect"]["type"], "linkSend", "{call}");
            let reply = unseal(&mut client, &body_of(&a["effect"])).unwrap();
            assert_eq!(reply, json!({ "id": 9, "result": { "failed": true } }), "{call}");
        }
        let names = ask(json!({ "op": "objectNames" }));
        for call in [
            json!({ "type": "objectGet", "name": names["bundle"] }),
            json!({ "type": "objectCreate", "name": names["claim"], "data": "00" }),
            json!({ "type": "objectPut", "name": "np-probe-1", "data": "00" }),
            json!({ "type": "objectList", "prefix": "np-probe-" }),
            json!({ "type": "objectSpace" }),
        ] {
            let (a, mut client) = paired_server();
            let f = seal(&mut client, &json!({ "id": 1, "call": call })).unwrap();
            let a = step(&a, chunk(&f));
            assert_eq!(a["effect"]["type"], call["type"], "{call} is for the driver to apply");
        }
    }

    // @verifies SWR-MG-10
    #[test]
    fn a_sealed_frame_cannot_be_read_altered_replayed_or_swapped_for_a_plain_one() {
        let (a, mut client) = paired_server();
        let call = json!({ "id": 1, "call": { "type": "objectSpace" } });
        let f = seal(&mut client, &call).unwrap();
        assert!(!String::from_utf8_lossy(&f).contains("objectSpace"), "the request is not on the wire in the clear");
        // Altered.
        let mut bad = f.clone();
        let last = bad.len() - 1;
        bad[last] ^= 1;
        assert_eq!(step(&a, chunk(&bad))["done"]["reason"], "malformed_frame");
        // Plain, as a bystander who never paired would send it.
        let plain = framed(&call.to_string().into_bytes());
        assert_eq!(step(&a, chunk(&plain))["done"]["reason"], "malformed_frame");
        // Replayed: the first goes through, the same bytes again do not.
        let first = step(&a, chunk(&f));
        assert_eq!(first["effect"]["type"], "objectSpace");
        let sent = step(&first, json!({ "free": 1 }));
        assert_eq!(sent["effect"]["type"], "linkSend");
        let again = step(&step(&sent, json!({ "ok": true })), chunk(&f));
        assert_eq!(again["done"]["reason"], "malformed_frame", "{again}");
    }

    // @verifies SWR-MG-10
    #[test]
    fn a_phone_that_does_not_know_the_code_is_sent_no_bundle_and_the_session_costs_a_try() {
        let (a, _) = pair("87654321", 3);
        let m: Value = serde_json::from_slice(&body_of(&a["effect"])).unwrap();
        assert_eq!(m, json!({ "pair": "fail" }), "the server refuses a confirmation made under another code");
        let end = step(&a, json!({ "ok": true }));
        assert_eq!(end["done"], json!({ "ok": false, "reason": "wrong_code", "paired": false, "triesLeft": 2 }));
    }

    // @verifies SWR-MG-10
    #[test]
    fn a_hello_that_is_not_one_ends_the_serve_and_a_frame_over_the_bound_is_refused() {
        let a = server(offer(3));
        assert_eq!(step(&a, chunk(&framed(b"not json")))["done"]["reason"], "malformed_frame");
        assert_eq!(step(&a, chunk(&framed(&json!({ "pair": "bye" }).to_string().into_bytes())))["done"]["reason"], "malformed_frame");
        assert_eq!(step(&a, chunk(&framed(&json!({ "pair": "hello", "msg": "zz" }).to_string().into_bytes())))["done"]["reason"], "malformed_frame");
        let big = ((32u32 * 1024 * 1024) + 1).to_be_bytes();
        assert_eq!(step(&a, chunk(&big))["done"]["reason"], "malformed_frame");
        assert_eq!(step(&a, json!({ "failed": true }))["done"]["reason"], "link_failed");
        // A phone that connects and leaves without a word costs no try.
        assert_eq!(step(&a, json!({ "chunk": null }))["done"], json!({ "ok": true, "reason": "closed", "paired": false, "triesLeft": 3 }));
    }

    // @verifies SWR-MG-10
    #[test]
    fn the_code_is_eight_digits_read_in_any_grouping_and_its_policy_is_the_cores() {
        assert_eq!(ask(json!({ "op": "pairingPolicy" })), json!({ "digits": 8, "lifetimeSeconds": 120, "tries": 3, "randomBytes": 32 }));
        let c = ask(json!({ "op": "pairingCode", "bytes": hex(&(0..32u8).collect::<Vec<_>>()) }));
        assert_eq!(c["code"], "01234567");
        // Bytes of 250 and over are skipped, so every digit is as likely as any other.
        let c = ask(json!({ "op": "pairingCode", "bytes": hex(&[255, 250, 3, 4, 5, 6, 7, 8, 9, 10, 11]) }));
        assert_eq!(c["code"], "34567890");
        assert!(migration_json(&json!({ "op": "pairingCode", "bytes": hex(&[255u8; 32]) }).to_string()).is_err());
        for (typed, want) in [("1234 5678", true), ("1234-5678", true), (" 12345678 ", true), ("1234567", false), ("123456789", false), ("1234 abcd", false), ("", false)] {
            assert_eq!(ask(json!({ "op": "pairingParse", "text": typed }))["ok"], want, "{typed:?}");
        }
    }
}
