//! A secret given as text is refused when the core is built with `refuse-hex-keys`, and read as before when it is not
//! (`SWR-MG-32` (h); `OI-PRIV-12` stage 2, the principal, 2026-11-19). The switch is **off** in every shipped build until the
//! Swift and Windows apps are deleted (`OI-UI-KMP-05`); run these with and without `--features refuse-hex-keys`.

use neurone_npps_core::api::{migration_json, migration_secret_json};
use serde_json::{json, Value};

const ON: bool = cfg!(feature = "refuse-hex-keys");
const NONCE: &str = "a0a1a2a3a4a5a6a7a8a9aaab";

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn ask(v: Value) -> Result<Value, String> {
    migration_json(&v.to_string()).map(|s| serde_json::from_str(&s).unwrap())
}

fn beside(v: Value, secret: &[u8]) -> Result<Value, String> {
    let mut framed = (secret.len() as u32).to_be_bytes().to_vec();
    framed.extend_from_slice(secret);
    migration_secret_json(&v.to_string(), &framed).map(|(s, _)| serde_json::from_str(&s).unwrap())
}

fn refused(r: &Result<Value, String>) -> bool {
    r.as_ref().err().is_some_and(|e| e.starts_with("hex_key_refused"))
}

// @verifies SWR-MG-32#h
#[test]
fn a_key_as_hex_text_is_refused_only_when_the_switch_is_on() {
    let key = [7u8; 32];
    let seal = |k: Value| ask(json!({ "op": "sealValue", "key": k, "nonce": NONCE, "name": "np.n", "value": "x" }));
    let as_text = seal(json!(hex(&key)));
    assert_eq!(refused(&as_text), ON, "{as_text:?}");
    assert_eq!(as_text.is_ok(), !ON);
    // The byte form works either way, and so does a kept key.
    assert!(beside(json!({ "op": "sealValue", "key": { "$secret": 0 }, "nonce": NONCE, "name": "np.n", "value": "x" }), &key).is_ok());
}

// @verifies SWR-MG-32#h
#[test]
fn a_passphrase_as_text_are_refused_only_when_the_switch_is_on() {
    let pass = ask(json!({ "op": "passphraseSecret", "passphrase": "correct horse battery staple" }));
    if ON {
        // A refusal is named; it is not reported as "too short" or "malformed", which would send a host looking elsewhere.
        let pass = pass.unwrap();
        assert_eq!(pass["ok"], false);
        assert!(pass["reason"].as_str().unwrap().starts_with("hex_key_refused"), "{pass}");
    } else {
        assert_eq!(pass.unwrap()["ok"], true);
    }
    let bytes = beside(json!({ "op": "passphraseSecret", "passphrase": { "$secret": 0 } }), b"correct horse battery staple").unwrap();
    assert_eq!(bytes["ok"], true);
}

// @verifies SWR-MG-32#h
#[test]
fn the_values_that_are_not_secrets_stay_hex() {
    // A nonce is hex in both builds: only secrets are refused.
    let key = [9u8; 32];
    let sealed = beside(json!({ "op": "sealValue", "key": { "$secret": 0 }, "nonce": NONCE, "name": "np.n", "value": "x" }), &key).unwrap();
    assert!(sealed["sealed"].is_string());
}
