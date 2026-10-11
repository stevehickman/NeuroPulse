//! The serializer against the web serializer it replaced (`npps-serialize-golden.json`, frozen) and against
//! the parser (round trip): what the core writes, the core reads back unchanged.

use neurone_npps_core::api::{parse_json, serialize_json};
use serde_json::{json, Value};
use std::fs;
use std::path::PathBuf;

fn root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn golden() -> Value {
    let p = root().join("app/NeurOneShared/TestData/npps-serialize-golden.json");
    serde_json::from_str(&fs::read_to_string(p).expect("golden file")).expect("golden JSON")
}

#[test]
fn every_block_of_the_library_is_written_as_the_web_serializer_wrote_it() {
    let g = golden();
    let files = g["files"].as_object().expect("files");
    assert!(files.len() > 60, "the golden covers the shipped library");
    let mut failures = Vec::new();
    let mut n = 0;
    for (rel, items) in files {
        for item in items.as_array().unwrap() {
            n += 1;
            let got = serialize_json(&json!({ "items": [item["item"]] }).to_string());
            let want = item["text"].as_str().unwrap();
            match got {
                Ok(t) if t == want => {}
                Ok(t) => failures.push(format!("{rel}: differs\n--- core\n{t}\n--- web\n{want}")),
                Err(e) => failures.push(format!("{rel}: refused: {e}")),
            }
        }
    }
    assert!(n > 100);
    assert!(failures.is_empty(), "{} divergence(s):\n{}", failures.len(), failures.join("\n"));
}

/// The parsed content, minus what a parse generates or the text does not carry.
fn strip(mut v: Value) -> Value {
    match &mut v {
        Value::Object(m) => {
            m.remove("isPredefined");
            for x in m.values_mut() {
                *x = strip(x.take());
            }
        }
        Value::Array(a) => {
            for x in a.iter_mut() {
                *x = strip(x.take());
            }
        }
        _ => {}
    }
    v
}

fn items_of(parsed: &Value) -> Vec<Value> {
    let mut items = Vec::new();
    for e in parsed["entries"].as_array().unwrap() {
        items.push(e.clone());
    }
    for (k, kind) in [("zones", "zone"), ("conditions", "condition"), ("wavelengthRules", "wavelengthRules"), ("limits", "limits")] {
        for x in parsed[k].as_array().unwrap() {
            items.push(json!({ "kind": kind, kind: x }));
        }
    }
    items
}

fn round_trip(name: &str, src: &str) {
    let first: Value = serde_json::from_str(&parse_json(src).unwrap_or_else(|e| panic!("{name}: {e}"))).unwrap();
    let text = serialize_json(&json!({ "items": items_of(&first) }).to_string()).unwrap_or_else(|e| panic!("{name}: {e}"));
    let second: Value = serde_json::from_str(&parse_json(&text).unwrap_or_else(|e| panic!("{name}: reparse: {e}\n{text}"))).unwrap();
    // Entries, zones, conditions and rules keep their order; the writer emits entries first, then the rest.
    for k in ["entries", "zones", "conditions", "wavelengthRules", "limits"] {
        assert_eq!(strip(first[k].clone()), strip(second[k].clone()), "{name}: {k} changed on a round trip\n{text}");
    }
}

#[test]
fn everything_the_library_declares_survives_write_then_read() {
    let g = golden();
    for rel in g["files"].as_object().unwrap().keys() {
        round_trip(rel, &fs::read_to_string(root().join(rel)).unwrap());
    }
}

#[test]
fn a_limits_set_survives_write_then_read() {
    // The web writer snake-cased property names (max_irradiance_m_wcm2, max_frequency_hz), which the parser
    // does not read, so ceilings written by the app were dropped on the next load.
    let src = r#"limits "Clinic" {
    level: helmet
    helmet_id: "H-1"
    description: "x"
    pbm_transcranial {
        max_irradiance_mw_cm2: 300
        max_frequency: 40
        max_duty_cycle: 25
        max_session_dose: 12.5
    }
    eeg_neurofeedback {
        allowed_bands: ["alpha", "beta"]
        require_closed_loop: true
    }
    tms {
        max_intensity_pct_mt: 80
        allowed_targets: ["dlpfc"]
    }
    audio_entrainment {
        max_volume_db: 80
        max_binaural_beats: 40
    }
}"#;
    round_trip("limits", src);
    let parsed: Value = serde_json::from_str(&parse_json(src).unwrap()).unwrap();
    let text = serialize_json(&json!({ "items": items_of(&parsed) }).to_string()).unwrap();
    assert!(text.contains("max_irradiance_mw_cm2: 300") && text.contains("max_intensity_pct_mt: 80"), "{text}");
}

