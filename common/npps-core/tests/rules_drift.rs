//! `SWR-HV-03`: what the wavelength rules a stored descriptor was compiled under do to its protocol now
//! (`library_json` op `rulesDrift`, NP-NPPS-REF-001 §7a.1). The core decides; the app words the notice and offers the
//! upload, and never re-uploads silently.

use neurone_npps_core::library::library_json;
use neurone_npps_core::wavelength::*;
use neurone_npps_core::wavelength_version::{stamp_for, ResolvedSet};
use serde_json::{json, Value};

fn stamp_hex(r: &WavelengthRules) -> String {
    stamp_for(&ResolvedSet::from_rules(r).unwrap()).unwrap().encode().iter().map(|b| format!("{b:02x}")).collect()
}

fn defaults_edit(f: impl Fn(&mut WavelengthRules)) -> WavelengthRules {
    let mut r = WavelengthRules::default_rules();
    f(&mut r);
    r
}

fn rules_json(r: &WavelengthRules) -> Value {
    json!({ "name": r.name, "channels": r.channels.iter().map(|c| json!({
        "element": c.element.name(), "nominalNm": c.nominal_nm, "minNm": c.min_nm, "maxNm": c.max_nm })).collect::<Vec<_>>() })
}

fn drift(stored: &WavelengthRules, in_force: Option<&WavelengthRules>, wavelengths: &[&str]) -> Value {
    let req = json!({ "op": "rulesDrift", "stampHex": stamp_hex(stored), "wavelengths": wavelengths,
        "rules": in_force.map(rules_json) });
    serde_json::from_str(&library_json(&req.to_string()).unwrap()).unwrap()
}

fn wide808() -> WavelengthRules {
    defaults_edit(|r| r.channels[1].max_nm = 860.0)
}

// @verifies SWR-HV-03
#[test]
fn the_same_windows_are_current_whatever_the_name_or_the_release() {
    let d = WavelengthRules::default_rules();
    assert_eq!(drift(&d, None, &["810nm"])["state"], "current", "rules null is the shipped defaults");
    let mut renamed = d.clone();
    renamed.name = "renamed".into();
    renamed.channels.reverse();
    assert_eq!(drift(&d, Some(&renamed), &["810nm"])["state"], "current", "compared by content, never by name or order");
}

// @verifies SWR-HV-03
#[test]
fn different_windows_that_map_this_protocol_the_same_are_noted_not_stale() {
    let r = drift(&wide808(), None, &["810nm", "660nm", "1064nm"]);
    assert_eq!(r["state"], "differs");
    assert_eq!(r["changes"], json!([]));
    assert_ne!(r["storedHash"], r["rulesHash"]);
    // A nominal moved with no overlapping windows decides nothing (V5), so no wavelength moves.
    let v5 = defaults_edit(|r| r.channels[1].nominal_nm = 810.0);
    assert_eq!(drift(&WavelengthRules::default_rules(), Some(&v5), &["810nm", "830nm"])["state"], "differs");
    // A protocol with no PBM wavelength is not touched by any rules change.
    assert_eq!(drift(&wide808(), None, &[])["state"], "differs");
}

// @verifies SWR-HV-03
#[test]
fn a_wavelength_that_moves_or_is_refused_or_newly_delivered_makes_it_stale_and_is_named() {
    // Stored under the wide 808 window, 850 nm went to the 808 channel; the defaults refuse it now.
    let r = drift(&wide808(), None, &["810nm", "850nm"]);
    assert_eq!(r["state"], "stale");
    assert_eq!(r["changes"], json!([{ "wavelength": "850nm", "was": "led_808", "now": null }]));
    // The other way: refused then, delivered now.
    let r = drift(&WavelengthRules::default_rules(), Some(&wide808()), &["850nm"]);
    assert_eq!(r["changes"], json!([{ "wavelength": "850nm", "was": null, "now": "led_808" }]));
    // A wavelength that changes channel: the 808 window now reaches 650 to 700 with its nominal near 660.
    let moved = defaults_edit(|r| {
        r.channels[1].nominal_nm = 650.0;
        r.channels[1].min_nm = 640.0;
        r.channels[1].max_nm = 700.0;
    });
    let r = drift(&WavelengthRules::default_rules(), Some(&moved), &["652nm"]);
    assert_eq!(r["changes"], json!([{ "wavelength": "652nm", "was": "led_660", "now": "led_808" }]));
    // A channel with no rule delivers nothing (V3).
    let no1064 = {
        let mut r = WavelengthRules::default_rules();
        r.channels.pop();
        r
    };
    let r = drift(&WavelengthRules::default_rules(), Some(&no1064), &["1064nm", "660nm"]);
    assert_eq!(r["changes"], json!([{ "wavelength": "1064nm", "was": "led_1064", "now": null }]));
}

