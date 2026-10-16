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
//! * **The phone that restores** (the client) starts its flow with `link: true` (and `objectStore: true`). Its `object*`
//!   effects are turned by this layer into `linkSend` and `linkRecv`, so the flow runs unchanged over the link and a driver
//!   answers only those two, the same on every platform.
//! * **The phone that holds the bundle** (the server) runs `flow: "serve"` with `authenticated: true`. It receives requests,
//!   asks its driver to apply each to its own object store, and sends the responses. It refuses to start unless the link is
//!   authenticated (`SWR-MG-10`: a bystander's phone must not be served), and it answers only the migration objects
//!   (`np-migration-*`) and the probe's (`np-probe-*`): a paired peer cannot read or write anything else the phone stores.
//!   The server arbitrates the claim, because it alone applies `objectCreate`.
//!
//! **How a link becomes authenticated is not here.** The pairing (a code shown on one phone and entered on the other, or
//! another way) is open; this layer takes its outcome as a fact the app asserts. The bundle is encrypted under the user's key
//! whatever the link does, so an unauthenticated link cannot read it, but it could race for the claim.
//!
//! | type | args | result |
//! |------|------|--------|
//! | `linkSend` | `frame` (hex) | `{ok}`: the bytes are on their way |
//! | `linkRecv` | none | `{chunk: hex}` the next bytes, or `{chunk: null}` when the link is closed; `{failed: true}` if it broke |

use crate::migration::{hex, unhex};
use crate::object_route::{BUNDLE_OBJECT, CLAIM_OBJECT, CONFIRMATION_OBJECT, PROBE_PREFIX};
use serde_json::{json, Value};

type R<T> = Result<T, String>;

/// The largest frame a peer may send (`UC-090`): a bundle is sent hex-encoded in one frame, so this bounds the bundle a link
/// can carry, and stops a peer from making the receiver buffer without end.
pub(crate) const MAX_FRAME: usize = 32 * 1024 * 1024;

const OBJECT_CALLS: [&str; 6] = ["objectGet", "objectPut", "objectCreate", "objectDelete", "objectList", "objectSpace"];
const MIGRATION_PREFIX: &str = "np-migration-";

fn frame(msg: &Value) -> Vec<u8> {
    let body = msg.to_string().into_bytes();
    let mut out = (body.len() as u32).to_be_bytes().to_vec();
    out.extend(body);
    out
}

/// Whole messages out of a stream: `buffer` is what was left over from the last chunk.
fn feed(buffer: &[u8], chunk: &[u8]) -> R<(Vec<Value>, Vec<u8>)> {
    let mut buf = buffer.to_vec();
    buf.extend_from_slice(chunk);
    let mut messages = vec![];
    loop {
        if buf.len() < 4 {
            break;
        }
        let len = u32::from_be_bytes([buf[0], buf[1], buf[2], buf[3]]) as usize;
        if len > MAX_FRAME {
            return Err("frame too large".into());
        }
        if buf.len() < 4 + len {
            break;
        }
        messages.push(serde_json::from_slice(&buf[4..4 + len]).map_err(|e| format!("malformed frame: {e}"))?);
        buf.drain(..4 + len);
    }
    Ok((messages, buf))
}

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

/// The pure operations: `linkRequest`, `linkResponse` and `linkFeed`.
pub(crate) fn op(name: &str, req: &Value) -> Option<R<Value>> {
    Some(match name {
        "linkRequest" => Ok(json!({ "frame": hex(&frame(&json!({ "id": req["id"], "call": req["call"] }))) })),
        "linkResponse" => Ok(json!({ "frame": hex(&frame(&json!({ "id": req["id"], "result": req["result"] }))) })),
        "linkFeed" => (|| {
            let (messages, rest) = feed(&unhex(req["buffer"].as_str().unwrap_or(""), "buffer")?, &unhex(req["chunk"].as_str().unwrap_or(""), "chunk")?)?;
            Ok(json!({ "messages": messages, "buffer": hex(&rest) }))
        })(),
        _ => return None,
    })
}

fn effect(state: Value, e: Value) -> R<Value> {
    Ok(json!({ "effect": e, "state": state }))
}

fn fed(st: &Value, chunk: &Value) -> R<(Vec<Value>, String)> {
    let (messages, rest) = feed(&unhex(st["buf"].as_str().unwrap_or(""), "buffer")?, &unhex(chunk.as_str().unwrap_or(""), "chunk")?)?;
    Ok((messages, hex(&rest)))
}

