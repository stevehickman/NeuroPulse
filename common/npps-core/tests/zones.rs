//! The zone model (`docs/reference/safety-zones.md`): safe runs, caution runs after an acknowledgement, danger never runs.

use neurone_npps_core::api::{compile_json, validate_json};
use neurone_npps_core::zones::{evaluate, protocol_zone, Zone};
use serde_json::{json, Value};

const UUID: &str = "00112233445566778899aabbccddeeff";

fn protocol(ty: &str, params: Value, interval: Value, seconds: u32) -> Value {
    json!({ "timingMode": { "type": "duration", "seconds": seconds },
            "modalities": [{ "type": ty, "params": params, "interval": interval }] })
}

fn compile(def: &Value, ack: &[String]) -> Result<Vec<u8>, String> {
    compile_json(&json!({ "def": def, "nowUnix": 1, "sessionUuidHex": UUID, "acknowledgedCautions": ack }).to_string())
}

fn validate(def: &Value) -> Value {
    let req = json!({ "entry": { "kind": "single", "protocol": def }, "limits": {} });
    serde_json::from_str(&validate_json(&req.to_string()).unwrap()).unwrap()
}

// 1 Hz sinusoidal tACS at 1 mA: 318 µC a phase on the 25 cm² pad is 12.7 µC/cm², between the caution and the danger boundary.
fn tacs_1hz(ma: f64) -> Value {
    protocol("bes_tacs", json!({ "frequencyHz": 1, "intensityMilliamps": ma, "waveform": "sinusoidal" }), json!({ "intervalOnSeconds": 0 }), 600)
}

// @verifies REQ-NPPS-21
#[test]
fn a_protocol_inside_every_boundary_is_safe_and_compiles_without_a_word() {
    let def = protocol("vns_hrv", json!({ "frequencyHz": 20, "intensityMilliamps": 0.8, "hrvProtocol": "standalone" }), json!({ "intervalOnSeconds": 0 }), 600);
    assert_eq!(protocol_zone(&evaluate(&def)), Zone::Safe);
    assert_eq!(validate(&def)["zone"], "safe");
    assert!(compile(&def, &[]).is_ok());
}

// @verifies REQ-NPPS-21
// @verifies SWR-SS-02#a,b
#[test]
fn caution_compiles_only_for_the_acknowledgement_of_that_dose() {
    let def = tacs_1hz(1.0);
    let v = validate(&def);
    assert_eq!(v["zone"], "caution");
    assert!(v["issues"].as_array().unwrap().iter().any(|i| i["severity"] == "warning" && i["message"]["key"] == "VALIDATE_MSG_ZONE_CAUTION"));
    let id = v["zones"][0]["axes"][0]["ackId"].as_str().expect("a caution carries its acknowledgement id").to_string();

    let refused = compile(&def, &[]).expect_err("an unacknowledged caution is refused");
    assert!(refused.contains("caution zone") && refused.contains(&id), "{refused:?}");
    assert!(compile(&def, &[id.clone()]).is_ok(), "the acknowledgement admits it");
    // The id names the value: once the dose changes, the old acknowledgement no longer matches.
    assert!(compile(&tacs_1hz(1.0001), &[id]).is_err(), "an acknowledgement is of one dose, not of the protocol");
}

// @verifies REQ-NPPS-14#a REQ-NPPS-14#d
#[test]
fn danger_is_refused_whatever_is_acknowledged() {
    // 2 mA of DC for an hour on a 35 cm² pad is 205 mC/cm², over the 150 mC/cm² ceiling.
    let def = protocol("tdcs", json!({ "intensityMilliamps": 2, "electrodePairs": [["F3", "F4"]], "electrodeAreaCm2": 35, "rampSeconds": 30 }), json!({ "intervalOnSeconds": 0 }), 3600);
    assert_eq!(protocol_zone(&evaluate(&def)), Zone::Danger);
    assert_eq!(validate(&def)["zone"], "danger");
    let all: Vec<String> = validate(&def)["zones"][0]["axes"].as_array().unwrap().iter().filter_map(|a| a["ackId"].as_str().map(str::to_string)).collect();
    let e = compile(&def, &all).expect_err("danger is refused");
    assert!(e.contains("danger zone"), "{e:?}");
}

