//! A stored value, sealed under a key the platform keeps (`SWR-MG-06`; issue #569 package 6r, `CLAUDE.md` §20).
//!
//! `SWR-MG-06` says the UHDR-class state the app holds is encrypted at rest under a key in the platform keystore, so that
//! destroying the key makes it unreadable. The key store is each platform's; the sealing is here, once, so that a value one
//! platform wrote is the same bytes another reads and no app carries a second implementation. A value is sealed with
//! AES-256-GCM under a random 12-byte nonce, and the value's own name is the associated data, so a sealed value copied under
//! another name does not open. Nothing is kept: the key and the nonce are handed in for one call.
//!
//! | operation | arguments | result |
//! |-----------|-----------|--------|
//! | `sealValue` | `key` (32 bytes, hex), `nonce` (12 bytes, hex), `name`, `value` (text) | `{sealed}`: the nonce and the ciphertext, hex |
//! | `openValue` | `key`, `name`, `sealed` | `{ok: true, value}`, or `{ok: false, reason}` with `authentication_failed` (a wrong key, a different name or an edited value) or `malformed` |

use crate::migration::{hex, unhex};
use aes_gcm::aead::{Aead, KeyInit, Payload};
use aes_gcm::{Aes256Gcm, Nonce};
use serde_json::{json, Value};

type R<T> = Result<T, String>;

const NONCE_LEN: usize = 12;
const KEY_LEN: usize = 32;

/// The associated data: a label that names this format, then the value's name.
fn aad(name: &str) -> Vec<u8> {
    let mut a = b"np.at-rest.v1:".to_vec();
    a.extend_from_slice(name.as_bytes());
    a
}

fn cipher(req: &Value) -> R<Aes256Gcm> {
    let key = unhex(req["key"].as_str().ok_or("key is required")?, "key")?;
    if key.len() != KEY_LEN {
        return Err(format!("the key is {KEY_LEN} bytes"));
    }
    Aes256Gcm::new_from_slice(&key).map_err(|_| "the key is not usable".to_string())
}

fn seal(req: &Value) -> R<Value> {
    let name = req["name"].as_str().ok_or("name is required")?;
    let value = req["value"].as_str().ok_or("value is required")?;
    let nonce = unhex(req["nonce"].as_str().ok_or("nonce is required")?, "nonce")?;
    if nonce.len() != NONCE_LEN {
        return Err(format!("the nonce is {NONCE_LEN} bytes"));
    }
    let ct = cipher(req)?
        .encrypt(Nonce::from_slice(&nonce), Payload { msg: value.as_bytes(), aad: &aad(name) })
        .map_err(|_| "encryption failed".to_string())?;
    let mut sealed = nonce;
    sealed.extend(ct);
    Ok(json!({ "sealed": hex(&sealed) }))
}

fn open(req: &Value) -> R<Value> {
    let name = req["name"].as_str().ok_or("name is required")?;
    let cipher = cipher(req)?;
    let Some(sealed) = req["sealed"].as_str().and_then(|s| unhex(s, "sealed").ok()).filter(|s| s.len() > NONCE_LEN) else {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    };
    let (nonce, ct) = sealed.split_at(NONCE_LEN);
    let Ok(plain) = cipher.decrypt(Nonce::from_slice(nonce), Payload { msg: ct, aad: &aad(name) }) else {
        return Ok(json!({ "ok": false, "reason": "authentication_failed" }));
    };
    match String::from_utf8(plain) {
        Ok(value) => Ok(json!({ "ok": true, "value": value })),
        Err(_) => Ok(json!({ "ok": false, "reason": "malformed" })),
    }
}

/// `sealValue` and `openValue`.
pub(crate) fn op(name: &str, req: &Value) -> Option<R<Value>> {
    match name {
        "sealValue" => Some(seal(req)),
        "openValue" => Some(open(req)),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const KEY: &str = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    const NONCE: &str = "a0a1a2a3a4a5a6a7a8a9aaab";

    fn sealed(name: &str, value: &str, key: &str) -> String {
        op("sealValue", &json!({ "key": key, "nonce": NONCE, "name": name, "value": value })).unwrap().unwrap()["sealed"].as_str().unwrap().to_string()
    }

    fn opened(name: &str, sealed: &str, key: &str) -> Value {
        op("openValue", &json!({ "key": key, "name": name, "sealed": sealed })).unwrap().unwrap()
    }

    // @verifies SWR-MG-06
    #[test]
    fn a_sealed_value_opens_under_its_key_and_its_name_and_the_text_is_not_in_it() {
        let s = sealed("np.session.history", "history of the person", KEY);
        let plain_hex = hex(b"history of the person");
        assert!(!s.contains(&plain_hex), "the value is in the clear");
        assert_eq!(s.len(), 2 * (NONCE_LEN + "history of the person".len() + 16), "nonce, ciphertext and a 16-byte tag");
        assert_eq!(opened("np.session.history", &s, KEY), json!({ "ok": true, "value": "history of the person" }));
        assert_eq!(opened("np.empty", &sealed("np.empty", "", KEY), KEY), json!({ "ok": true, "value": "" }));
    }

    // @verifies SWR-MG-06
    #[test]
    fn a_wrong_key_another_name_or_an_edited_value_does_not_open() {
        let s = sealed("np.a", "value", KEY);
        let other_key = "ff".repeat(32);
        assert_eq!(opened("np.a", &s, &other_key)["reason"], "authentication_failed", "a destroyed key is a wrong key");
        assert_eq!(opened("np.b", &s, KEY)["reason"], "authentication_failed", "a value copied under another name");
        let mut edited = s.clone().into_bytes();
        let last = edited.len() - 1;
        edited[last] = if edited[last] == b'0' { b'1' } else { b'0' };
        assert_eq!(opened("np.a", &String::from_utf8(edited).unwrap(), KEY)["reason"], "authentication_failed");
    }

    // @verifies SWR-MG-06
    #[test]
    fn a_value_too_short_to_hold_a_nonce_or_not_hex_is_malformed_and_a_bad_key_or_nonce_is_an_error() {
        assert_eq!(opened("np.a", "abcd", KEY)["reason"], "malformed");
        assert_eq!(opened("np.a", "zz", KEY)["reason"], "malformed");
        assert!(op("sealValue", &json!({ "key": "00", "nonce": NONCE, "name": "n", "value": "v" })).unwrap().is_err());
        assert!(op("sealValue", &json!({ "key": KEY, "nonce": "00", "name": "n", "value": "v" })).unwrap().is_err());
        assert!(op("somethingElse", &json!({})).is_none());
    }
}
