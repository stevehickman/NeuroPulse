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

#[test]
fn a_protocol_inside_every_boundary_is_safe_and_compiles_without_a_word() {
    let def = protocol("vns_hrv", json!({ "frequencyHz": 20, "intensityMilliamps": 0.8, "hrvProtocol": "standalone" }), json!({ "intervalOnSeconds": 0 }), 600);
    assert_eq!(protocol_zone(&evaluate(&def)), Zone::Safe);
    assert_eq!(validate(&def)["zone"], "safe");
    assert!(compile(&def, &[]).is_ok());
}

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

#[test]
fn a_block_that_drives_no_electrode_or_lacks_a_parameter_has_no_axes() {
    assert!(evaluate(&protocol("eeg_neurofeedback", json!({}), json!({}), 600)).is_empty());
    assert!(evaluate(&protocol("bes_tacs", json!({ "frequencyHz": 10 }), json!({}), 600)).is_empty(), "a missing amplitude is the validator's to report");
}
