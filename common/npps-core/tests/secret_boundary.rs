//! A key crosses the core's boundary as bytes or not at all (`SWR-MG-32`; `OI-PRIV-12` stage 2). Through the entry point
//! every binding calls for a key-bearing operation: `migration_secret_json`.

use neurone_npps_core::api::{migration_json, migration_secret_json};
use serde_json::{json, Value};
use std::sync::{Mutex, MutexGuard};

/// The table of kept keys is one per process, and these tests count it and empty it.
static ONE: Mutex<()> = Mutex::new(());

fn one() -> MutexGuard<'static, ()> {
    let guard = ONE.lock().unwrap_or_else(|e| e.into_inner());
    migration_json(r#"{"op":"keyReleaseAll"}"#).unwrap();
    guard
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn frame(secrets: &[&[u8]]) -> Vec<u8> {
    let mut out = vec![];
    for s in secrets {
        out.extend_from_slice(&(s.len() as u32).to_be_bytes());
        out.extend_from_slice(s);
    }
    out
}

fn unframe(mut bytes: &[u8]) -> Vec<Vec<u8>> {
    let mut out = vec![];
    while !bytes.is_empty() {
        let len = u32::from_be_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]) as usize;
        out.push(bytes[4..4 + len].to_vec());
        bytes = &bytes[4 + len..];
    }
    out
}

/// The old way: everything in the JSON.
fn ask(v: Value) -> Value {
    serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
}

/// The new way: the answer, the secrets beside it, and the answer's own text (to search for a key).
fn ask_with(v: Value, secrets: &[&[u8]]) -> Result<(Value, Vec<Vec<u8>>, String), String> {
    let (text, out) = migration_secret_json(&v.to_string(), &frame(secrets))?;
    Ok((serde_json::from_str(&text).unwrap(), unframe(&out), text))
}

const KEY: [u8; 32] = [
    0xd3, 0x4c, 0x91, 0x7e, 0x02, 0xb8, 0x65, 0xfa, 0x1d, 0xa7, 0x30, 0xc9, 0x5b, 0xe4, 0x86, 0x2f, 0x79, 0x0a, 0xbd, 0x43, 0xe1, 0x58, 0x9c, 0x27, 0xf6, 0x3a, 0x84, 0xcd,
    0x12, 0x6f, 0xb0, 0x55,
];
const NONCE: &str = "a0a1a2a3a4a5a6a7a8a9aaab";

fn kdf(method: &str) -> Value {
    ask(json!({ "op": "bundleKdf", "method": method, "salt": "5a".repeat(32) }))["kdf"].clone()
}

// @verifies SWR-MG-32#a
#[test]
fn a_key_given_beside_the_request_does_what_the_same_key_in_hex_did_and_is_in_no_text() {
    let _one = one();
    let old = ask(json!({ "op": "sealValue", "key": hex(&KEY), "nonce": NONCE, "name": "np.n", "value": "history" }));
    let request = json!({ "op": "sealValue", "key": { "$secret": 0 }, "nonce": NONCE, "name": "np.n", "value": "history" });
    assert!(!request.to_string().contains(&hex(&KEY)[..16]), "the request names the key and does not hold it");
    let (new, beside, _) = ask_with(request, &[&KEY]).unwrap();
    assert_eq!(new, old, "the same bytes are sealed either way");
    assert!(beside.is_empty(), "nothing secret is in this answer");
    let (opened, _, _) = ask_with(json!({ "op": "openValue", "key": { "$secret": 0 }, "name": "np.n", "sealed": new["sealed"] }), &[&KEY]).unwrap();
    assert_eq!(opened["value"], "history");

    // A signing seed and a cloud secret go the same way.
    let seed = [7u8; 32];
    let (signed, _, _) = ask_with(json!({ "op": "ed25519Sign", "seed": { "$secret": 0 }, "message": "00ff" }), &[&seed]).unwrap();
    assert_eq!(signed, ask(json!({ "op": "ed25519Sign", "seed": hex(&seed), "message": "00ff" })));
    let aws = |secret: Value, token: Value| json!({ "op": "awsSigV4", "method": "GET", "host": "examplebucket.s3.amazonaws.com", "path": "/test.txt", "query": [],
        "headers": {}, "payloadHash": "UNSIGNED-PAYLOAD", "region": "us-east-1", "service": "s3", "accessKeyId": "AKIAIOSFODNN7EXAMPLE",
        "secretAccessKey": secret, "sessionToken": token, "amzDate": "20130524T000000Z" });
    let (signed, _, _) = ask_with(aws(json!({ "$secret": 1 }), json!({ "$secret": 0 })), &[b"a-session-token", b"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"]).unwrap();
    assert_eq!(signed, ask(aws(json!("wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"), json!("a-session-token"))));
}