// @verifies REQ-NPPS-21
#[test]
fn frequency_and_duty_enter_through_the_current_densities() {
    let vns = |interval: Value| protocol("vns_hrv", json!({ "frequencyHz": 30, "intensityMilliamps": 5, "pulseWidthUs": 300 }), interval, 600);
    let axis = |def: &Value, id: &str| evaluate(def)[0].axes.iter().find(|a| a.id == id).map(|a| (a.value, a.zone())).unwrap();
    // 5 mA at 300 µs and 30 Hz, running throughout: 1.3 mA/cm² rms on the 0.5 cm² clip, past the caution boundary.
    let continuous = vns(json!({ "intervalOnSeconds": 0 }));
    assert_eq!(axis(&continuous, "rmsCurrentDensity").1, Zone::Caution);
    // The same pulses gated 30 s on, 270 s off (Capone 2017's pattern) carry a tenth of the heating: safe.
    let gated = vns(json!({ "intervalOnSeconds": 30, "intervalOffSeconds": 270 }));
    assert_eq!(axis(&gated, "rmsCurrentDensity").1, Zone::Safe);
    // Charge per phase does not move with frequency or duty: the old per-phase check alone could not tell them apart.
    assert_eq!(axis(&continuous, "phaseChargeDensity").0, axis(&gated, "phaseChargeDensity").0);
    // At a current no study approaches, the same axis reaches its danger boundary.
    let extreme = protocol("vns_hrv", json!({ "frequencyHz": 30, "intensityMilliamps": 20, "pulseWidthUs": 300 }), json!({ "intervalOnSeconds": 0 }), 600);
    assert_eq!(axis(&extreme, "rmsCurrentDensity").1, Zone::Danger);
}

// @verifies REQ-NPPS-21
#[test]
fn a_block_that_drives_no_electrode_or_lacks_a_parameter_has_no_axes() {
    // The session-length axis (SWR-SS-02) belongs to the session, so it is the one axis such a block can carry.
    let only_session_length = |def: &Value| evaluate(def).iter().all(|b| b.axes.iter().all(|a| a.id == "sessionDuration"));
    assert!(only_session_length(&protocol("eeg_neurofeedback", json!({}), json!({}), 600)));
    assert!(only_session_length(&protocol("bes_tacs", json!({ "frequencyHz": 10 }), json!({}), 600)), "a missing amplitude is the validator's to report");
}

// ── SWR-SS-02: a session past the long-session level (OI-HLIM-01, the principal, 2026-11-13) ────────────────────────────
// No health limit on session length is defined for any modality (docs/reference/health-limits.md), so the rule is one
// general acknowledgement above one level, for every modality but audio. It is not presented as a health limit.

const AUDIO: &str = r#"{ "binauralBeatsHz": 10, "carrierHz": 440, "volumeDb": 60, "eegAdaptive": false, "boneConductionPacer": false }"#;

fn long_tacs(minutes: f64) -> Value {
    protocol("bes_tacs", json!({ "frequencyHz": 10, "intensityMilliamps": 0.5, "waveform": "sine" }), json!({ "intervalOnSeconds": 0 }), (minutes * 60.0) as u32)
}

fn issues_with(v: &Value, key: &str) -> Vec<Value> {
    v["issues"].as_array().unwrap().iter().filter(|i| i["message"]["key"] == key).cloned().collect()
}

// @verifies SWR-SS-02#a
// @verifies SWR-SS-02#b
#[test]
fn a_session_past_the_long_session_level_runs_only_with_the_acknowledgement_of_that_length() {
    let def = long_tacs(180.0);
    assert_eq!(validate(&def)["zone"], "caution");
    let id = "0:bes_tacs:sessionDuration=180.000".to_string();
    let refused = compile(&def, &[]).expect_err("an unacknowledged long session does not sign");
    assert!(refused.contains("has not been acknowledged") && refused.contains(&id), "{refused:?}");
    assert!(compile(&def, &[id.clone()]).is_ok(), "the acknowledgement admits it");
    // The id names the length: a typo edited in afterwards (180 → 1800 minutes) needs a new acknowledgement.
    assert!(compile(&long_tacs(1800.0), &[id]).is_err(), "an acknowledgement is of one length");
}