/// A flow over a link: `serve` on the phone that holds the bundle; on the other, `inner` run with its object calls carried
/// over the link when its params say `link: true`.
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
        let answer = inner(req)?;
        return intercept(req, answer, json!({ "link": true, "buf": "", "id": 0 }));
    }
    let st = req["state"].clone();
    let down = |result: Value| -> R<Value> {
        let next = json!({ "flow": req["flow"], "state": st["inner"], "result": result });
        let answer = inner(&next)?;
        intercept(req, answer, json!({ "link": true, "buf": st["buf"], "id": st["id"] }))
    };
    match st["phase"].as_str().unwrap_or("") {
        // The driver answered an effect that is not a link call: hand it down.
        "call" => down(req["result"].clone()),
        // The request left. Now wait for its answer.
        "sent" => {
            if req["result"]["failed"] == true || req["result"]["ok"] != true {
                return down(json!({ "failed": true }));
            }
            let mut st = st.clone();
            st["phase"] = json!("recv");
            effect(st, json!({ "type": "linkRecv" }))
        }
        "recv" => {
            let chunk = &req["result"]["chunk"];
            if req["result"]["failed"] == true || chunk.is_null() {
                return down(json!({ "failed": true }));
            }
            let Ok((messages, buf)) = fed(&st, chunk) else { return down(json!({ "failed": true })) };
            let mut st = st.clone();
            st["buf"] = json!(buf);
            match messages.iter().find(|m| m["id"] == st["id"]) {
                Some(m) => {
                    let rest = json!({ "link": true, "buf": st["buf"], "id": st["id"], "inner": st["inner"] });
                    let next = json!({ "flow": req["flow"], "state": rest["inner"], "result": m["result"] });
                    intercept(req, inner(&next)?, json!({ "link": true, "buf": rest["buf"], "id": rest["id"] }))
                }
                None => effect(st, json!({ "type": "linkRecv" })),
            }
        }
        other => Err(format!("unknown link phase '{other}'")),
    }
}

/// The inner flow's next effect: an object call goes over the link; anything else is the driver's.
fn intercept(_req: &Value, answer: Value, mut st: Value) -> R<Value> {
    let Some(e) = answer.get("effect") else { return Ok(answer) };
    st["inner"] = answer["state"].clone();
    if OBJECT_CALLS.contains(&e["type"].as_str().unwrap_or("")) {
        let id = st["id"].as_u64().unwrap_or(0) + 1;
        st["id"] = json!(id);
        st["phase"] = json!("sent");
        let f = hex(&frame(&json!({ "id": id, "call": e })));
        return effect(st, json!({ "type": "linkSend", "frame": f }));
    }
    st["phase"] = json!("call");
    effect(st, e.clone())
}

// ─── the serving phone ─────────────────────────────────────────────────────────────────────────────────────────────────

fn serve(req: &Value) -> R<Value> {
    let mut st = req["state"].clone();
    let step = st["step"].as_str().unwrap_or("start").to_string();
    match step.as_str() {
        "start" => {
            if req["params"]["authenticated"] != true {
                return Ok(json!({ "done": { "ok": false, "reason": "not_authenticated" } }));
            }
            effect(json!({ "step": "recv", "buf": "", "queue": [] }), json!({ "type": "linkRecv" }))
        }
        "recv" => {
            let chunk = &req["result"]["chunk"];
            if req["result"]["failed"] == true {
                return Ok(json!({ "done": { "ok": false, "reason": "link_failed" } }));
            }
            if chunk.is_null() {
                return Ok(json!({ "done": { "ok": true, "reason": "closed" } }));
            }
            let Ok((messages, buf)) = fed(&st, chunk) else { return Ok(json!({ "done": { "ok": false, "reason": "malformed_frame" } })) };
            st["buf"] = json!(buf);
            let mut queue = st["queue"].as_array().cloned().unwrap_or_default();
            queue.extend(messages);
            st["queue"] = json!(queue);
            serve_next(st)
        }
        "served" => {
            let reply = frame(&json!({ "id": st["cur"], "result": req["result"] }));
            st["step"] = json!("sent");
            effect(st, json!({ "type": "linkSend", "frame": hex(&reply) }))
        }
        "sent" => {
            if req["result"]["failed"] == true || req["result"]["ok"] != true {
                return Ok(json!({ "done": { "ok": false, "reason": "link_failed" } }));
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
        st["step"] = json!("recv");
        return effect(st, json!({ "type": "linkRecv" }));
    }
    let msg = queue.remove(0);
    st["queue"] = json!(queue);
    if !allowed(&msg["call"]) {
        let reply = frame(&json!({ "id": msg["id"], "result": { "failed": true } }));
        st["step"] = json!("sent");
        return effect(st, json!({ "type": "linkSend", "frame": hex(&reply) }));
    }
    st["cur"] = msg["id"].clone();
    st["step"] = json!("served");
    effect(st, msg["call"].clone())
}
