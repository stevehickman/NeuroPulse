//! `SWR-HV-01`, `SWR-HV-05`, `SWR-WO-03`: a protocol's identity is the hash of its compiled content, and the
//! inputs the requirement excludes cannot move it.

use neurone_npps_core::api::{compile_json, content_hash_json};
use neurone_npps_core::identity;
use neurone_npps_core::parse_file;
use serde_json::{json, Value};

fn def() -> Value {
    json!({
        "name": "Test", "familyId": "11111111-2222-3333-4444-555555555555",
        "timingMode": { "type": "duration", "seconds": 600 },
        "modalities": [{
            "type": "pbm_transcranial", "enabled": true,
            "params": { "zones": "named", "zoneRefs": ["Frontal"], "wavelength": "660nm",
                        "irradianceMWcm2": 200, "frequencyHz": 40, "dutyCyclePercent": 25 },
            "interval": { "intervalOnSeconds": 0, "intervalOffSeconds": 0 }
        }]
    })
}

fn zones_by_version() -> Value {
    json!({ "Frontal": { "1": [3, 1, 2], "2": [10, 11] }, "Occipital": { "1": [7] } })
}

fn req(def: Value) -> Value {
    json!({
        "def": def, "hardwareVersion": 1, "zonesByVersion": zones_by_version(), "clinicianSockets": null,
        "deviceSerialHex": "aabbccdd", "nowUnix": 1_000, "sessionUuidHex": "00112233445566778899aabbccddeeff",
        "wavelengthRules": null, "autonomous": false, "acknowledgedCautions": []
    })
}

fn hash(r: &Value) -> String {
    content_hash_json(&r.to_string()).unwrap()
}

// @verifies SWR-HV-01
#[test]
fn hash_is_64_lowercase_hex() {
    let h = hash(&req(def()));
    assert_eq!(h.len(), 64);
    assert!(h.chars().all(|c| c.is_ascii_digit() || ('a'..='f').contains(&c)));
}

// @verifies SWR-HV-01
#[test]
fn excluded_inputs_do_not_move_the_hash() {
    let base = hash(&req(def()));
    let mut r = req(def());
    r["deviceSerialHex"] = json!("0102030405060708");
    r["nowUnix"] = json!(99_999);
    r["sessionUuidHex"] = json!("ffeeddccbbaa99887766554433221100");
    r["autonomous"] = json!(true);
    r["acknowledgedCautions"] = json!(["some-caution"]);
    r["wavelengthRules"] = json!({ "name": "v9", "channels": [] });
    assert_eq!(hash(&r), base, "serial, time, session, Mode 3, cautions and rules are not content");
}

// @verifies SWR-WO-03
#[test]
fn metadata_and_used_flag_do_not_move_the_hash() {
    let base = hash(&req(def()));
    let mut d = def();
    d["familyId"] = json!("99999999-2222-3333-4444-555555555555");
    d["isUsed"] = json!(true);
    d["name"] = json!("Renamed");
    d["description"] = json!("changed");
    d["version"] = json!("2.7");
    d["id"] = json!("another");
    assert_eq!(hash(&req(d)), base);
}

// @verifies SWR-HV-01
#[test]
fn socket_order_and_repeats_do_not_move_the_hash_but_membership_does() {
    let base = hash(&req(def()));
    let mut r = req(def());
    r["zonesByVersion"]["Frontal"]["1"] = json!([1, 2, 3, 3, 2]);
    assert_eq!(hash(&r), base, "a zone is a set");
    r["zonesByVersion"]["Frontal"]["1"] = json!([1, 2, 4]);
    assert_ne!(hash(&r), base, "editing a zone's sockets changes the identity of a protocol that names it (H1 (v))");
}

// @verifies SWR-HV-01
#[test]
fn changing_an_input_the_requirement_names_changes_the_hash() {
    let base = hash(&req(def()));
    let mut d = def();
    d["modalities"][0]["params"]["wavelength"] = json!("808nm");
    assert_ne!(hash(&req(d)), base, "the requested wavelength");
    let mut r = req(def());
    r["hardwareVersion"] = json!(2);
    assert_ne!(hash(&r), base, "the hardware version");
    let mut d = def();
    d["modalities"][0]["params"]["zoneRefs"] = json!(["Occipital"]);
    assert_ne!(hash(&req(d)), base, "the socket list");
    let mut d = def();
    d["modalities"][0]["params"]["irradianceMWcm2"] = json!(201);
    assert_ne!(hash(&req(d)), base, "the dose");
    let mut d = def();
    d["timingMode"]["seconds"] = json!(601);
    assert_ne!(hash(&req(d)), base, "the timing");
}

// @verifies SWR-HV-01
#[test]
fn a_disabled_modality_is_not_content() {
    let mut d = def();
    let mut extra = d["modalities"][0].clone();
    extra["enabled"] = json!(false);
    extra["params"]["wavelength"] = json!("808nm");
    d["modalities"].as_array_mut().unwrap().push(extra);
    assert_eq!(hash(&req(d)), hash(&req(def())));
}

