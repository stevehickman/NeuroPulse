//! A stored value, sealed under a key the platform keeps (`SWR-MG-06`; issue #569 package 6r, `CLAUDE.md` §20).
//!
//! `SWR-MG-06` says the UHDR-class state the app holds is encrypted at rest under a key in the platform keystore, so that
//! destroying the key makes it unreadable. The key store is each platform's; the sealing is here, once, so that a value one
//! platform wrote is the same bytes another reads and no app carries a second implementation. A value is sealed with
//! AES-256-GCM under a random 12-byte nonce, and the value's own name is the associated data, so a sealed value copied under
//! another name does not open. The key is handed in for one call as bytes beside the request, or named by the handle of a key the core keeps (`secret_io`, `SWR-MG-32`); the nonce is handed in for one call.
//!
//! | operation | arguments | result |
//! |-----------|-----------|--------|
//! | `sealValue` | `key` (32 bytes, hex), `nonce` (12 bytes, hex), `name`, `value` (text) | `{sealed}`: the nonce and the ciphertext, hex |
//! | `wrapKey` | `key` (the wrapping key), `nonce`, `name`, `data` (the key to wrap: bytes beside the request, or a kept `data` key) | `{sealed}`, the same form `sealValue` gives the key's hex |
//! | `unwrapKey` | `key`, `name`, `sealed` | `{ok: true, key}` with the key as a secret of the answer, or `{ok: false, reason}` as `openValue` |
//! | `openValue` | `key`, `name`, `sealed` | `{ok: true, value}`, or `{ok: false, reason}` with `authentication_failed` (a wrong key, a different name or an edited value) or `malformed` |
//! | `keySeal` | `key` (a `data` key only), `nonce`, `name`, `bundle` (a kept `bundle` key, `{"$handle": n}`, and nothing else) | `{sealed}` |
//! | `keyUnseal` | `key` (a `data` key only), `name`, `sealed` | `{ok: true, key: {"$handle": n}}`, the key kept again under `bundle`, or `{ok: false, reason}` as `openValue` |
//!
//! # A kept backup key, sealed (`SWR-MG-32` (i); `OI-PRIV-65` (2), the principal, 2026-11-24)
//! The automatic backup must run with no one to type the passphrase or recovery code. It keeps a key derived from the secret
//! instead of the secret, so the secret a person may reuse elsewhere is never stored. The key is one the core derived and keeps
//! (`deriveBundleKey` with `"keep": "bundle"`), and no operation gives a kept key back as bytes or hex (`secret_io` rule 3).
//! `keySeal` is the one way it leaves the core: AES-256-GCM under the data key of the store that will hold it, with its own
//! associated-data label, so a sealed key is never read as a value (`openValue`) or as a wrapped data key (`unwrapKey`), and
//! the reverse. `keyUnseal` brings it back as a kept key with a new handle, never as bytes. Only a `bundle` key can be sealed
//! and only a `data` key can seal: a wrapping key (the passphrase gate's) cannot leave, and a backup key cannot seal.

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

/// The cipher for the request's key. The decoded key is wiped when this returns, and the cipher wipes its own copy when it
/// is dropped (the `zeroize` feature of `aes-gcm`), `SWR-MG-28` (c).
fn cipher(req: &Value) -> R<Aes256Gcm> {
    cipher_of(req, &["data", "wrapping"])
}

