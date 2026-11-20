//! Secrets the core handles are not left where the next reader finds them (`SWR-MG-28`), through the public API every
//! binding calls. The same rules are tested closer to the code in `link.rs`, `migration_flow.rs`, `bundle_key.rs` and
//! `sigv4.rs`; these are the ones a host could observe.

use neurone_npps_core::api::migration_json;
use neurone_npps_core::bundle_key::passphrase_secret;
use serde_json::{json, Value};
use unicode_normalization::UnicodeNormalization;

const CODE: &str = "48151623";

fn ask(v: Value) -> Result<Value, String> {
    migration_json(&v.to_string()).map(|s| serde_json::from_str(&s).unwrap())
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

/// One end of a link: the flow's name, its last answer, and the bytes waiting for it.
struct End {
    flow: &'static str,
    answer: Value,
    inbox: Vec<u8>,
}

impl End {
    fn start(flow: &'static str, params: Value) -> End {
        End { flow, answer: ask(json!({ "op": "flow", "flow": flow, "params": params })).unwrap(), inbox: vec![] }
    }
    fn state(&self) -> &Value {
        &self.answer["state"]
    }
    fn effect(&self) -> &str {
        self.answer["effect"]["type"].as_str().unwrap_or("")
    }
    fn advance(&mut self, result: Value) -> Result<(), String> {
        self.answer = ask(json!({ "op": "flow", "flow": self.flow, "state": self.answer["state"], "result": result }))?;
        Ok(())
    }
}

/// Every object key in `v`, at any depth.
fn keys_of(v: &Value, out: &mut Vec<String>) {
    match v {
        Value::Object(o) => o.iter().for_each(|(k, v)| {
            out.push(k.clone());
            keys_of(v, out);
        }),
        Value::Array(a) => a.iter().for_each(|v| keys_of(v, out)),
        _ => {}
    }
}

/// Run the pairing between a restoring and a serving phone, giving each step's two states to `watch`, until both hold a
/// sealed channel or one of them ends. Only the link's own effects are answered: the first effect of the restore itself
/// is left unanswered, which is where a paired link stands when its work begins.
fn pair(client: &mut End, server: &mut End, watch: &mut dyn FnMut(&Value, &Value)) {
    for _ in 0..200 {
        watch(client.state(), server.state());
        if !client.state()["ctx"].is_null() && !server.state()["ctx"].is_null() {
            return;
        }
        let mut moved = false;
        for side in 0..2 {
            let (me, other) = if side == 0 { (&mut *client, &mut *server) } else { (&mut *server, &mut *client) };
            let result = match me.effect() {
                "random" => json!({ "bytes": hex(&vec![0x11 * (side as u8 + 1); me.answer["effect"]["bytes"].as_u64().unwrap() as usize]) }),
                "now" => json!({ "seconds": 1_000 }),
                "linkSend" => {
                    other.inbox.extend(unhex(me.answer["effect"]["frame"].as_str().unwrap()));
                    json!({ "ok": true })
                }
                "linkRecv" if !me.inbox.is_empty() => json!({ "chunk": hex(&std::mem::take(&mut me.inbox)) }),
                _ => continue,
            };
            me.advance(result).unwrap();
            moved = true;
        }
        assert!(moved, "the pairing stopped: {} / {}", client.answer, server.answer);
    }
    panic!("the pairing did not end");
}

fn two_ends() -> (End, End) {
    let client = End::start("restore", json!({ "link": true, "objectStore": true, "pairing": { "code": CODE } }));
    let server = End::start("serve", json!({ "pairing": { "code": CODE, "expiresAt": 1_120, "triesLeft": 3 } }));
    (client, server)
}

// @verifies SWR-MG-28#d
#[test]
fn a_paired_link_s_states_hold_the_channel_keys_and_nothing_else_of_the_pairing() {
    let (mut client, mut server) = two_ends();
    let mut wrong: Vec<String> = vec![];
    let mut seen_code = [false; 2];
    pair(&mut client, &mut server, &mut |c, s| {
        for (who, st, side) in [("restoring", c, 0), ("serving", s, 1)] {
            let mut keys = vec![];
            keys_of(st, &mut keys);
            let text = st.to_string();
            seen_code[side] |= text.contains(CODE);
            // The shared key has been made once the channel keys are in the state, under any of their three names.
            let made = !st["ctx"].is_null() || !st["keys"].is_null() || !st["after"].is_null();
            if made {
                for gone in ["code", "seed", "key", "hello", "a", "b"] {
                    if keys.iter().any(|k| k == gone) {
                        wrong.push(format!("the {who} phone's state holds '{gone}' after the shared key was made"));
                    }
                }
                if text.contains(CODE) {
                    wrong.push(format!("the {who} phone's state holds the code after the shared key was made"));
                }
            }
            if who == "restoring" && keys.iter().any(|k| k == "pairing") {
                wrong.push("the restoring phone's state holds the pairing it was started with, beside the code".into());
            }
        }
    });
    assert!(seen_code[0] && seen_code[1], "the code was in each state while the pairing needed it, so the checks above saw it go");
    assert!(wrong.is_empty(), "{}", wrong.join("\n"));

    // What is left. The restoring phone: the channel, its buffer and call count, and the restore's own state.
    let mut c: Vec<&str> = client.state().as_object().unwrap().keys().map(String::as_str).collect();
    c.sort_unstable();
    assert_eq!(c, ["buf", "ctx", "id", "inner", "link", "phase"]);
    // The serving phone: the channel, its buffer and queue, and the offer's expiry and tries (not its code).
    let mut s: Vec<&str> = server.state().as_object().unwrap().keys().map(String::as_str).collect();
    s.sort_unstable();
    assert_eq!(s, ["buf", "ctx", "pairing", "queue", "step"]);
    assert_eq!(server.state()["pairing"], json!({ "expiresAt": 1_120, "triesLeft": 2 }));
    // The channel: a key and a count per direction. The keys are in the core, and the state names them (next test).
    for st in [client.state(), server.state()] {
        let mut k: Vec<&str> = st["ctx"].as_object().unwrap().keys().map(String::as_str).collect();
        k.sort_unstable();
        assert_eq!(k, ["rk", "rn", "sk", "sn"]);
    }
    release(&client, &server);
}

/// Have the core wipe the channel keys of two ends a test has finished with: its table is one per process.
fn release(client: &End, server: &End) {
    for end in [client, server] {
        ask(json!({ "op": "linkRelease", "state": end.state() })).unwrap();
    }
}

/// Every string in `v`, at any depth.
fn strings_of(v: &Value, out: &mut Vec<String>) {
    match v {
        Value::String(s) => out.push(s.clone()),
        Value::Object(o) => o.values().for_each(|v| strings_of(v, out)),
        Value::Array(a) => a.iter().for_each(|v| strings_of(v, out)),
        _ => {}
    }
}

// @verifies SWR-MG-32#f
#[test]
fn a_link_s_channel_keys_are_in_the_core_and_its_state_names_them_and_holds_none() {
    let held = || ask(json!({ "op": "keyCount" })).unwrap()["count"].as_u64().unwrap();
    let (mut client, mut server) = two_ends();
    let mut keys_in_text = vec![];
    pair(&mut client, &mut server, &mut |c, s| {
        // At no step, pairing or paired, does a state hold anything shaped like a 32-byte key in hex under a channel's name.
        for st in [c, s] {
            for channel in ["ctx", "after", "keys"] {
                let mut strings = vec![];
                strings_of(&st[channel], &mut strings);
                keys_in_text.extend(strings.into_iter().filter(|t| t.len() >= 32));
            }
        }
    });
    assert!(keys_in_text.is_empty(), "a channel key is in a state as text: {keys_in_text:?}");
    for (who, end) in [("restoring", &client), ("serving", &server)] {
        let c = &end.state()["ctx"];
        assert!(c["sk"]["$handle"].is_u64() && c["rk"]["$handle"].is_u64(), "the {who} phone's channel: {c}");
        assert_ne!(c["sk"], c["rk"], "one key for each direction");
        assert_eq!(c.as_object().unwrap().len(), 4, "two handles and two counts, and nothing else: {c}");
    }
    // A state that carries a channel key as text is refused: there is no going back to a key the host holds.
    let mut forged = server.state().clone();
    forged["ctx"]["rk"] = json!("11".repeat(32));
    let r = ask(json!({ "op": "flow", "flow": "serve", "state": forged, "result": { "chunk": "0000000100" } })).unwrap();
    assert_eq!(r["done"]["reason"], "malformed_frame", "{r}");
    // That refusal ended a serve that named the serving phone's keys, so the core wiped them: the real state opens nothing now.
    let after = ask(json!({ "op": "flow", "flow": "serve", "state": server.state(), "result": { "chunk": "0000000100" } })).unwrap();
    assert_eq!(after["done"]["reason"], "malformed_frame");

    // A driver that stops part-way has the core wipe what its last state names, and that state is then of no use.
    let before = held();
    let released = ask(json!({ "op": "linkRelease", "state": client.state() })).unwrap();
    assert_eq!(released, json!({ "released": 2 }));
    assert_eq!(held(), before - 2);
    assert_eq!(ask(json!({ "op": "linkRelease", "state": client.state() })).unwrap(), json!({ "released": 0 }), "twice is safe");
    assert_eq!(ask(json!({ "op": "linkRelease", "state": null })).unwrap(), json!({ "released": 0 }));
}

// @verifies SWR-MG-28#e
#[test]
fn a_driver_that_gives_fewer_random_bytes_than_a_pairing_asked_for_is_refused_at_both_ends() {
    let (mut client, mut server) = two_ends();
    assert_eq!(client.effect(), "random");
    let asked = client.answer["effect"]["bytes"].as_u64().unwrap() as usize;
    for short in [0, 1, 16, asked - 1] {
        let state = client.answer.clone();
        let refusal = client.advance(json!({ "bytes": hex(&vec![7u8; short]) }));
        assert!(refusal.is_err(), "{short} bytes were taken for {asked}");
        client.answer = state;
    }
    // The serving phone asks for its own after the hello and the clock.
    client.advance(json!({ "bytes": hex(&vec![7u8; asked]) })).unwrap();
    server.inbox = unhex(client.answer["effect"]["frame"].as_str().unwrap());
    let hello = hex(&std::mem::take(&mut server.inbox));
    server.advance(json!({ "chunk": hello })).unwrap();
    server.advance(json!({ "seconds": 1_000 })).unwrap();
    assert_eq!(server.effect(), "random");
    assert!(server.advance(json!({ "bytes": hex(&[7u8; 16]) })).is_err(), "16 bytes were stretched over a session");
}

// @verifies SWR-MG-28#c
#[test]
fn a_passphrase_that_outgrows_its_buffer_when_normalised_is_still_exactly_its_nfkc_form() {
    // U+FDFA is 3 bytes and normalises to 33. The secret's buffer is outgrown several times, and each time it is moved
    // by hand into a new wiped buffer instead of being reallocated; the bytes must be what one pass of NFKC gives.
    let typed = "\u{FDFA}".repeat(40) + "ﬁve words and a ligature";
    let plain: String = typed.nfkc().collect();
    assert!(plain.len() > 4 * typed.len());
    assert_eq!(*passphrase_secret(&typed).unwrap(), plain.into_bytes());
    let through_the_api = ask(json!({ "op": "passphraseSecret", "passphrase": typed })).unwrap();
    assert_eq!(through_the_api["secret"], hex(&typed.nfkc().collect::<String>().into_bytes()));
}
