//! `SWR-MG-34`: the superseded notice is composed, signed, verified and ordered by the core, once
//! (`OI-PRIV-68`, the principal, 2026-11-21). Through the public surface every platform uses
//! (`migration_json`), and the pure functions the operations are built from.

use neurone_npps_core::current_device::{accepts, message, next_count};
use neurone_npps_core::migration::migration_json;
use serde_json::{json, Value};
use sha2::{Digest, Sha256};

type R<T> = Result<T, String>;

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn unhex(s: &str, _what: &str) -> R<Vec<u8>> {
    if s.len() % 2 != 0 {
        return Err("odd".into());
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).map_err(|e| e.to_string())).collect()
}

fn is_key_id(s: &str) -> bool {
    s.len() == 64 && s.bytes().all(|b| matches!(b, b'0'..=b'9' | b'a'..=b'f'))
}


fn ask(v: Value) -> R<Value> {
    migration_json(&v.to_string()).map(|s| serde_json::from_str(&s).unwrap())
}

const SEED: [u8; 32] = [7u8; 32];

fn key() -> (String, String) {
    let r = ask(json!({ "op": "currentDeviceKey", "seed": hex(&SEED) })).unwrap();
    (r["publicKey"].as_str().unwrap().to_string(), r["keyId"].as_str().unwrap().to_string())
}

fn sign(count: u64) -> Value {
    ask(json!({ "op": "supersededNoticeSign", "seed": hex(&SEED), "count": count })).unwrap()
}

// @verifies SWR-MG-34#a
#[test]
fn the_signed_bytes_are_exactly_the_form() {
    let id = "a".repeat(64);
    assert_eq!(message(&id, 3), format!("NP-SUPERSEDED-V1\n64:{id}\n1:3\n").into_bytes());
    assert_eq!(message(&id, 120), format!("NP-SUPERSEDED-V1\n64:{id}\n3:120\n").into_bytes());
    // A count has no leading zeros, and a field's length is its byte length.
    assert!(!String::from_utf8(message(&id, 7)).unwrap().contains(":07"));
}

// @verifies SWR-MG-34#a
#[test]
fn the_key_id_is_the_sha256_of_the_public_key_in_lowercase_hex() {
    let (public, id) = key();
    let bytes: [u8; 32] = unhex(&public, "publicKey").unwrap().try_into().unwrap();
    assert_eq!(id, hex(&Sha256::digest(bytes)));
    assert!(is_key_id(&id));
    assert!(!is_key_id(&id.to_uppercase()));
}

// @verifies SWR-MG-34#b
#[test]
fn a_notice_is_signed_by_the_key_the_seed_gives() {
    let (public, id) = key();
    let n = sign(2);
    assert_eq!(n["keyId"], id);
    let ok = ask(json!({ "op": "supersededNoticeVerify", "publicKey": public, "keyId": id, "count": 2, "signature": n["signature"] })).unwrap();
    assert_eq!(ok["ok"], true);
}

// @verifies SWR-MG-34#b
#[test]
fn a_signing_call_without_a_usable_seed_is_refused() {
    assert!(ask(json!({ "op": "supersededNoticeSign", "count": 1 })).is_err());
    assert!(ask(json!({ "op": "supersededNoticeSign", "seed": hex(&[1u8; 5]), "count": 1 })).is_err());
}

// @verifies SWR-MG-34#c
#[test]
fn an_altered_or_forged_notice_is_refused() {
    let (public, id) = key();
    let n = sign(2);
    let sig = n["signature"].as_str().unwrap().to_string();
    let v = |key: &str, id: &str, count: u64, sig: &str| {
        ask(json!({ "op": "supersededNoticeVerify", "publicKey": key, "keyId": id, "count": count, "signature": sig })).unwrap()["ok"] == true
    };
    assert!(v(&public, &id, 2, &sig));
    assert!(!v(&public, &id, 3, &sig), "another count");
    let mut other = [0u8; 32];
    other.copy_from_slice(&unhex(&public, "publicKey").unwrap());
    other[0] ^= 1;
    assert!(!v(&hex(&other), &id, 2, &sig), "another key");
    let wrong_id = "b".repeat(64);
    assert!(!v(&public, &wrong_id, 2, &sig), "a key id that is not this key's");
    assert!(!v(&public, &id.to_uppercase(), 2, &sig), "a key id not in lowercase");
    let mut bad = unhex(&sig, "signature").unwrap();
    bad[10] ^= 1;
    assert!(!v(&public, &id, 2, &hex(&bad)), "a flipped signature bit");
    assert!(!v(&public, &id, 2, &hex(&bad[..63])), "a short signature");
    assert!(!v("zz", &id, 2, &sig), "a key that is not hex");
}

