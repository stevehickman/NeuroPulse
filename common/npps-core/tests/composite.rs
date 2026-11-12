//! `OI-NPPS-COMP-01`, `SWR-PL-01`: a composite compiles to one descriptor, and its identity is the identity of what it
//! delivers. The rules refuse and never reshape.

use neurone_npps_core::api::{compile_json, content_hash_json};
use neurone_npps_core::{parse_file, Entry};
use serde_json::{json, Map, Value};

struct Library {
    members: Map<String, Value>,
    composites: Vec<Value>,
    zones: Value,
}

fn library() -> Library {
    let dir = concat!(env!("CARGO_MANIFEST_DIR"), "/../../protocols/predefined");
    let mut lib = Library { members: Map::new(), composites: Vec::new(), zones: json!({}) };
    for e in std::fs::read_dir(dir).unwrap() {
        let path = e.unwrap().path();
        if path.extension().and_then(|x| x.to_str()) != Some("npps") {
            continue;
        }
        let f = parse_file(&std::fs::read_to_string(&path).unwrap()).unwrap();
        for z in &f.zones {
            let sockets = z["socketsByVersion"]["1"].clone();
            lib.zones[z["name"].as_str().unwrap()] = sockets;
        }
        for entry in f.entries {
            match entry {
                Entry::Single(p) => {
                    lib.members.insert(p["name"].as_str().unwrap().to_string(), p);
                }
                Entry::Composite(c) => lib.composites.push(c),
            }
        }
    }
    lib
}

fn compile_req(lib: &Library, composite: &Value) -> String {
    json!({
        "composite": composite, "members": lib.members, "zones": lib.zones, "clinicianSockets": null,
        "deviceSerialHex": "aabbccdd", "nowUnix": 1000, "sessionUuidHex": "00112233445566778899aabbccddeeff",
        "wavelengthRules": null, "autonomous": false, "acknowledgedCautions": [], "hardwareVersion": 1
    })
    .to_string()
}

fn hash_req(lib: &Library, composite: &Value) -> String {
    json!({ "composite": composite, "members": lib.members, "zones": lib.zones, "hardwareVersion": 1, "clinicianSockets": null }).to_string()
}

fn layer(name: &str, start: f64, duration: Option<f64>, scale: f64) -> Value {
    let mut l = json!({ "protocolName": name, "startOffsetSeconds": start, "intensityScale": scale });
    if let Some(d) = duration {
        l["durationSeconds"] = json!(d);
    }
    l
}

fn named(lib: &Library, name: &str) -> Value {
    lib.composites.iter().find(|c| c["name"] == name).unwrap().clone()
}

fn two_layer(lib: &Library, mode: &str, a: Value, b: Value) -> Value {
    let _ = lib;
    json!({ "name": "T", "conflictResolution": mode, "layers": [a, b] })
}

// @verifies SWR-PL-01
#[test]
fn a_composite_with_no_overlapping_modality_compiles_and_has_a_hash() {
    let lib = library();
    let c = named(&lib, "Sleep Wind-Down");
    let blob = compile_json(&compile_req(&lib, &c)).unwrap();
    assert_eq!(&blob[0..4], &compile_json(&compile_req(&lib, &named(&lib, "Sleep Optimisation Stack"))).unwrap()[0..4], "same descriptor magic");
    let h = content_hash_json(&hash_req(&lib, &c)).unwrap();
    assert_eq!(h.len(), 64);
    assert_eq!(h, content_hash_json(&hash_req(&lib, &c)).unwrap());
    // The name and other metadata are not identity.
    let mut renamed = c.clone();
    renamed["name"] = json!("Renamed");
    renamed["description"] = json!("other");
    assert_eq!(h, content_hash_json(&hash_req(&lib, &renamed)).unwrap());
}

// @verifies SWR-PL-01
#[test]
fn two_layers_driving_one_modality_at_once_are_refused_not_summed() {
    let lib = library();
    // "Full Multi-Modal RCT" has the same overlap, but a layer of it is continuous-wave PBM, which the compiler refuses
    // first now that a 100 % duty is no longer encoded as 25 % (REQ-NPPS-33 (a)); so it is refused for its duty.
    let err = compile_json(&compile_req(&lib, &named(&lib, "Calm to Focus"))).unwrap_err();
    assert!(err.contains("refused rather than summed or reduced"), "{err}");
    let err = compile_json(&compile_req(&lib, &named(&lib, "Full Multi-Modal RCT"))).unwrap_err();
    assert!(err.contains("exceeds firmware maximum of 25%"), "{err}");
}

// @verifies SWR-PL-01
#[test]
fn a_change_to_what_is_delivered_changes_the_hash() {
    let lib = library();
    let mut c = named(&lib, "Sleep Wind-Down");
    c["conflictResolution"] = json!("merge"); // a sequential composite does not read its authored starts
    let h = content_hash_json(&hash_req(&lib, &c)).unwrap();
    let mut later = c.clone();
    later["layers"][1]["startOffsetSeconds"] = json!(later["layers"][1]["startOffsetSeconds"].as_f64().unwrap() + 10.0);
    assert_ne!(h, content_hash_json(&hash_req(&lib, &later)).unwrap());
    let mut scaled = c.clone();
    scaled["layers"][0]["intensityScale"] = json!(0.5);
    assert_ne!(h, content_hash_json(&hash_req(&lib, &scaled)).unwrap());
    let mut shorter = c.clone();
    shorter["layers"][0]["durationSeconds"] = json!(60);
    assert_ne!(h, content_hash_json(&hash_req(&lib, &shorter)).unwrap());
    // An edit to a member's content changes the composite's identity.
    let mut lib2 = library();
    let first = c["layers"][0]["protocolName"].as_str().unwrap().to_string();
    let m = lib2.members.get_mut(&first).unwrap();
    m["modalities"][0]["params"]["frequencyHz"] = json!(m["modalities"][0]["params"]["frequencyHz"].as_f64().unwrap_or(10.0) + 1.0);
    assert_ne!(h, content_hash_json(&hash_req(&lib2, &c)).unwrap());
}