// @verifies SWR-HV-03
#[test]
fn a_protocol_definition_is_read_for_its_enabled_pbm_blocks_only() {
    let def = json!({ "modalities": [
        { "type": "pbm_transcranial", "enabled": true, "params": { "wavelength": "850nm" } },
        { "type": "pbm_intranasal", "enabled": false, "params": { "wavelength": "633nm" } },
        { "type": "bes_tacs", "enabled": true, "params": {} },
        { "type": "pbm_transcranial", "enabled": true, "params": { "wavelength": "850nm" } } ] });
    let req = json!({ "op": "rulesDrift", "stampHex": stamp_hex(&wide808()), "def": def, "rules": null });
    let r: Value = serde_json::from_str(&library_json(&req.to_string()).unwrap()).unwrap();
    assert_eq!(r["changes"], json!([{ "wavelength": "850nm", "was": "led_808", "now": null }]), "once, and not the disabled block");
}

// @verifies SWR-HV-03
#[test]
fn a_descriptor_that_states_no_rules_is_not_judged_and_a_malformed_stamp_is_refused() {
    let req = |hex: &str| json!({ "op": "rulesDrift", "stampHex": hex, "wavelengths": ["850nm"], "rules": null }).to_string();
    let r: Value = serde_json::from_str(&library_json(&req(&"00".repeat(39))).unwrap()).unwrap();
    assert_eq!(r["state"], "notStated");
    assert_eq!(r["changes"], json!([]), "nothing is guessed about rules the descriptor did not state");
    assert!(r["storedHash"].is_null());
    assert!(library_json(&req("00")).is_err());
    assert!(library_json(&req(&"zz".repeat(39))).is_err());
    assert!(library_json(&req(&format!("03{}", "00".repeat(38)))).unwrap_err().contains("origin"));
    // A rule value off the grid is refused, not rounded, here as in a compile.
    let mut bad = rules_json(&WavelengthRules::default_rules());
    bad["channels"][0]["minNm"] = json!(650.00004);
    let e = library_json(&json!({ "op": "rulesDrift", "stampHex": stamp_hex(&WavelengthRules::default_rules()), "wavelengths": [], "rules": bad }).to_string()).unwrap_err();
    assert!(e.contains("0.1 pm"), "{e}");
}

// @verifies SWR-HV-03
#[test]
fn a_compiled_descriptors_stamp_is_read_back_for_the_record_and_a_non_descriptor_is_refused() {
    use neurone_npps_core::api::compile_json;
    let req = json!({
        "def": { "name": "T", "familyId": "11111111-2222-3333-4444-555555555555", "timingMode": { "type": "duration", "seconds": 600 },
            "modalities": [{ "type": "pbm_transcranial", "enabled": true,
                "params": { "zones": "named", "zoneRefs": ["Frontal"], "wavelength": "810nm", "irradianceMWcm2": 200, "frequencyHz": 40, "dutyCyclePercent": 25 },
                "interval": { "intervalOnSeconds": 0, "intervalOffSeconds": 0 } }] },
        "hardwareVersion": 1, "zonesByVersion": { "Frontal": { "1": [3, 1, 2] } }, "clinicianSockets": null,
        "deviceSerialHex": "aabbccdd", "nowUnix": 1_000, "sessionUuidHex": "00112233445566778899aabbccddeeff",
        "wavelengthRules": rules_json(&wide808()), "autonomous": true, "acknowledgedCautions": [] });
    let blob = compile_json(&req.to_string()).unwrap();
    let hex: String = blob.iter().map(|b| format!("{b:02x}")).collect();
    let got: Value = serde_json::from_str(&library_json(&json!({ "op": "descriptorStamp", "descriptorHex": hex }).to_string()).unwrap()).unwrap();
    assert_eq!(got["stampHex"], stamp_hex(&wide808()), "the stamp of the rules the descriptor was compiled under");
    // Fed back, the record is judged against the rules now in force: the shipped defaults refuse 850 nm.
    let drift: Value = serde_json::from_str(&library_json(&json!({ "op": "rulesDrift", "stampHex": got["stampHex"], "wavelengths": ["850nm"], "rules": null }).to_string()).unwrap()).unwrap();
    assert_eq!(drift["state"], "stale");
    assert!(library_json(&json!({ "op": "descriptorStamp", "descriptorHex": "00" }).to_string()).is_err());
    assert!(library_json(&json!({ "op": "descriptorStamp", "descriptorHex": "0g" }).to_string()).is_err());
}
