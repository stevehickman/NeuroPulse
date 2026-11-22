//! The superseded notice (`OI-PRIV-68`, `SWR-MG-34`; `NP-SW-PORTAL-API-001` §6a).
//!
//! When a person moves to a new device, the new device publishes a signed notice naming the key id of the person's
//! current-device key and a count that rises with each move. Every other device that holds that key checks the
//! notice against the public key it already has, and stops being current for the person if the count is higher than
//! the one it knows. The server holds no key and verifies nothing, so this file is the only place the form, the
//! signature and the ordering are decided, and every platform reaches it through `migration_json`.
//!
//! The signed bytes are length-prefixed, not delimited (`NP-SW-PORTAL-API-001` §4a gives the reason), and the first
//! line is the domain separator:
//!
//! ```text
//! NP-SUPERSEDED-V1\n
//! <byte length>:<key id: SHA-256 of the public key, 64 lowercase hex digits>\n
//! <byte length>:<count, decimal, no leading zeros>\n
//! ```
//!
//! | `op` | request | answer |
//! |------|---------|--------|
//! | `currentDeviceKey` | `seed` (a secret, 32 bytes) | `{publicKey, keyId}` |
//! | `supersededNoticeSign` | `seed` (a secret), `count` | `{keyId, count, signature}` (64 bytes, hex) |
//! | `supersededNoticeVerify` | `publicKey`, `keyId`, `count`, `signature` (hex) | `{ok}`; anything wrong is `ok: false`, never an error |
//! | `supersededNoticeAccepts` | `knownCount` (a number or `null`), `count` | `{accept}` |
//! | `supersededNextCount` | `highestKnown` | `{count}`, or an error when it cannot be raised |
//!
//! The seed is the person's current-device key and lives in the UHDR. It reaches the core as a secret (`secret_io`),
//! never as text, and this module holds no copy of it.

use crate::migration::{hex, unhex};
use ed25519_dalek::{Signature, Signer, VerifyingKey};
use serde_json::{json, Value};
use sha2::{Digest, Sha256};

type R<T> = Result<T, String>;

/// The domain separator and form name.
pub const FORM: &str = "NP-SUPERSEDED-V1";

/// The key id of a public key: SHA-256, as 64 lowercase hex digits.
pub fn key_id(public_key: &[u8; 32]) -> String {
    hex(&Sha256::digest(public_key))
}

fn is_key_id(s: &str) -> bool {
    s.len() == 64 && s.bytes().all(|b| matches!(b, b'0'..=b'9' | b'a'..=b'f'))
}

/// The bytes that are signed. Length-prefixed: a field is `<byte length>:<value>\n`.
pub fn message(key_id: &str, count: u64) -> Vec<u8> {
    let count = count.to_string();
    format!("{FORM}\n{}:{key_id}\n{}:{count}\n", key_id.len(), count.len()).into_bytes()
}

/// Whether a device takes a notice: it knows no count for the key id, or the notice's is higher.
pub fn accepts(known: Option<u64>, count: u64) -> bool {
    known.map_or(true, |k| count > k)
}

/// The count a move signs: one higher than the highest known. `None` when it cannot be raised.
pub fn next_count(highest_known: u64) -> Option<u64> {
    highest_known.checked_add(1)
}

/// A notice verifies only if its key id is the SHA-256 of the key the device holds and the signature is valid
/// under the strict check.
pub fn verify(public_key: &[u8], key_id_text: &str, count: u64, signature: &[u8]) -> bool {
    let (Ok(public_key), Ok(signature)) = (<[u8; 32]>::try_from(public_key), <[u8; 64]>::try_from(signature)) else {
        return false;
    };
    if !is_key_id(key_id_text) || key_id(&public_key) != key_id_text {
        return false;
    }
    let Ok(key) = VerifyingKey::from_bytes(&public_key) else { return false };
    key.verify_strict(&message(key_id_text, count), &Signature::from_bytes(&signature)).is_ok()
}

fn count_of(req: &Value, name: &str) -> R<u64> {
    req[name].as_u64().ok_or_else(|| format!("{name} is required and is a whole number"))
}

fn hex_field(req: &Value, name: &str) -> R<Vec<u8>> {
    unhex(req[name].as_str().ok_or_else(|| format!("{name} is required"))?, name)
}

/// The operations of `migration_json` that belong to the superseded notice.
pub(crate) fn op(name: &str, req: &Value) -> Option<R<Value>> {
    Some(match name {
        "currentDeviceKey" => crate::primitives::seed_key(req).map(|k| {
            let public = *k.verifying_key().as_bytes();
            json!({ "publicKey": hex(&public), "keyId": key_id(&public) })
        }),
        "supersededNoticeSign" => (|| {
            let key = crate::primitives::seed_key(req)?;
            let count = count_of(req, "count")?;
            let id = key_id(key.verifying_key().as_bytes());
            let signature = key.sign(&message(&id, count)).to_bytes();
            Ok(json!({ "keyId": id, "count": count, "signature": hex(&signature) }))
        })(),
        "supersededNoticeVerify" => (|| {
            let ok = match (hex_field(req, "publicKey"), hex_field(req, "signature"), req["keyId"].as_str(), req["count"].as_u64()) {
                (Ok(key), Ok(sig), Some(id), Some(count)) => verify(&key, id, count, &sig),
                _ => false,
            };
            Ok(json!({ "ok": ok }))
        })(),
        "supersededNoticeAccepts" => (|| {
            let known = if req["knownCount"].is_null() { None } else { Some(count_of(req, "knownCount")?) };
            Ok(json!({ "accept": accepts(known, count_of(req, "count")?) }))
        })(),
        "supersededNextCount" => (|| {
            let next = next_count(count_of(req, "highestKnown")?).ok_or("a count at the top of its range cannot be raised")?;
            Ok(json!({ "count": next }))
        })(),
        _ => return None,
    })
}