/// As [`cipher`], taking a kept key only of one of `purposes`.
fn cipher_of(req: &Value, purposes: &[&str]) -> R<Aes256Gcm> {
    let key = crate::secret_io::secret(req, "key", purposes)?;
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

/// `wrapKey`: seal a key under another (the data key under the key a passphrase makes). The stored form is exactly what
/// `sealValue` makes of the key's hex, so a record written before this opens with `unwrapKey` and the reverse; what
/// changes is that the key being wrapped arrives as bytes (`SWR-MG-32`) and its hex exists only here, in a wiped buffer.
fn wrap_key(req: &Value) -> R<Value> {
    let name = req["name"].as_str().ok_or("name is required")?;
    let nonce = unhex(req["nonce"].as_str().ok_or("nonce is required")?, "nonce")?;
    if nonce.len() != NONCE_LEN {
        return Err(format!("the nonce is {NONCE_LEN} bytes"));
    }
    let data = crate::secret_io::secret(req, "data", &["data"])?;
    if data.is_empty() {
        return Err("the key to wrap is empty".into());
    }
    let mut text = zeroize::Zeroizing::new(String::with_capacity(2 * data.len()));
    for b in data.iter() {
        text.push(char::from_digit(u32::from(b >> 4), 16).expect("a nibble"));
        text.push(char::from_digit(u32::from(b & 15), 16).expect("a nibble"));
    }
    let ct = cipher(req)?
        .encrypt(Nonce::from_slice(&nonce), Payload { msg: text.as_bytes(), aad: &aad(name) })
        .map_err(|_| "encryption failed".to_string())?;
    let mut sealed = nonce;
    sealed.extend(ct);
    Ok(json!({ "sealed": hex(&sealed) }))
}

/// `unwrapKey`: the key `wrapKey` (or, before it, `sealValue` over the key's hex) sealed, as a secret of the answer: bytes
/// beside it through the secret entry point, and never the text `openValue` would have answered with.
fn unwrap_key(req: &Value) -> R<Value> {
    let name = req["name"].as_str().ok_or("name is required")?;
    let cipher = cipher(req)?;
    let Some(sealed) = req["sealed"].as_str().and_then(|s| unhex(s, "sealed").ok()).filter(|s| s.len() > NONCE_LEN) else {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    };
    let (nonce, ct) = sealed.split_at(NONCE_LEN);
    let Ok(plain) = cipher.decrypt(Nonce::from_slice(nonce), Payload { msg: ct, aad: &aad(name) }) else {
        return Ok(json!({ "ok": false, "reason": "authentication_failed" }));
    };
    let plain = zeroize::Zeroizing::new(plain);
    let key = std::str::from_utf8(&plain).ok().and_then(|text| unhex(text, "key").ok()).map(zeroize::Zeroizing::new);
    Ok(match key {
        Some(key) if !key.is_empty() => json!({ "ok": true, "key": crate::secret_io::answer_secret(&key) }),
        _ => json!({ "ok": false, "reason": "malformed" }),
    })
}

/// The associated data of a sealed kept key: a label of its own, then the name it is stored under.
fn kept_aad(name: &str) -> Vec<u8> {
    let mut a = b"np.kept-key.v1:".to_vec();
    a.extend_from_slice(name.as_bytes());
    a
}

/// `keySeal`: a kept `bundle` key, sealed under a `data` key (the module text). The key is read from the table straight into a
/// wiped buffer and is never in the request or the answer.
fn key_seal(req: &Value) -> R<Value> {
    let name = req["name"].as_str().ok_or("name is required")?;
    let nonce = unhex(req["nonce"].as_str().ok_or("nonce is required")?, "nonce")?;
    if nonce.len() != NONCE_LEN {
        return Err(format!("the nonce is {NONCE_LEN} bytes"));
    }
    let kept = crate::secret_io::kept(req, "bundle", &["bundle"])?;
    let ct = cipher_of(req, &["data"])?
        .encrypt(Nonce::from_slice(&nonce), Payload { msg: kept.as_slice(), aad: &kept_aad(name) })
        .map_err(|_| "encryption failed".to_string())?;
    let mut sealed = nonce;
    sealed.extend(ct);
    Ok(json!({ "sealed": hex(&sealed) }))
}

/// `keyUnseal`: the key `keySeal` sealed, kept again under `bundle`; the answer holds its handle and none of it.
fn key_unseal(req: &Value) -> R<Value> {
    let name = req["name"].as_str().ok_or("name is required")?;
    let cipher = cipher_of(req, &["data"])?;
    let Some(sealed) = req["sealed"].as_str().and_then(|s| unhex(s, "sealed").ok()).filter(|s| s.len() > NONCE_LEN) else {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    };
    let (nonce, ct) = sealed.split_at(NONCE_LEN);
    let Ok(plain) = cipher.decrypt(Nonce::from_slice(nonce), Payload { msg: ct, aad: &kept_aad(name) }) else {
        return Ok(json!({ "ok": false, "reason": "authentication_failed" }));
    };
    let key = zeroize::Zeroizing::new(plain);
    if key.len() != KEY_LEN {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    }
    Ok(json!({ "ok": true, "key": crate::secret_io::keep("bundle", key)? }))
}

/// `sealValue`, `openValue`, `wrapKey`, `unwrapKey`, `keySeal` and `keyUnseal`.
pub(crate) fn op(name: &str, req: &Value) -> Option<R<Value>> {
    match name {
        "keySeal" => Some(key_seal(req)),
        "keyUnseal" => Some(key_unseal(req)),
        "wrapKey" => Some(wrap_key(req)),
        "unwrapKey" => Some(unwrap_key(req)),
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
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
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
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
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
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_value_too_short_to_hold_a_nonce_or_not_hex_is_malformed_and_a_bad_key_or_nonce_is_an_error() {
        assert_eq!(opened("np.a", "abcd", KEY)["reason"], "malformed");
        assert_eq!(opened("np.a", "zz", KEY)["reason"], "malformed");
        assert!(op("sealValue", &json!({ "key": "00", "nonce": NONCE, "name": "n", "value": "v" })).unwrap().is_err());
        assert!(op("sealValue", &json!({ "key": KEY, "nonce": "00", "name": "n", "value": "v" })).unwrap().is_err());
        assert!(op("somethingElse", &json!({})).is_none());
    }
}
