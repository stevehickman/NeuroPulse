//! What a direct link's flow state holds, through the core's public entry point (SWR-MG-28 (c), OI-PRIV-12 stage 1).
//!
//! Two phones are paired here as two drivers would pair them: only `migration_json`, the frames each side sends, the
//! clock and the random bytes. The unit tests in `link.rs` and `migration_flow.rs` check the same rule turn by turn
//! through a whole restore; this one stands outside the crate, where a host stands.

use neurone_npps_core::api::migration_json;
use serde_json::{json, Value};

const CODE: &str = "48151623";

fn ask(v: Value) -> Value {
    serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

struct End {
    flow: &'static str,
    answer: Value,
    /// Hex of the bytes the other end has sent and this one has not yet read.
    inbox: String,
    fill: u8,
}

impl End {
    fn paired(&self) -> bool {
        self.answer["state"]["ctx"].is_object()
    }
    /// Answer this end's pending effect if it can be answered now. Returns the frame it sent, if it sent one.
    fn turn(&mut self) -> Option<Option<String>> {
        let e = self.answer["effect"].clone();
        let mut sent = None;
        let result = match e["type"].as_str()? {
            "random" => json!({ "bytes": hex(&vec![self.fill; e["bytes"].as_u64().unwrap() as usize]) }),
            "now" => json!({ "seconds": 1_000 }),
            "linkSend" => {
                sent = Some(e["frame"].as_str().unwrap().to_string());
                json!({ "ok": true })
            }
            "linkRecv" if !self.inbox.is_empty() => json!({ "chunk": std::mem::take(&mut self.inbox) }),
            _ => return None,
        };
        self.answer = ask(json!({ "op": "flow", "flow": self.flow, "state": self.answer["state"], "result": result }));
        Some(sent)
    }
}

/// The names of the fields a state holds at its top level and under `pairing`.
fn held(state: &Value) -> Vec<String> {
    let mut names: Vec<String> = state.as_object().map(|o| o.keys().cloned().collect()).unwrap_or_default();
    if let Some(p) = state["pairing"].as_object() {
        names.extend(p.keys().map(|k| format!("pairing.{k}")));
    }
    names
}

// @verifies SWR-MG-28#c
#[test]
fn two_paired_phones_hold_the_channel_keys_and_no_code_seed_or_shared_key() {
    let client_start = ask(json!({ "op": "flow", "flow": "restore", "params": { "link": true, "objectStore": true, "pairing": { "code": CODE } } }));
    let server_start = ask(json!({ "op": "flow", "flow": "serve", "params": { "pairing": { "code": CODE, "expiresAt": 1_120, "triesLeft": 3 } } }));
    // The restoring phone keeps the request to start its flow with, and that copy has no code in it from the first turn.
    assert!(client_start["state"]["origin"]["params"].get("pairing").is_none(), "{}", client_start["state"]["origin"]);

    let mut ends = [
        End { flow: "restore", answer: client_start, inbox: String::new(), fill: 0x5c },
        End { flow: "serve", answer: server_start, inbox: String::new(), fill: 0xa3 },
    ];
    let mut during = [false; 2];
    for _ in 0..200 {
        if ends[0].paired() && ends[1].paired() {
            break;
        }
        let mut moved = false;
        for side in 0..2 {
            if ends[side].paired() || ends[side].answer.get("done").is_some() {
                continue;
            }
            if let Some(sent) = ends[side].turn() {
                moved = true;
                if let Some(frame) = sent {
                    ends[1 - side].inbox.push_str(&frame);
                }
                // While the pairing is under way the code is in the state: that is said, not hidden.
                during[side] |= ends[side].answer["state"].to_string().contains(CODE);
            }
        }
        assert!(moved, "the two ends are each waiting for the other: {} / {}", ends[0].answer, ends[1].answer);
    }
    assert!(during[0] && during[1], "the test saw the code in both states during the pairing, so its absence after means something");

    for (side, end) in ends.iter().enumerate() {
        let st = &end.answer["state"];
        assert!(end.paired(), "side {side} did not pair: {}", end.answer);
        let names = held(st);
        for gone in ["code", "seed", "a", "b", "hello", "key", "keys", "after", "origin", "pairing.code"] {
            assert!(!names.iter().any(|n| n == gone), "side {side} still holds '{gone}' once paired: {names:?}");
        }
        assert!(!st.to_string().contains(CODE), "side {side}: the code is in the state once paired");
        // What does remain: one key for each direction, as hex, and their message counts.
        assert_eq!(st["ctx"]["sk"].as_str().map(str::len), Some(64), "side {side}");
        assert_eq!(st["ctx"]["rk"].as_str().map(str::len), Some(64), "side {side}");
    }
    // The two ends hold the same two keys, crossed: the pairing worked, and the keys are this session's.
    assert_eq!(ends[0].answer["state"]["ctx"]["sk"], ends[1].answer["state"]["ctx"]["rk"]);
    assert_eq!(ends[0].answer["state"]["ctx"]["rk"], ends[1].answer["state"]["ctx"]["sk"]);
    assert_eq!(ends[1].answer["state"]["pairing"]["triesLeft"], 2, "the tries left outlive the code, for the host");
}