// @verifies SWR-MG-34#c
#[test]
fn a_signature_for_another_form_is_not_a_notice() {
    // The same key signing different bytes must not verify: the domain line is what separates the forms.
    let (public, id) = key();
    let other = ask(json!({ "op": "ed25519Sign", "seed": hex(&SEED), "message": hex(format!("{id}\n2\n").as_bytes()) })).unwrap();
    let ok = ask(json!({ "op": "supersededNoticeVerify", "publicKey": public, "keyId": id, "count": 2, "signature": other["signature"] })).unwrap();
    assert_eq!(ok["ok"], false);
}

// @verifies SWR-MG-34#d
#[test]
fn a_device_takes_only_a_higher_count() {
    assert!(accepts(None, 0));
    assert!(accepts(Some(4), 5));
    assert!(!accepts(Some(4), 4), "an equal count changes nothing");
    assert!(!accepts(Some(4), 3), "an older notice delayed in the network changes nothing");
    let a = |known: Value, count: u64| ask(json!({ "op": "supersededNoticeAccepts", "knownCount": known, "count": count })).unwrap()["accept"] == true;
    assert!(a(Value::Null, 1));
    assert!(a(json!(4), 5));
    assert!(!a(json!(4), 4));
}

// @verifies SWR-MG-34#d
#[test]
fn the_count_a_move_signs_is_one_higher_and_a_count_at_the_top_is_refused() {
    assert_eq!(next_count(0), Some(1));
    assert_eq!(next_count(41), Some(42));
    assert_eq!(next_count(u64::MAX), None);
    assert_eq!(ask(json!({ "op": "supersededNextCount", "highestKnown": 9 })).unwrap()["count"], 10);
    assert!(ask(json!({ "op": "supersededNextCount", "highestKnown": u64::MAX })).is_err());
}

// ── Catching up (`SWR-MG-35`; the principal, 2026-11-21): the cursor, retirement and the extract gate ──

const OTHER_SEED: [u8; 32] = [9u8; 32];

fn key_of(seed: [u8; 32]) -> (String, String) {
    let r = ask(json!({ "op": "currentDeviceKey", "seed": hex(&seed) })).unwrap();
    (r["publicKey"].as_str().unwrap().to_string(), r["keyId"].as_str().unwrap().to_string())
}

fn notice(seed: [u8; 32], sequence: u64, count: u64) -> Value {
    let n = ask(json!({ "op": "supersededNoticeSign", "seed": hex(&seed), "count": count })).unwrap();
    json!({ "sequence": sequence, "keyId": n["keyId"], "count": count, "signature": n["signature"] })
}

fn catch_up(cursor: u64, latest: u64, held: Value, notices: Value) -> Value {
    ask(json!({ "op": "supersededCatchUp", "cursor": cursor, "latest": latest, "held": held, "notices": notices })).unwrap()
}

// @verifies SWR-MG-35#a
#[test]
fn the_cursor_moves_to_the_highest_sequence_after_it_and_never_back() {
    let (public, _) = key();
    let held = json!([{ "publicKey": public, "knownCount": 3 }]);
    // Notices for other people's keys still move the cursor: the device processed them.
    let page = json!([notice(OTHER_SEED, 12, 1), notice(OTHER_SEED, 11, 2)]);
    assert_eq!(catch_up(10, 12, held.clone(), page)["cursor"], 12);
    // A page of notices at or below the cursor changes nothing; the cursor does not go back.
    let stale = json!([notice(SEED, 4, 9), notice(OTHER_SEED, 7, 1)]);
    let r = catch_up(10, 12, held, stale);
    assert_eq!(r["cursor"], 10);
    assert_eq!(r["held"][0]["retired"], false, "a notice at or below the cursor was already processed");
}