// @verifies SWR-SS-02#a
#[test]
fn the_level_itself_is_not_past_the_level() {
    // "More than two hours", as the advisory always read: a session of exactly 120 minutes needs nothing.
    assert_eq!(validate(&long_tacs(120.0))["zone"], "safe");
    assert!(compile(&long_tacs(120.0), &[]).is_ok());
    assert_eq!(validate(&long_tacs(120.0 + 1.0 / 60.0))["zone"], "caution", "one second over is over");
}

// @verifies SWR-SS-02#c
#[test]
fn the_request_names_the_level_and_the_amount_over_it_and_says_no_health_limit_is_defined() {
    let v = validate(&long_tacs(150.0));
    let issues = issues_with(&v, "VALIDATE_MSG_LONG_SESSION_CAUTION");
    assert_eq!(issues.len(), 1, "one caution, and not also the old advisory: {v}");
    let args: Vec<String> = issues[0]["message"]["args"].as_array().unwrap().iter().map(|a| a["value"].as_str().or(a.as_str()).unwrap_or("").to_string()).collect();
    assert_eq!(&args[..3], ["150.00 min", "120.00 min", "30.00 min"], "the session, the level, the amount over");
    assert!(args[3].ends_with("sessionDuration=150.000"), "{args:?}");
    assert!(v["issues"].as_array().unwrap().iter().all(|i| i["message"]["key"] != "VALIDATE_MSG_GENERAL_DURATION_2"));
    // The English says what the level is not.
    let en: Value = serde_json::from_str(&std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/../../locales/en.json")).unwrap()).unwrap();
    assert!(en["VALIDATE_MSG_LONG_SESSION_CAUTION"].as_str().unwrap().contains("No health limit on session length is defined"));
    // The compiler's refusal carries the same figures.
    let refused = compile(&long_tacs(150.0), &[]).unwrap_err();
    assert!(refused.contains("150 min") && refused.contains("120 min"), "{refused:?}");
}

// @verifies SWR-SS-02#a
#[test]
fn every_modality_but_audio_falls_under_the_long_session_level() {
    for ty in ["eeg_neurofeedback", "pbm_transcranial", "vns_hrv", "tdcs", "cervical_vns"] {
        let blocks = evaluate(&protocol(ty, json!({}), json!({}), 3 * 3600));
        assert_eq!(protocol_zone(&blocks), Zone::Caution, "{ty}: a three-hour session needs the acknowledgement");
        assert!(blocks.iter().flat_map(|b| &b.axes).any(|a| a.id == "sessionDuration" && a.zone() == Zone::Caution), "{ty}");
    }
}

// @verifies SWR-SS-02#a
#[test]
fn audio_is_outside_the_rule_its_level_ceiling_is_the_whole_control() {
    let audio: Value = serde_json::from_str(AUDIO).unwrap();
    let alone = protocol("audio_entrainment", audio.clone(), json!({ "intervalOnSeconds": 0 }), 3 * 3600);
    assert_eq!(protocol_zone(&evaluate(&alone)), Zone::Safe, "audio alone has no session-length axis");
    assert!(compile(&alone, &[]).is_ok(), "and runs without an acknowledgement");
    // The advisory the validator always gave a long session stays for it: a warning, asking for nothing.
    let v = validate(&alone);
    assert_eq!(issues_with(&v, "VALIDATE_MSG_GENERAL_DURATION_2").len(), 1);
    assert_eq!(v["zone"], "safe");
    // Audio with another modality is a session of that modality too: the caution is carried on the block that is not audio.
    let mixed = json!({ "timingMode": { "type": "duration", "seconds": 3 * 3600 }, "modalities": [
        { "type": "audio_entrainment", "params": audio, "interval": { "intervalOnSeconds": 0 } },
        { "type": "bes_tacs", "params": { "frequencyHz": 10, "intensityMilliamps": 0.5, "waveform": "sine" }, "interval": { "intervalOnSeconds": 0 } } ] });
    let blocks = evaluate(&mixed);
    let carrier = blocks.iter().find(|b| b.axes.iter().any(|a| a.id == "sessionDuration")).expect("the session-length axis is carried");
    assert_eq!((carrier.index, carrier.kind.as_str()), (1, "bes_tacs"));
    assert!(compile(&mixed, &[]).is_err());
    assert!(compile(&mixed, &["1:bes_tacs:sessionDuration=180.000".to_string()]).is_ok());
}
