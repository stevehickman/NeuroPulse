//! `SWR-WO-07` (`OI-PRIV-11` part 2): the share form of an export. A file written for someone else carries no used flag, no
//! helmet serial and no individual id, and its protocol and family ids are new; everything else is as the sender wrote it.

use neurone_npps_core::api::{parse_json, serialize_json};
use serde_json::{json, Value};

const SRC: &str = r#"protocol "Evening" {
  id: "p1"
  family_id: "11111111-2222-3333-4444-555555555555"
  description: "for sleep"
  author: "Alex"
  used: true
  conditions: ["Insomnia"]
  duration: 10m
  pbm_transcranial {
    zones: [All]
    wavelength: "660nm"
    irradiance: 200mW_cm2
  }
}

protocol "Evening v2" {
  id: "p2"
  family_id: "11111111-2222-3333-4444-555555555555"
  used: true
  duration: 12m
  pbm_transcranial {
    zones: [All]
    wavelength: "660nm"
    irradiance: 200mW_cm2
  }
}

limits "Clinic helmet" {
  level: helmet
  helmet_id: "NP-000123"
  tdcs {
    max_intensity: 1mA
  }
}

limits "Alex" {
  level: individual
  individual_id: "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"
  tdcs {
    max_intensity: 1mA
  }
}"#;

const SEED_A: &str = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
const SEED_B: &str = "ff0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

fn items() -> Vec<Value> {
    let parsed: Value = serde_json::from_str(&parse_json(SRC).unwrap()).unwrap();
    let mut items = parsed["entries"].as_array().unwrap().clone();
    items.extend(parsed["limits"].as_array().unwrap().iter().map(|l| json!({ "kind": "limits", "limits": l })));
    items
}

fn full() -> String {
    serialize_json(&json!({ "items": items() }).to_string()).unwrap()
}

fn share(seed: &str) -> String {
    serialize_json(&json!({ "items": items(), "share": { "randomHex": seed } }).to_string()).unwrap()
}

fn field(text: &str, key: &str) -> Vec<String> {
    text.lines().filter_map(|l| l.trim().strip_prefix(&format!("{key}: ")).map(str::to_string)).collect()
}

// @verifies SWR-WO-07#a
#[test]
fn the_share_form_carries_no_used_flag_and_the_full_form_still_does() {
    assert_eq!(field(&full(), "used"), ["true", "true"]);
    assert!(field(&share(SEED_A), "used").is_empty());
    assert!(!share(SEED_A).contains("used"));
}

// @verifies SWR-WO-07#b
#[test]
fn the_share_form_has_new_protocol_and_family_ids_and_two_shares_do_not_match() {
    let a = share(SEED_A);
    let b = share(SEED_B);
    for t in [&a, &b] {
        assert!(!t.contains("\"p1\"") && !t.contains("\"p2\"") && !t.contains("11111111-2222"));
        let ids = field(t, "id");
        let fams = field(t, "family_id");
        assert_eq!((ids.len(), fams.len()), (2, 2));
        assert_ne!(ids[0], ids[1]);
        assert_eq!(fams[0], fams[1], "two variants of one family in one file stay one family");
        assert_ne!(ids[0], fams[0]);
        // A version 4 UUID: 8-4-4-4-12, version nibble 4, variant 8..b.
        let id = ids[0].trim_matches('"');
        assert_eq!(id.len(), 36);
        assert_eq!(&id[14..15], "4");
        assert!(matches!(&id[19..20], "8" | "9" | "a" | "b"));
    }
    assert_ne!(field(&a, "id"), field(&b, "id"));
    assert_ne!(field(&a, "family_id"), field(&b, "family_id"));
}

// @verifies SWR-WO-07#c
#[test]
fn a_shared_limits_set_keeps_its_level_and_names_no_helmet_and_no_person() {
    let full = full();
    assert!(full.contains("helmet_id: \"NP-000123\"") && full.contains("individual_id: \"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\""));
    let s = share(SEED_A);
    assert!(!s.contains("helmet_id") && !s.contains("individual_id") && !s.contains("NP-000123") && !s.contains("aaaaaaaa"));
    assert_eq!(field(&s, "level"), ["helmet", "individual"]);
    // It reads back as limits at the same levels.
    let back: Value = serde_json::from_str(&parse_json(&s).unwrap()).unwrap();
    let levels: Vec<&str> = back["limits"].as_array().unwrap().iter().map(|l| l["level"].as_str().unwrap()).collect();
    assert_eq!(levels, ["helmet", "individual"]);
}

// @verifies SWR-WO-07#d
#[test]
fn everything_else_is_written_as_the_sender_wrote_it() {
    let strip = |t: &str| -> Vec<String> {
        t.lines()
            .filter(|l| {
                let l = l.trim();
                !(l.starts_with("id: ") || l.starts_with("family_id: ") || l.starts_with("used: ") || l.starts_with("helmet_id: ") || l.starts_with("individual_id: "))
            })
            .map(str::to_string)
            .collect()
    };
    let s = share(SEED_A);
    assert_eq!(strip(&s), strip(&full()));
    assert!(s.contains("author: \"Alex\"") && s.contains("\"Insomnia\"") && s.contains("description: \"for sleep\""));
}

// @verifies SWR-WO-07#e
#[test]
fn a_share_request_without_32_random_bytes_is_refused_and_never_written_in_full() {
    for share in [json!({}), json!({ "randomHex": "" }), json!({ "randomHex": "0011" }), json!({ "randomHex": "zz" }), json!(true)] {
        let r = serialize_json(&json!({ "items": items(), "share": share }).to_string());
        assert!(r.is_err(), "{share} must be refused, got {r:?}");
    }
}
