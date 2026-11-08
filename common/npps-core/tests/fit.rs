//! The fit check against descriptors the real compiler writes (`hub-descriptor-cases.json`), so the bytes it reads are
//! the bytes the hub reads (`OI-HWFIT-01`).

use neurone_npps_core::compiler::{compile_protocol, CompileOptions};
use neurone_npps_core::fit::check_fit;
use serde_json::Value;
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

fn cases() -> Value {
    let p = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../app/NeurOneShared/TestData/hub-descriptor-cases.json");
    serde_json::from_str(&fs::read_to_string(p).unwrap()).unwrap()
}

/// The compiled descriptor of a golden case, and the sockets of the zone it targets.
fn compiled(name: &str) -> (Vec<u8>, Vec<u32>) {
    let c = cases();
    let zones: HashMap<String, Vec<u32>> = c["zones"]
        .as_object()
        .unwrap()
        .iter()
        .map(|(k, v)| (k.clone(), v.as_array().unwrap().iter().map(|n| n.as_u64().unwrap() as u32).collect()))
        .collect();
    let clinician: Vec<u32> = c["clinicianSockets"].as_array().unwrap().iter().map(|n| n.as_u64().unwrap() as u32).collect();
    let opts = CompileOptions {
        device_serial: None,
        zones: Some(&zones),
        clinician_sockets: Some(&clinician),
        wavelength_rules: None,
        autonomous: false,
        now_unix: 0,
        session_uuid: [0; 16],
        hardware_version: 1,
        acknowledged_cautions: &[],
    };
    let blob = compile_protocol(&c["cases"][name]["def"], &opts).unwrap().blob;
    (blob, zones["Frontal"].clone())
}

fn fitted(sockets: &[u32], elements: &[&str]) -> HashMap<u32, Vec<String>> {
    sockets.iter().map(|&s| (s, elements.iter().map(|e| e.to_string()).collect())).collect()
}

// @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21)
#[test]
fn a_dual_module_runs_660_and_808_and_lacks_1064() {
    let dual = ["led_660", "led_808", "ntc", "pd_forward", "pd_back"];
    let (merged, frontal) = compiled("pbmMerged");
    assert!(check_fit(&merged, &fitted(&frontal, &dual)).unwrap().is_empty());

    let (tri, frontal) = compiled("pbm1064");
    let short = check_fit(&tri, &fitted(&frontal, &dual)).unwrap();
    assert_eq!(short.len(), frontal.len(), "every socket of the zone is short, not just one");
    let m = &short[0].missing;
    assert_eq!(m.len(), 1);
    assert_eq!(m[0].element, "led_1064");
    assert_eq!(m[0].candidate_parts, ["ZM-COMBO-TRI", "ZM-PBM-TRI"]);
    assert!(!short[0].empty);
}

// @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21)
#[test]
fn a_zone_with_one_socket_missing_names_that_socket_and_a_wrong_module_misses_the_emitter() {
    let (blob, frontal) = compiled("pbm808");
    let mut inv = fitted(&frontal, &["led_660", "led_808"]);
    inv.remove(&frontal[3]);
    inv.insert(frontal[5], vec!["led_660".into(), "ntc".into()]);
    let short = check_fit(&blob, &inv).unwrap();
    let sockets: Vec<u32> = short.iter().map(|s| s.socket).collect();
    assert_eq!(sockets, [frontal[3].min(frontal[5]), frontal[3].max(frontal[5])]);
    assert!(short.iter().find(|s| s.socket == frontal[3]).unwrap().empty);
    assert!(!short.iter().find(|s| s.socket == frontal[5]).unwrap().empty);
}

// @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21)
#[test]
fn slot_addressed_modalities_need_no_socket_and_garbage_is_refused() {
    let (eeg, _) = compiled("eegAll");
    assert!(check_fit(&eeg, &HashMap::new()).unwrap().is_empty());
    assert!(check_fit(&[0u8; 10], &HashMap::new()).is_err());
    let (mut cut, _) = compiled("pbm660");
    cut.truncate(70);
    assert!(check_fit(&cut, &HashMap::new()).is_err());
}