// @verifies SWR-PL-01
#[test]
fn sequential_layers_run_end_to_end_and_the_authored_start_is_not_read() {
    let lib = library();
    let c = named(&lib, "Sleep Wind-Down");
    let mut seq = c.clone();
    seq["conflictResolution"] = json!("sequential");
    let mut moved = seq.clone();
    for l in moved["layers"].as_array_mut().unwrap() {
        l["startOffsetSeconds"] = json!(9999);
    }
    assert_eq!(
        content_hash_json(&hash_req(&lib, &seq)).unwrap(),
        content_hash_json(&hash_req(&lib, &moved)).unwrap()
    );
    assert!(compile_json(&compile_req(&lib, &seq)).is_ok());
}

// @verifies SWR-PL-01
#[test]
fn a_layer_is_refused_when_it_cannot_be_laid_out() {
    let lib = library();
    let name = lib.members.keys().next().unwrap().clone();
    let one = |l: Value| json!({ "name": "T", "conflictResolution": "merge", "layers": [l] });
    for (l, what) in [
        (layer(&name, 0.0, Some(60.0), 0.0), "above 0"),
        (layer(&name, 0.0, Some(60.0), 1.5), "at most 1"),
        (layer("No Such Protocol", 0.0, Some(60.0), 1.0), "not in the library"),
        (layer(&name, 0.0, Some(1.0e9), 1.0), "longer than its protocol"),
    ] {
        let e = compile_json(&compile_req(&lib, &one(l))).unwrap_err();
        assert!(e.contains(what), "{what}: {e}");
    }
    let _ = two_layer(&lib, "merge", json!(null), json!(null));
}

// @verifies SWR-PL-01
#[test]
fn override_cuts_the_earlier_layer_where_the_later_one_takes_over() {
    let lib = library();
    let mut c = named(&lib, "Calm to Focus");
    c["conflictResolution"] = json!("override");
    assert!(compile_json(&compile_req(&lib, &c)).is_ok(), "the overlap is resolved by the later layer, not refused");
    let mut merged = c.clone();
    merged["conflictResolution"] = json!("merge");
    assert_ne!(content_hash_json(&hash_req(&lib, &c)).unwrap(), content_hash_json(&hash_req(&lib, &merged)).unwrap());
}

fn pbm_member(name: &str, wavelength: &str, hz: f64, zone: &str) -> Value {
    json!({
        "name": name, "timingMode": { "type": "duration", "seconds": 600 },
        "modalities": [{
            "type": "pbm_transcranial", "enabled": true,
            "params": { "zones": "named", "zoneRefs": [zone], "wavelength": wavelength,
                        "irradianceMWcm2": 100, "frequencyHz": hz, "dutyCyclePercent": 25 },
            "interval": { "intervalOnSeconds": 0, "intervalOffSeconds": 0 }
        }]
    })
}

fn two_pbm(lib: &mut Library, a: Value, b: Value) -> Value {
    for m in [&a, &b] {
        lib.members.insert(m["name"].as_str().unwrap().to_string(), m.clone());
    }
    json!({ "name": "T", "conflictResolution": "merge", "layers": [
        layer(a["name"].as_str().unwrap(), 0.0, None, 1.0), layer(b["name"].as_str().unwrap(), 0.0, None, 1.0) ] })
}

fn disjoint_zones(lib: &Library) -> (String, String) {
    let z = lib.zones.as_object().unwrap();
    let ids = |v: &Value| -> Vec<u64> { v.as_array().unwrap().iter().map(|n| n.as_u64().unwrap()).collect() };
    for (a, av) in z {
        for (b, bv) in z {
            let (x, y) = (ids(av), ids(bv));
            if a < b && !x.is_empty() && !y.is_empty() && !x.iter().any(|k| y.contains(k)) {
                return (a.clone(), b.clone());
            }
        }
    }
    panic!("the shipped zones have no disjoint pair");
}

// @verifies SWR-PL-01
#[test]
fn pbm_layers_overlap_only_on_sockets_they_do_not_share() {
    // Two layers whose channels would be merged onto one tile are refused, even when window, frequency and duty match and
    // the wavelengths differ: the weighted sum over the merged channels is not checked yet (OI-NPPS-COMP-01).
    let mut lib = library();
    let shared = two_pbm(&mut lib, pbm_member("PBM Red", "660nm", 40.0, "All"), pbm_member("PBM NIR", "808nm", 40.0, "All"));
    let e = compile_json(&compile_req(&lib, &shared)).unwrap_err();
    assert!(e.contains("refused rather than summed or reduced") && e.contains("do not share"), "{e}");

    // Sockets that share none pass.
    let mut lib = library();
    let (za, zb) = disjoint_zones(&lib);
    let apart = two_pbm(&mut lib, pbm_member("PBM Red", "660nm", 40.0, &za), pbm_member("PBM NIR", "808nm", 40.0, &zb));
    assert!(compile_json(&compile_req(&lib, &apart)).is_ok(), "{za} and {zb} share no socket");
}