#[test]
fn a_model_that_cannot_be_written_is_refused_with_the_web_messages() {
    let zone = json!({ "items": [{ "kind": "zone", "zone": { "name": "Z", "sockets": [0, 4, 4, 999] } }] });
    assert_eq!(
        serialize_json(&zone.to_string()).unwrap_err(),
        "cannot serialize zone \"Z\": 0, 999 are not sockets on this helmet — ids are whole numbers 1–80"
    );
    let named = json!({ "items": [{ "kind": "single", "protocol": {
        "name": "P", "id": "i", "description": "", "author": "a", "version": "1", "tags": [],
        "timingMode": { "type": "duration", "seconds": 60 },
        "modalities": [{ "type": "pbm_transcranial", "enabled": true, "interval": {},
            "params": { "wavelength": "808nm", "irradianceMWcm2": 100, "frequencyHz": 40, "dutyCyclePercent": 25,
                        "zones": "named", "zoneRefs": [] } }] } }] });
    assert!(serialize_json(&named.to_string()).unwrap_err().starts_with("pbm_transcranial: zones is 'named' but zoneRefs is empty"));
}

#[test]
fn a_zone_is_written_with_a_list_for_each_hardware_version_and_a_bare_list_migrates_to_version_one() {
    // SWR-HV-05. A model with only `sockets` (every zone before hardware versions) is written as version 1, which is how
    // an existing zone file migrates: on its next save, and with the same members.
    let bare = json!({ "items": [{ "kind": "zone", "zone": { "name": "Z", "sockets": [5, 3, 3] } }] });
    let written = serialize_json(&bare.to_string()).unwrap();
    assert_eq!(written, "zone \"Z\" {\n    hardware_version 1 {\n        sockets: [3, 5]\n    }\n}");
    // The same members are read back, as version 1.
    let back: Value = serde_json::from_str(&parse_json(&written).unwrap()).unwrap();
    assert_eq!(back["zones"][0]["sockets"], json!([3, 5]));
    assert_eq!(back["zones"][0]["socketsByVersion"], json!({ "1": [3, 5] }));

    // A zone with several versions keeps each list under its own version, in version order.
    let two = json!({ "items": [{ "kind": "zone", "zone": { "name": "Z", "sockets": [1, 2],
        "socketsByVersion": { "2": [9, 8], "1": [2, 1] } } }] });
    let written = serialize_json(&two.to_string()).unwrap();
    assert_eq!(
        written,
        "zone \"Z\" {\n    hardware_version 1 {\n        sockets: [1, 2]\n    }\n    hardware_version 2 {\n        sockets: [8, 9]\n    }\n}"
    );
    round_trip("two versions", &written);
}

#[test]
fn a_zone_whose_two_version_one_lists_disagree_is_refused_not_resolved() {
    let zone = json!({ "items": [{ "kind": "zone", "zone": { "name": "Z", "sockets": [1, 2, 3],
        "socketsByVersion": { "1": [1, 2] } } }] });
    assert!(serialize_json(&zone.to_string()).unwrap_err().contains("'sockets' and socketsByVersion[1] disagree"));
    let bad = json!({ "items": [{ "kind": "zone", "zone": { "name": "Z", "socketsByVersion": { "0": [1] } } }] });
    assert!(serialize_json(&bad.to_string()).unwrap_err().contains("'0' is not a hardware version"));
}

#[test]
fn a_protocols_family_id_and_hardware_version_survive_write_then_read() {
    let src = "protocol \"P\" {\n    id: \"11111111-2222-3333-4444-555555555555\"\n    family_id: \"0123abcd-4567-4def-8901-23456789abcd\"\n    hardware_version: 1\n    duration: 5m\n}";
    round_trip("identity", src);
    let parsed: Value = serde_json::from_str(&parse_json(src).unwrap()).unwrap();
    let text = serialize_json(&json!({ "items": items_of(&parsed) }).to_string()).unwrap();
    assert!(text.contains("family_id: \"0123abcd-4567-4def-8901-23456789abcd\"\n    hardware_version: 1\n"), "{text}");
    // A protocol that states neither is written with neither: the writer adds no identity of its own.
    let plain = "protocol \"P\" {\n    duration: 5m\n}";
    let parsed: Value = serde_json::from_str(&parse_json(plain).unwrap()).unwrap();
    let text = serialize_json(&json!({ "items": items_of(&parsed) }).to_string()).unwrap();
    assert!(!text.contains("family_id") && !text.contains("hardware_version"), "{text}");
}
