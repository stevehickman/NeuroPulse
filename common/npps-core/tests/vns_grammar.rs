//! The study parameters of a `vns_hrv` block: side, pulse width, trigger, burst, intensity basis, and the course
//! (`sessions_per_week`, `course_weeks`) a protocol is delivered over (NP-NPPS-REF-001 §4.6, OI-NPPS-VNS-01).

use neurone_npps_core::api::{compile_json, parse_json, serialize_json, validate_json};
use serde_json::{json, Value};

const UUID: &str = "00112233445566778899aabbccddeeff";

fn protocol_text(block: &str, header: &str) -> String {
    format!("protocol \"VNS study\" {{\n    duration: 10m\n{header}\n    vns_hrv {{\n{block}\n    }}\n}}\n")
}

fn parse(block: &str, header: &str) -> Value {
    let out: Value = serde_json::from_str(&parse_json(&protocol_text(block, header)).expect("parses")).unwrap();
    out["entries"][0]["protocol"].clone()
}

fn compile(protocol: &Value) -> Result<Vec<u8>, String> {
    let req = json!({ "def": protocol, "nowUnix": 1, "sessionUuidHex": UUID });
    compile_json(&req.to_string())
}

const FULL: &str = "        frequency: 30Hz\n        intensity: 0.8mA\n        side: bilateral\n        pulse_width_us: 300\n        trigger: movement_cue\n        burst: 0.5s\n        intensity_basis: perceptual_threshold\n        intensity_percent_of_threshold: 200%";

// @verifies REQ-NPPS-07
#[test]
fn every_study_parameter_parses_into_the_model() {
    let p = parse(FULL, "    sessions_per_week: 3\n    course_weeks: 6");
    let v = &p["modalities"][0]["params"];
    assert_eq!(v["side"], "bilateral");
    assert_eq!(v["pulseWidthUs"], 300);
    assert_eq!(v["trigger"], "movement_cue");
    assert_eq!(v["burstSeconds"], 0.5);
    assert_eq!(v["intensityBasis"], "perceptual_threshold");
    assert_eq!(v["intensityPercentOfThreshold"], 200);
    assert_eq!((p["sessionsPerWeek"].clone(), p["courseWeeks"].clone()), (json!(3), json!(6)));
}

// @verifies REQ-NPPS-07
#[test]
fn a_block_that_authors_none_of_them_is_unchanged() {
    let p = parse("        frequency: 25Hz\n        intensity: 0.5mA", "");
    let v = p["modalities"][0]["params"].as_object().unwrap();
    for k in ["side", "pulseWidthUs", "trigger", "burstSeconds", "intensityBasis", "intensityPercentOfThreshold"] {
        assert!(!v.contains_key(k), "{k} must be absent unless authored");
    }
    assert!(p.get("sessionsPerWeek").is_none() && p.get("courseWeeks").is_none());
}

// @verifies REQ-NPPS-07
#[test]
fn the_study_parameters_survive_a_round_trip() {
    let p = parse(FULL, "    sessions_per_week: 3\n    course_weeks: 6");
    let text = serialize_json(&json!({ "items": [{ "kind": "single", "protocol": p }] }).to_string()).unwrap();
    let again: Value = serde_json::from_str(&parse_json(&text).expect("the written file parses")).unwrap();
    assert_eq!(again["entries"][0]["protocol"]["modalities"], p["modalities"]);
    assert_eq!(again["entries"][0]["protocol"]["sessionsPerWeek"], p["sessionsPerWeek"]);
    assert_eq!(again["entries"][0]["protocol"]["courseWeeks"], p["courseWeeks"]);
}

// @verifies REQ-NPPS-07
#[test]
fn pulse_width_and_side_reach_the_wire() {
    let ok = |extra: &str| compile(&parse(&format!("        frequency: 25Hz\n        intensity: 0.8mA\n{extra}"), "")).unwrap();
    // 68-byte header + 14-byte command header, then: side, freq mHz (LE32), amp µA (LE16), pulse width µs (LE16), ppg, eeg, hrv.
    let params = |b: &[u8]| b[82..94].to_vec();
    assert_eq!(params(&ok("")), [0, 0xA8, 0x61, 0, 0, 0x20, 0x03, 0, 0, 1, 1, 0], "no authored width is 0, the firmware's 250 µs");
    assert_eq!(params(&ok("        pulse_width_us: 300\n        side: right")), [1, 0xA8, 0x61, 0, 0, 0x20, 0x03, 0x2C, 0x01, 1, 1, 0], "300 µs does not fit a byte");
    assert_eq!(params(&ok("        pulse_width: 500\n        side: bilateral"))[7..9], [0xF4, 0x01], "the `pulse_width` spelling reads the same field");
}

// @verifies REQ-NPPS-07
#[test]
fn what_the_hub_cannot_deliver_is_refused_not_run_as_something_else() {
    let refused = |extra: &str, needle: &str| {
        let e = compile(&parse(&format!("        frequency: 25Hz\n        intensity: 0.8mA\n{extra}"), "")).expect_err("refused");
        assert!(e.contains(needle), "{e:?} lacks {needle:?}");
    };
    refused("        trigger: movement_cue\n        burst: 0.5s", "no movement-trigger input");
    refused("        burst: 0.5s", "burst_seconds needs trigger");
    refused("        intensity_basis: pain_threshold\n        intensity_percent_of_threshold: 90", "sensory-threshold calibration");
    refused("        pulse_width_us: 49", "outside the authored range 50–500");
    refused("        pulse_width_us: 501", "outside the authored range 50–500");
    refused("        side: both", "not left, right or bilateral");
}