// @verifies SWR-MG-32#a
#[test]
fn a_key_in_an_answer_is_beside_it_as_bytes_and_never_in_its_text() {
    let _one = one();
    let secret = b"a secret of the right length ...";
    let old_key = ask(json!({ "op": "deriveBundleKey", "secret": hex(secret), "kdf": kdf("recoveryCode") }))["key"].as_str().unwrap().to_string();
    let (answer, beside, text) = ask_with(json!({ "op": "deriveBundleKey", "secret": { "$secret": 0 }, "kdf": kdf("recoveryCode") }), &[secret]).unwrap();
    assert_eq!(answer, json!({ "ok": true, "key": { "$secret": 0 } }));
    assert_eq!(hex(&beside[0]), old_key, "the same key, as bytes");
    assert!(!text.contains(&old_key[..16]), "the answer's text holds the key: {text}");

    // The secret a passphrase stands for, and the device-bound key, likewise.
    let (answer, beside, text) = ask_with(json!({ "op": "passphraseSecret", "passphrase": { "$secret": 0 } }), &["correct horse battery".as_bytes()]).unwrap();
    assert_eq!(answer["secret"], json!({ "$secret": 0 }));
    assert_eq!(beside[0], b"correct horse battery");
    assert!(!text.contains(&hex(b"correct horse")), "{text}");
    let (answer, beside, _) = ask_with(json!({ "op": "deriveUhdrKey", "seed": { "$secret": 0 }, "salt": "09".repeat(32) }), &[&[7u8; 32]]).unwrap();
    assert_eq!(answer["key"], json!({ "$secret": 0 }));
    assert_eq!(hex(&beside[0]), ask(json!({ "op": "deriveUhdrKey", "seed": "07".repeat(32), "salt": "09".repeat(32) }))["key"].as_str().unwrap());
}

