//! `SWR-HV-03`: the register of released wavelength rule sets (`common/npps/wavelength-rules-register.json`,
//! NP-NPPS-REF-001 §7a.1). The shipped file must be the last row, so a shipped window cannot change without a
//! release being recorded.

use neurone_npps_core::parse_file;
use neurone_npps_core::wavelength::*;
use serde_json::Value;

fn rules_of(v: &Value) -> WavelengthRules {
    let ch = |e: &Value| ChannelRule {
        element: match e["element"].as_str().unwrap() {
            "led_660" => Channel::Led660,
            "led_808" => Channel::Led808,
            _ => Channel::Led1064,
        },
        nominal_nm: e["nominalNm"].as_f64().unwrap(),
        min_nm: e["minNm"].as_f64().unwrap(),
        max_nm: e["maxNm"].as_f64().unwrap(),
    };
    WavelengthRules {
        name: v["name"].as_str().unwrap().into(),
        channels: v["channels"].as_array().unwrap().iter().map(ch).collect(),
    }
}

fn file_rules(rel: &str) -> WavelengthRules {
    let p = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../..").join(rel);
    let parsed = parse_file(&std::fs::read_to_string(&p).unwrap_or_else(|e| panic!("{}: {e}", p.display()))).unwrap();
    assert_eq!(parsed.wavelength_rules.len(), 1, "{rel} holds one wavelength_rules block");
    rules_of(&parsed.wavelength_rules[0])
}

const V1: &str = "316654aa4924a962a5e3501f80aeff3d0289498e513ac4a8572581c0f8828aa1";

// @verifies SWR-HV-03
#[test]
fn the_shipped_defaults_file_and_the_core_copy_are_the_last_release() {
    let reg = releases();
    let last = reg.last().unwrap();
    assert_eq!(rules_hash(&file_rules("protocols/predefined/00-wavelength-rules.npps")).unwrap(), last.hash,
        "protocols/predefined/00-wavelength-rules.npps is not the last row of the register: a shipped window changed without a release, or a release was not appended");
    assert_eq!(rules_hash(&WavelengthRules::default_rules()).unwrap(), last.hash,
        "WavelengthRules::default_rules() is not the last row of the register");
}

// @verifies SWR-HV-03
#[test]
fn release_1_0_is_pinned_to_vector_v1_and_never_edited() {
    // A golden, so editing the first row means editing this test too: a released row is not changed in place.
    let first = &releases()[0];
    assert_eq!((first.major, first.minor, first.hash.as_str()), (1, 0, V1));
}

// @verifies SWR-HV-03
#[test]
fn an_edited_set_is_not_a_release_and_the_shipped_set_is_whatever_its_level_says() {
    // The fixture overrides one channel; the rules in force are the defaults with it merged in (§7a).
    let user = file_rules("npps/fixtures/wavelength_rules.npps");
    let mut lab = WavelengthRules::default_rules();
    for u in &user.channels {
        lab.channels.retain(|c| c.element != u.element);
        lab.channels.push(u.clone());
    }
    let s = stamp_for(&lab).unwrap();
    assert_eq!((s.origin, s.major, s.minor), (ORIGIN_OWNER_SET, 1, 0), "an owner set over the latest release");
    assert_eq!(s.hash_hex().unwrap(), "eadab368297cb7a38b3e388b046ebdce52c47ca2a9032d471ed4e19d1ef54181");
    let s = stamp_for(&file_rules("protocols/predefined/00-wavelength-rules.npps")).unwrap();
    assert_eq!((s.origin, s.major, s.minor), (ORIGIN_RELEASE, 1, 0));
}

// @verifies SWR-HV-03
#[test]
fn the_register_refuses_what_append_only_forbids() {
    let row = |maj: u8, min: u8, h: &str| format!(r#"{{"major":{maj},"minor":{min},"hash":"{h}"}}"#);
    let reg = |rows: &[String]| format!(r#"{{"releases":[{}]}}"#, rows.join(","));
    let other = "ab".repeat(32);
    let third = "cd".repeat(32);
    assert_eq!(parse_register(&reg(&[row(1, 0, V1), row(1, 1, &other)])).unwrap().len(), 2);
    for (what, text, needle) in [
        ("a version reused", reg(&[row(1, 0, V1), row(1, 0, &other)]), "never goes down and is never reused"),
        ("a version lowered", reg(&[row(1, 1, V1), row(1, 0, &other)]), "never goes down and is never reused"),
        ("a set released twice", reg(&[row(1, 0, V1), row(1, 1, &other), row(2, 0, V1)]), "already released"),
        ("a hash not 64 hex", reg(&[row(1, 0, "abc")]), "64 lowercase hex"),
        ("an upper-case hash", reg(&[row(1, 0, &V1.to_uppercase())]), "64 lowercase hex"),
        ("no release", reg(&[]), "no release"),
        ("not JSON", "{".to_string(), "not JSON"),
    ] {
        let e = parse_register(&text).unwrap_err();
        assert!(e.contains(needle), "{what}: {e}");
    }
    let _ = third;
}
