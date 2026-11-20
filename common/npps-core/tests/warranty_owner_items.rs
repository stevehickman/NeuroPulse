//! `REQ-SHDR-10` (b), (c): the warranty owner's records are not a user's, so neither kind of user bundle carries them, and a
//! bundle an earlier version sealed with the enrolment in it opens without it (the principal, 2026-11-20, `OI-PRIV-04`).
//! Through the public JSON surface every platform uses.

use aes_gcm::aead::{Aead, KeyInit, Payload};
use aes_gcm::{Aes256Gcm, Nonce};
use neurone_npps_core::migration::{migration_json, FORMAT_VERSION};
use serde_json::{json, Value};

const KEY: [u8; 32] = [0x42; 32];
const NONCE: [u8; 12] = [0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab];

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn ask(v: Value) -> Value {
    serde_json::from_str(&migration_json(&v.to_string()).expect("the core answers")).unwrap()
}

fn kdf() -> Value {
    ask(json!({"op":"bundleKdf","method":"recoveryCode","salt":"5a".repeat(32)}))["kdf"].clone()
}

fn seal(kind: &str, items: Value) -> Result<String, String> {
    migration_json(&json!({"op":"sealBundle","epoch":1,"kind":kind,"key":hex(&KEY),"nonce":hex(&NONCE),"items":items,"kdf":kdf()}).to_string())
}

/// A bundle as a version before `REQ-SHDR-10` sealed it: the bytes `sealBundle` makes (magic, header length, header, then the
/// ciphertext with the three as associated data), with no check of what the kind carries, so it can hold an item that
/// version carried and this one refuses to seal.
fn sealed_by_an_earlier_version(kind: &str, items: Value) -> String {
    let header = json!({ "kind": kind, "formatVersion": FORMAT_VERSION, "nonce": hex(&NONCE), "kdf": kdf() }).to_string().into_bytes();
    let mut out = b"NPBK".to_vec();
    out.extend_from_slice(&(header.len() as u32).to_be_bytes());
    out.extend_from_slice(&header);
    let plain = json!({ "epoch": 1, "items": items }).to_string().into_bytes();
    let ct = Aes256Gcm::new_from_slice(&KEY).unwrap().encrypt(Nonce::from_slice(&NONCE), Payload { msg: &plain, aad: &out }).unwrap();
    out.extend_from_slice(&ct);
    hex(&out)
}

fn open(bundle: &str) -> Value {
    ask(json!({"op":"openBundle","seenEpoch":0,"bundle":bundle,"key":hex(&KEY)}))
}

// @verifies REQ-SHDR-10#b
#[test]
#[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
fn neither_kind_of_user_bundle_seals_the_warranty_owners_enrolment_or_consent() {
    for kind in ["migrate", "uhdr"] {
        for item in ["characterisationEnrolment", "warrantyConsentFlag"] {
            assert!(seal(kind, json!({ item: {} })).is_err(), "{kind} must not seal {item}");
        }
        let inv = ask(json!({"op":"inventory","kind":kind}));
        assert!(!inv["carried"].to_string().contains("characterisationEnrolment"), "{kind}");
    }
    // The migration's own inventory names both among what never moves.
    let never = ask(json!({"op":"inventory","kind":"migrate"}))["perInstall"].to_string();
    assert!(never.contains("characterisationEnrolment") && never.contains("warrantyConsentFlag"));
    // No stored key of the warranty owner's is a carried key, so no adapter is ever handed one to put in a bundle.
    let carried = ask(json!({"op":"itemKeys"}))["carried"].to_string();
    for k in ["np.warranty-owner.records", "np.warranty.consent-granted", "np.characterisation.enrolled", "np.characterisation.consent-epoch", "np.characterisation.programme-id"] {
        assert!(!carried.contains(k), "{k}");
    }
    // Control: the same request with an item the kind carries is sealed, and a carried key is listed.
    assert!(seal("uhdr", json!({"sessionHistory": []})).is_ok());
    assert!(carried.contains("np.session.history"));
}

// @verifies REQ-SHDR-10#c
#[test]
#[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
fn a_bundle_an_earlier_version_sealed_with_the_enrolment_opens_without_it() {
    let history = json!([{"id": 1}]);
    // Control: the helper makes the bundle the core makes, byte for byte, when the items are ones it seals.
    let same: Value = serde_json::from_str(&seal("uhdr", json!({"sessionHistory": history})).unwrap()).unwrap();
    assert_eq!(same["bundle"], sealed_by_an_earlier_version("uhdr", json!({"sessionHistory": history})));

    for kind in ["migrate", "uhdr"] {
        let old = sealed_by_an_earlier_version(kind, json!({"sessionHistory": history, "characterisationEnrolment": {"np.characterisation.enrolled": "true"}}));
        let opened = open(&old);
        assert_eq!(opened["ok"], true, "{opened}");
        assert_eq!(opened["items"], json!({"sessionHistory": history}), "the enrolment is dropped and the rest restored");
    }
    // An item no version carried is still refused whole.
    let forged = sealed_by_an_earlier_version("uhdr", json!({"sessionHistory": history, "consentGrants": {}}));
    assert_eq!(open(&forged)["reason"], "malformed");
}