// @verifies SWR-MG-35#a
#[test]
fn a_notice_that_does_not_verify_moves_the_cursor_and_retires_no_one() {
    let (public, id) = key();
    let held = json!([{ "publicKey": public, "knownCount": 3 }]);
    let mut forged = notice(SEED, 21, 9);
    forged["count"] = json!(10); // signed for 9, claims 10
    let junk = json!({ "sequence": 22, "keyId": id, "count": 50, "signature": hex(&[0u8; 64]) });
    let malformed = json!({ "keyId": id, "count": 50 });
    let r = catch_up(20, 22, held, json!([forged, junk, malformed]));
    assert_eq!(r["cursor"], 22, "junk cannot stall a device");
    assert_eq!(r["held"][0]["retired"], false);
    assert_eq!(r["held"][0]["knownCount"], 3);
    assert_eq!(r["mayExtract"], true);
}

// @verifies SWR-MG-35#b
#[test]
fn a_verified_higher_count_retires_the_person_and_records_the_count() {
    let (public, id) = key();
    let (other_public, other_id) = key_of(OTHER_SEED);
    let held = json!([{ "publicKey": public, "knownCount": 3 }, { "publicKey": other_public, "knownCount": 1 }]);
    // Two notices for the first person in one page, out of order: the highest that verifies is taken.
    let page = json!([notice(SEED, 33, 5), notice(SEED, 31, 4), notice(OTHER_SEED, 32, 1)]);
    let r = catch_up(30, 33, held, page);
    assert_eq!(r["held"][0], json!({ "keyId": id, "knownCount": 5, "retired": true }));
    assert_eq!(r["held"][1], json!({ "keyId": other_id, "knownCount": 1, "retired": false }), "an equal count is this device's own move");
}

// @verifies SWR-MG-35#b
#[test]
fn an_older_or_equal_count_retires_no_one() {
    let (public, _) = key();
    let held = json!([{ "publicKey": public, "knownCount": 6 }]);
    let r = catch_up(0, 2, held, json!([notice(SEED, 1, 6), notice(SEED, 2, 5)]));
    assert_eq!(r["held"][0]["retired"], false);
    assert_eq!(r["held"][0]["knownCount"], 6);
}

// @verifies SWR-MG-35#c
#[test]
fn a_device_behind_the_latest_sequence_may_not_extract() {
    use neurone_npps_core::current_device::may_extract;
    assert!(may_extract(7, 7));
    assert!(may_extract(8, 7));
    assert!(!may_extract(6, 7));
    let (public, _) = key();
    let held = json!([{ "publicKey": public, "knownCount": 1 }]);
    // One page of two: the cursor is still behind the latest, so nothing may be extracted yet.
    let first = catch_up(0, 4, held.clone(), json!([notice(OTHER_SEED, 1, 1), notice(OTHER_SEED, 2, 2)]));
    assert_eq!(first["mayExtract"], false);
    let second = catch_up(first["cursor"].as_u64().unwrap(), 4, held, json!([notice(OTHER_SEED, 3, 3), notice(OTHER_SEED, 4, 4)]));
    assert_eq!(second["mayExtract"], true);
}

// @verifies SWR-MG-35#a
#[test]
fn a_catch_up_call_missing_its_cursor_or_lists_is_refused() {
    let (public, _) = key();
    let held = json!([{ "publicKey": public, "knownCount": 1 }]);
    assert!(ask(json!({ "op": "supersededCatchUp", "latest": 1, "held": held, "notices": [] })).is_err());
    assert!(ask(json!({ "op": "supersededCatchUp", "cursor": 0, "held": held, "notices": [] })).is_err());
    assert!(ask(json!({ "op": "supersededCatchUp", "cursor": 0, "latest": 1, "notices": [] })).is_err());
    assert!(ask(json!({ "op": "supersededCatchUp", "cursor": 0, "latest": 1, "held": [{ "publicKey": "00" }], "notices": [] })).is_err());
}