// @verifies SWR-HV-01
#[test]
fn a_zone_named_by_name_and_the_same_sockets_chosen_by_the_clinician_are_one_protocol() {
    let mut d = def();
    d["modalities"][0]["params"]["zones"] = json!("clinician_selected");
    d["modalities"][0]["params"].as_object_mut().unwrap().remove("zoneRefs");
    let mut r = req(d);
    r["clinicianSockets"] = json!([2, 1, 3]);
    assert_eq!(hash(&r), hash(&req(def())), "the hash is over the resolved socket list");
}

// @verifies SWR-HV-05
#[test]
fn version_two_resolves_its_own_list() {
    let d = def();
    let mut r = req(d.clone());
    r["hardwareVersion"] = json!(2);
    assert!(content_hash_json(&r.to_string()).is_ok());
    let c = identity::canonical_content(&d, Some(2), &[("Frontal".to_string(), vec![10, 11])].into_iter().collect(), None).unwrap();
    assert_eq!(c["modalities"][0]["params"]["sockets"], json!([10, 11]));
}

// @verifies SWR-HV-05
#[test]
fn a_zone_with_no_list_for_the_helmets_version_is_refused_by_hash_and_by_compile() {
    let mut d = def();
    d["modalities"][0]["params"]["zoneRefs"] = json!(["Occipital"]);
    let mut r = req(d);
    r["hardwareVersion"] = json!(2);
    let want = "Protocol 'Test' is for hardware version 2, and zone 'Occipital' has no socket list for it";
    assert_eq!(content_hash_json(&r.to_string()).unwrap_err(), want);
    assert_eq!(compile_json(&r.to_string()).unwrap_err(), want);
}

// @verifies SWR-HV-05
#[test]
fn compile_resolves_zones_by_version_to_the_same_bytes_as_a_flat_zone_map() {
    let by_version = compile_json(&req(def()).to_string()).unwrap();
    let mut flat = req(def());
    flat.as_object_mut().unwrap().remove("zonesByVersion");
    flat["zones"] = json!({ "Frontal": [3, 1, 2] });
    assert_eq!(by_version, compile_json(&flat.to_string()).unwrap());
}

// @verifies SWR-HV-01
#[test]
fn json_keys_are_written_sorted_so_the_canonical_form_is_stable() {
    // `serde_json::Map` is a BTreeMap unless the `preserve_order` feature is on, which would make key order
    // depend on how a caller built the object. If this fails, the hash is no longer a function of the content.
    let a: Value = serde_json::from_str(r#"{"b":1,"a":2}"#).unwrap();
    assert_eq!(a.to_string(), r#"{"a":2,"b":1}"#);
}

// @verifies SWR-HV-01
#[test]
fn the_hash_has_a_known_value() {
    // Pins the canonical form and the domain: a change to either changes this string on purpose, with a new DOMAIN.
    let zones = [("Frontal".to_string(), vec![1, 2, 3])].into_iter().collect();
    let c = identity::canonical_content(&def(), Some(1), &zones, None).unwrap();
    assert_eq!(
        String::from_utf8(identity::canonical_bytes(&c)).unwrap(),
        "NeurOne.ProtocolContent.1\n{\"hardwareVersion\":1,\"modalities\":[{\"interval\":{\"intervalOffSeconds\":0,\"intervalOnSeconds\":0},\"params\":{\"dutyCyclePercent\":25,\"frequencyHz\":40,\"irradianceMWcm2\":200,\"sockets\":[1,2,3],\"wavelength\":\"660nm\"},\"type\":\"pbm_transcranial\"}],\"timingMode\":{\"seconds\":600,\"type\":\"duration\"}}"
    );
}

// @verifies SWR-WO-03
#[test]
fn used_flag_round_trips_through_parse_and_write_and_is_not_content() {
    let src = r#"protocol "P" {
  id: "p1"
  family_id: "11111111-2222-3333-4444-555555555555"
  used: true
  duration: 10m
  pbm_transcranial {
    zones: [All]
    wavelength: "660nm"
    irradiance: 200mW_cm2
  }
}"#;
    let f = parse_file(src).unwrap();
    let p = match &f.entries[0] {
        neurone_npps_core::Entry::Single(p) => p.clone(),
        _ => panic!("single"),
    };
    assert_eq!(p["isUsed"], json!(true));
    let text = neurone_npps_core::api::serialize_json(&json!({ "items": [{ "kind": "single", "protocol": p }] }).to_string()).unwrap();
    assert!(text.contains("used: true"));
    let again = parse_file(&text).unwrap();
    match &again.entries[0] {
        neurone_npps_core::Entry::Single(q) => assert_eq!(q["isUsed"], json!(true)),
        _ => panic!("single"),
    }
    // Absent or false is not written.
    let unused = src.replace("  used: true\n", "  used: false\n");
    let q = match &parse_file(&unused).unwrap().entries[0] {
        neurone_npps_core::Entry::Single(q) => q.clone(),
        _ => panic!("single"),
    };
    assert!(q.get("isUsed").is_none());
}