// @verifies SWR-MG-32#a
#[test]
fn a_secret_the_call_was_not_given_and_a_buffer_that_is_not_framed_are_refused() {
    let _one = one();
    let seal = json!({ "op": "sealValue", "key": { "$secret": 1 }, "nonce": NONCE, "name": "np.n", "value": "v" });
    assert!(ask_with(seal.clone(), &[&KEY]).unwrap_err().contains("was not given"));
    // The plain entry point is given nothing, so a reference there names nothing.
    assert!(migration_json(&seal.to_string()).is_err());
    // A secret does not outlive its call: the next call on this thread starts with none.
    assert!(ask_with(json!({ "op": "sealValue", "key": { "$secret": 0 }, "nonce": NONCE, "name": "np.n", "value": "v" }), &[]).is_err());
    for bad in [&[0u8, 0, 0][..], &[0, 0, 0, 9, 1, 2], &[0, 0, 0, 1, 7, 0]] {
        assert!(migration_secret_json(r#"{"op":"keyCount"}"#, bad).unwrap_err().contains("not framed"));
    }
    assert!(ask_with(json!({ "op": "sealValue", "key": { "$other": 0 }, "nonce": NONCE, "name": "np.n", "value": "v" }), &[&KEY]).is_err());
}

// @verifies SWR-MG-32#b
#[test]
fn a_derived_key_is_kept_in_the_core_and_used_by_its_handle() {
    let _one = one();
    let secret = b"a secret of the right length ...";
    let k = kdf("recoveryCode");
    let by_hex = ask(json!({ "op": "deriveBundleKey", "secret": hex(secret), "kdf": k }))["key"].clone();
    let (kept, beside, text) = ask_with(json!({ "op": "deriveBundleKey", "secret": { "$secret": 0 }, "kdf": k, "keep": "bundle" }), &[secret]).unwrap();
    let handle = kept["key"].clone();
    assert!(handle["$handle"].is_u64(), "{kept}");
    assert!(beside.is_empty() && !text.contains(&by_hex.as_str().unwrap()[..16]), "a kept key is not handed out in any form: {text}");

    // The handle does everything the bytes did, to the same bytes.
    let items = json!({});
    let seal = |key: &Value| ask(json!({ "op": "sealBundle", "epoch": 1, "kind": "uhdr", "key": key, "nonce": NONCE, "items": items, "kdf": k }));
    let (a, b) = (seal(&by_hex), seal(&handle));
    assert_eq!(a, b);
    assert_eq!(ask(json!({ "op": "openBundle", "seenEpoch": 0, "bundle": a["bundle"], "key": handle }))["ok"], true);
    let tag = |key: &Value| ask(json!({ "op": "confirmationTag", "key": key, "bundleHash": a["bundleHash"] }));
    assert_eq!(tag(&by_hex), tag(&handle));
    let header = ask(json!({ "op": "archiveStart", "salt": "01".repeat(16) }))["header"].clone();
    let chunk = |key: &Value| ask(json!({ "op": "archiveSealChunk", "objectId": "recording:r1", "key": key, "header": header, "counter": 0, "last": true, "data": "00ff" }));
    assert_eq!(chunk(&by_hex), chunk(&handle));

    // A key the host holds is brought in once, beside the request, and used by its handle after that.
    let (imported, _, _) = ask_with(json!({ "op": "keyImport", "purpose": "data", "key": { "$secret": 0 } }), &[&KEY]).unwrap();
    let data = imported["key"].clone();
    let sealed = ask(json!({ "op": "sealValue", "key": data, "nonce": NONCE, "name": "np.n", "value": "history" }));
    assert_eq!(sealed, ask(json!({ "op": "sealValue", "key": hex(&KEY), "nonce": NONCE, "name": "np.n", "value": "history" })));
    assert_eq!(ask(json!({ "op": "keyCount" })), json!({ "count": 2, "max": 64 }));
}

// @verifies SWR-MG-32#b
#[test]
fn a_handle_is_typed_never_reused_never_exported_bounded_and_gone_when_released() {
    let _one = one();
    let import = |purpose: &str, key: &[u8]| ask_with(json!({ "op": "keyImport", "purpose": purpose, "key": { "$secret": 0 } }), &[key]);
    let data = import("data", &KEY).unwrap().0["key"].clone();
    let n = data["$handle"].as_u64().unwrap();

    // Typed: a data key seals no bundle, and an operation that takes no kept key takes none.
    let wrong = migration_json(&json!({ "op": "sealBundle", "epoch": 1, "kind": "uhdr", "key": data, "nonce": NONCE, "items": {}, "kdf": kdf("recoveryCode") }).to_string());
    assert!(wrong.unwrap_err().starts_with("wrong_key_purpose"));
    assert!(migration_json(&json!({ "op": "deriveUhdrKey", "seed": data, "salt": "09".repeat(32) }).to_string()).unwrap_err().starts_with("wrong_key_purpose"));

    // A host brings in only what a host holds, and only beside the request.
    assert!(import("bundle", &KEY).is_err(), "a backup key is derived here, never brought in");
    assert!(import("nonsense", &KEY).is_err());
    assert!(import("data", &[]).is_err());
    assert!(migration_json(&json!({ "op": "keyImport", "purpose": "data", "key": hex(&KEY) }).to_string()).is_err(), "a key brought in as hex text would be the string this exists to avoid");
    assert!(migration_json(&json!({ "op": "deriveBundleKey", "secret": "00", "kdf": kdf("recoveryCode"), "keep": "data" }).to_string()).is_err());

    // Never exported: no operation answers with a kept key's bytes. The ones that could name it say only the count.
    for op in ["keyCount", "keyExport", "keyGet", "keyRead"] {
        let r = migration_secret_json(&json!({ "op": op, "key": data }).to_string(), &[]);
        if let Ok((text, beside)) = r {
            assert!(beside.is_empty() && !text.contains(&hex(&KEY)[..16]), "{op} answered with the key");
        }
    }

    // Released: gone, by name; released again: nothing to release; a number never issued: unknown.
    assert_eq!(ask(json!({ "op": "keyRelease", "key": data })), json!({ "released": true }));
    let seal = |key: &Value| migration_json(&json!({ "op": "sealValue", "key": key, "nonce": NONCE, "name": "np.n", "value": "v" }).to_string());
    assert_eq!(seal(&data).unwrap_err(), "unknown_key");
    assert_eq!(ask(json!({ "op": "keyRelease", "key": data })), json!({ "released": false }));
    assert_eq!(seal(&json!({ "$handle": n + 1000 })).unwrap_err(), "unknown_key");

    // Never reused: the next key gets a number that was never given out, so the stale handle reaches nothing.
    let next = import("data", &[9u8; 32]).unwrap().0["key"]["$handle"].as_u64().unwrap();
    assert!(next > n);
    assert_eq!(seal(&data).unwrap_err(), "unknown_key");

    // Bounded: the table takes 64 and refuses the next; releasing all empties it.
    while ask(json!({ "op": "keyCount" }))["count"] != 64 {
        import("signing", &[1u8; 32]).unwrap();
    }
    assert_eq!(import("signing", &[1u8; 32]).unwrap_err(), "too_many_keys");
    assert_eq!(ask(json!({ "op": "keyReleaseAll" })), json!({ "released": 64 }));
    assert_eq!(ask(json!({ "op": "keyCount" }))["count"], 0);
    assert_eq!(seal(&json!({ "$handle": next })).unwrap_err(), "unknown_key");
}

// @verifies SWR-MG-32#c
#[test]
fn a_passphrase_or_a_recovery_code_becomes_a_kept_key_in_one_call_and_its_secret_never_comes_out() {
    let _one = one();
    let typed = "ｃｏｒｒｅｃｔ ｈｏｒｓｅ ｂａｔｔｅｒｙ"; // full-width: the secret it stands for is the plain ASCII
    let k = kdf("passphrase");
    // The two-step way, which hands the normalised secret out and takes it in again.
    let stood_for = ask(json!({ "op": "passphraseSecret", "passphrase": typed }))["secret"].clone();
    let two_step = ask(json!({ "op": "deriveBundleKey", "secret": stood_for, "kdf": k }))["key"].clone();
    // One call: the passphrase in as bytes, the key kept.
    let (kept, beside, text) = ask_with(json!({ "op": "deriveBundleKey", "passphrase": { "$secret": 0 }, "kdf": k, "keep": "bundle" }), &[typed.as_bytes()]).unwrap();
    assert!(beside.is_empty());
    for held in [stood_for.as_str().unwrap(), two_step.as_str().unwrap(), "correct horse"] {
        assert!(!text.contains(&held[..12]), "the answer holds a secret: {text}");
    }
    let seal = |key: &Value| ask(json!({ "op": "sealBundle", "epoch": 1, "kind": "uhdr", "key": key, "nonce": NONCE, "items": {}, "kdf": k }));
    assert_eq!(seal(&kept["key"]), seal(&two_step), "the same key either way");

    // Too short is said, with the floor, and nothing is kept.
    let before = ask(json!({ "op": "keyCount" }))["count"].clone();
    let (short, _, _) = ask_with(json!({ "op": "deriveBundleKey", "passphrase": { "$secret": 0 }, "kdf": k, "keep": "bundle" }), &[b"short"]).unwrap();
    assert_eq!((short["ok"].clone(), short["reason"].clone(), short["minChars"].clone()), (json!(false), json!("too_short"), json!(12)));
    assert_eq!(ask(json!({ "op": "keyCount" }))["count"], before);

    // A recovery code the same way.
    let code = ask(json!({ "op": "encodeRecoveryCode", "bytes": "c7".repeat(32) }))["code"].as_str().unwrap().to_string();
    let rk = kdf("recoveryCode");
    let via_secret = ask(json!({ "op": "deriveBundleKey", "secret": "c7".repeat(32), "kdf": rk }))["key"].clone();
    let (kept, _, text) = ask_with(json!({ "op": "deriveBundleKey", "recoveryCode": { "$secret": 0 }, "kdf": rk, "keep": "bundle" }), &[code.as_bytes()]).unwrap();
    assert!(!text.contains("c7c7c7c7"));
    let seal = |key: &Value| ask(json!({ "op": "sealBundle", "epoch": 1, "kind": "uhdr", "key": key, "nonce": NONCE, "items": {}, "kdf": rk }));
    assert_eq!(seal(&kept["key"]), seal(&via_secret));
    let (bad, _, _) = ask_with(json!({ "op": "deriveBundleKey", "recoveryCode": { "$secret": 0 }, "kdf": rk, "keep": "bundle" }), &[b"ABCD"]).unwrap();
    assert_eq!(bad["ok"], false);
}

// @verifies SWR-MG-32#d
#[test]
fn a_key_is_wrapped_and_unwrapped_as_bytes_in_the_form_the_older_records_have() {
    let _one = one();
    let wrapping = [0x5au8; 32];
    // The older way: the data key's hex sealed as a value, and read back as text.
    let old = ask(json!({ "op": "sealValue", "key": hex(&wrapping), "nonce": NONCE, "name": "np.wrap", "value": hex(&KEY) }));
    let (wrapped, beside, _) = ask_with(json!({ "op": "wrapKey", "key": { "$secret": 0 }, "nonce": NONCE, "name": "np.wrap", "data": { "$secret": 1 } }), &[&wrapping, &KEY]).unwrap();
    assert_eq!(wrapped, old, "a record written either way is the same bytes");
    assert!(beside.is_empty());
    let (opened, beside, text) = ask_with(json!({ "op": "unwrapKey", "key": { "$secret": 0 }, "name": "np.wrap", "sealed": old["sealed"] }), &[&wrapping]).unwrap();
    assert_eq!(opened, json!({ "ok": true, "key": { "$secret": 0 } }));
    assert_eq!(beside[0], KEY);
    assert!(!text.contains(&hex(&KEY)[..16]), "the unwrapped key is in the answer's text: {text}");
    // A wrong key, another name and a value that is not a key are told apart as `openValue` tells them.
    let wrong = ask_with(json!({ "op": "unwrapKey", "key": { "$secret": 0 }, "name": "np.wrap", "sealed": old["sealed"] }), &[&[1u8; 32]]).unwrap().0;
    assert_eq!(wrong, json!({ "ok": false, "reason": "authentication_failed" }));
    let renamed = ask_with(json!({ "op": "unwrapKey", "key": { "$secret": 0 }, "name": "np.other", "sealed": old["sealed"] }), &[&wrapping]).unwrap().0;
    assert_eq!(renamed["reason"], "authentication_failed");
    let not_a_key = ask(json!({ "op": "sealValue", "key": hex(&wrapping), "nonce": NONCE, "name": "np.wrap", "value": "not hex" }));
    let r = ask_with(json!({ "op": "unwrapKey", "key": { "$secret": 0 }, "name": "np.wrap", "sealed": not_a_key["sealed"] }), &[&wrapping]).unwrap().0;
    assert_eq!(r, json!({ "ok": false, "reason": "malformed" }));
    // With a kept wrapping key: the one call a passphrase gate makes.
    let k = kdf("passphrase");
    let (kept, _, _) = ask_with(json!({ "op": "deriveBundleKey", "passphrase": { "$secret": 0 }, "kdf": k, "keep": "wrapping" }), &[b"correct horse battery"]).unwrap();
    let (w, _, _) = ask_with(json!({ "op": "wrapKey", "key": kept["key"], "nonce": NONCE, "name": "np.wrap", "data": { "$secret": 0 } }), &[&KEY]).unwrap();
    let (u, beside, _) = ask_with(json!({ "op": "unwrapKey", "key": kept["key"], "name": "np.wrap", "sealed": w["sealed"] }), &[]).unwrap();
    assert_eq!((u["ok"].clone(), beside[0].clone()), (json!(true), KEY.to_vec()));
    // A recovery code is made from secret bytes given beside the request.
    let (code, _, _) = ask_with(json!({ "op": "encodeRecoveryCode", "bytes": { "$secret": 0 } }), &[&[0xc7u8; 32]]).unwrap();
    assert_eq!(code, ask(json!({ "op": "encodeRecoveryCode", "bytes": "c7".repeat(32) })));
}