fn issues(block: &str, header: &str) -> Vec<Value> {
    let p = parse(block, header);
    let req = json!({ "entry": { "kind": "single", "protocol": p }, "limits": {} });
    let out: Value = serde_json::from_str(&validate_json(&req.to_string()).unwrap()).unwrap();
    out["issues"].as_array().unwrap().clone()
}

fn has(issues: &[Value], key: &str) -> bool {
    issues.iter().any(|i| i["message"]["key"] == key && i["severity"] == "error")
}

// @verifies REQ-NPPS-07
#[test]
fn the_validator_reports_each_of_them_before_signing() {
    let base = "        frequency: 25Hz\n        intensity: 0.8mA\n";
    assert!(has(&issues(&format!("{base}        pulse_width_us: 600"), ""), "VALIDATE_MSG_VNS_HRV_PULSEWIDTH"));
    assert!(has(&issues(&format!("{base}        side: both"), ""), "VALIDATE_MSG_VNS_HRV_ENUM"));
    assert!(has(&issues(&format!("{base}        trigger: movement_cue\n        burst: 0.5s"), ""), "VALIDATE_MSG_VNS_HRV_NOT_DELIVERABLE"));
    assert!(has(&issues(&format!("{base}        trigger: movement_cue"), ""), "VALIDATE_MSG_VNS_HRV_TRIGGER_NEEDS_BURST"));
    assert!(has(&issues(&format!("{base}        burst: 0.5s"), ""), "VALIDATE_MSG_VNS_HRV_BURST_NEEDS_TRIGGER"));
    assert!(has(&issues(&format!("{base}        intensity_basis: perceptual_threshold"), ""), "VALIDATE_MSG_VNS_HRV_BASIS_NEEDS_PERCENT"));
    assert!(has(&issues(base, "    sessions_per_week: 2.5"), "VALIDATE_MSG_GENERAL_POSITIVE_WHOLE"));
    assert!(has(&issues(base, "    course_weeks: 0"), "VALIDATE_MSG_GENERAL_POSITIVE_WHOLE"));
    let clean = issues(&format!("{base}        pulse_width_us: 300\n        side: left"), "    sessions_per_week: 3\n    course_weeks: 6");
    assert!(!clean.iter().any(|i| i["severity"] == "error"), "an in-range, deliverable block is clean: {clean:?}");
}

// @verifies REQ-NPPS-07
#[test]
fn a_titrated_basis_is_a_band_with_a_cap_and_is_refused_until_it_can_be_calibrated() {
    let block = "        frequency: 20Hz\n        intensity: 2.0mA\n        intensity_basis: titrated";
    let p = parse(block, "");
    assert_eq!(p["modalities"][0]["params"]["intensityBasis"], "titrated");
    assert!(p["modalities"][0]["params"].get("intensityPercentOfThreshold").is_none(), "titrated carries no percentage");
    let text = serialize_json(&json!({ "items": [{ "kind": "single", "protocol": p.clone() }] }).to_string()).unwrap();
    let again: Value = serde_json::from_str(&parse_json(&text).unwrap()).unwrap();
    assert_eq!(again["entries"][0]["protocol"]["modalities"], p["modalities"]);
    let found = issues(block, "");
    assert!(has(&found, "VALIDATE_MSG_VNS_HRV_NOT_DELIVERABLE"), "not deliverable yet");
    assert!(!has(&found, "VALIDATE_MSG_VNS_HRV_BASIS_NEEDS_PERCENT"), "and it needs no percentage");
    let e = compile(&p).expect_err("refused");
    assert!(e.contains("sensory-threshold calibration"), "{e:?}");
    // The 2 mA ceiling is gone (Rev 67) and the 40 mA stop-gap holds (Rev 68, #554, #559): 3 mA is not an intensity
    // error, 41 mA is.
    let err = |ma: &str| issues(&format!("        frequency: 20Hz\n        intensity: {ma}mA\n        intensity_basis: titrated"), "")
        .iter().any(|i| i["parameterKey"] == "intensityMilliamps" && i["severity"] == "error");
    assert!(!err("3.0"), "3 mA is inside the 40 mA ceiling");
    assert!(err("41.0"), "41 mA is over it");
}

// @verifies REQ-NPPS-24
#[test]
fn a_field_the_block_does_not_have_is_an_error_naming_it() {
    // A misspelt study parameter used to be accepted and run as the default.
    let text = protocol_text("        frequency: 25Hz\n        intensity: 0.8mA\n        pulse_widht_us: 300", "");
    let e = parse_json(&text).expect_err("an unknown key is refused");
    assert!(e.contains("'pulse_widht_us' is not a field of this modality"), "{e}");
    assert!(e.contains("pulse_width_us"), "the message lists the real fields: {e}");
    assert!(e.contains("not run with that field ignored"), "{e}");
}

// @verifies REQ-NPPS-24
#[test]
fn a_field_that_belongs_to_another_modality_is_also_refused() {
    // `trigger` is a vns_hrv field; a tdcs block does not have it.
    let text = "protocol \"x\" {\n    duration: 10m\n    tdcs {\n        intensity: 1mA\n        electrode_area_cm2: 35\n        trigger: movement_cue\n    }\n}\n";
    let e = parse_json(text).expect_err("refused");
    assert!(e.contains("tdcs: 'trigger' is not a field of this modality"), "{e}");
}

// @verifies REQ-NPPS-24
#[test]
fn aliases_and_block_keys_are_still_accepted() {
    // `frequency`, `pulse_width` and `burst` are aliases; `interval_on`, `interval_off`, `start` and `enabled` are block keys.
    let text = protocol_text(
        "        frequency: 25Hz\n        intensity: 0.8mA\n        pulse_width: 300\n        interval_on: 30s\n        interval_off: 30s\n        enabled: true",
        "",
    );
    assert!(parse_json(&text).is_ok(), "aliases and block keys must still parse");
}