// @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21)
#[test]
fn the_library_entry_point_every_app_already_calls_answers_fitcheck() {
    use neurone_npps_core::api::library_json;
    let (blob, frontal) = compiled("pbm1064");
    let hex: String = blob.iter().map(|b| format!("{b:02x}")).collect();
    let inv: serde_json::Map<String, Value> =
        frontal.iter().map(|s| (s.to_string(), serde_json::json!(["led_660", "led_808"]))).collect();
    let ask = |inventory: &serde_json::Map<String, Value>| {
        let req = serde_json::json!({ "op": "fitCheck", "descriptorHex": hex, "inventory": inventory });
        serde_json::from_str::<Value>(&library_json(&req.to_string()).unwrap()).unwrap()
    };
    let short = ask(&inv);
    assert_eq!(short["fits"], false);
    assert_eq!(short["shortfalls"][0]["missing"][0]["element"], "led_1064");
    assert_eq!(short["shortfalls"][0]["missing"][0]["candidateParts"], serde_json::json!(["ZM-COMBO-TRI", "ZM-PBM-TRI"]));

    let tri: serde_json::Map<String, Value> =
        frontal.iter().map(|s| (s.to_string(), serde_json::json!(["led_660", "led_808", "led_1064"]))).collect();
    assert_eq!(ask(&tri)["fits"], true);

    let bad = serde_json::json!({ "op": "fitCheck", "descriptorHex": "zz", "inventory": {} });
    assert!(library_json(&bad.to_string()).is_err());
}

// @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21)
#[test]
fn a_caller_with_only_the_hubs_tile_types_asks_with_them() {
    use neurone_npps_core::api::library_json;
    let (blob, frontal) = compiled("pbm1064");
    let hex: String = blob.iter().map(|b| format!("{b:02x}")).collect();
    let by_type = |t: u64| -> serde_json::Map<String, Value> { frontal.iter().map(|s| (s.to_string(), serde_json::json!(t))).collect() };
    let ask = |inv: &serde_json::Map<String, Value>| {
        let req = serde_json::json!({ "op": "fitCheck", "descriptorHex": hex, "inventory": inv });
        serde_json::from_str::<Value>(&library_json(&req.to_string()).unwrap()).unwrap()
    };
    assert_eq!(ask(&by_type(3))["fits"], true, "T1-C carries 1064 nm");
    for t in [1, 2, 4] {
        assert_eq!(ask(&by_type(t))["fits"], false, "type {t} has no 1064 nm emitter");
    }
    // Absent, unknown and a type this build does not know all carry nothing, and the socket reads as empty.
    for t in [0, 255, 77] {
        let r = ask(&by_type(t));
        assert_eq!(r["fits"], false);
        assert_eq!(r["shortfalls"][0]["empty"], true);
    }
}

// @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21)
#[test]
fn sockets_that_fall_short_the_same_way_are_one_group() {
    use neurone_npps_core::api::library_json;
    let (blob, frontal) = compiled("pbmMerged"); // needs 660 and 808 nm
    let hex: String = blob.iter().map(|b| format!("{b:02x}")).collect();
    let both = serde_json::json!(["led_660", "led_808"]);
    let mut inv: serde_json::Map<String, Value> = frontal.iter().map(|s| (s.to_string(), both.clone())).collect();
    // Two sockets lack both emitters (one empty, one an electrode), one lacks 808 nm only.
    inv.remove(&frontal[0].to_string());
    inv.insert(frontal[1].to_string(), serde_json::json!(["eeg_electrode"]));
    inv.insert(frontal[2].to_string(), serde_json::json!(["led_660"]));
    let req = serde_json::json!({ "op": "fitCheck", "descriptorHex": hex, "inventory": inv });
    let r: Value = serde_json::from_str(&library_json(&req.to_string()).unwrap()).unwrap();
    let groups = r["groups"].as_array().unwrap();
    assert_eq!(groups.len(), 2);
    let mut first = [frontal[0], frontal[1]];
    first.sort();
    assert_eq!(groups[0]["sockets"], serde_json::json!(first));
    assert_eq!(groups[0]["missing"].as_array().unwrap().len(), 2);
    assert_eq!(groups[1]["sockets"], serde_json::json!([frontal[2]]));
    assert_eq!(groups[1]["missing"][0]["element"], "led_808");
}
