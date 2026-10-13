//! The compiler refuses what it cannot encode faithfully. A missing parameter used to saturate to its ceiling
//! (`f64::min(NaN, x)` is `x`), and an out-of-range one used to be clamped; both are now refusals.

use neurone_npps_core::api::compile_json;
use serde_json::{json, Value};

const UUID: &str = "00112233445566778899aabbccddeeff";

fn compile(ty: &str, params: Value) -> Result<Vec<u8>, String> {
    compile_with(ty, params, Value::Null, json!(1))
}

fn compile_with(ty: &str, params: Value, clinician: Value, now: Value) -> Result<Vec<u8>, String> {
    let req = json!({
        "def": { "timingMode": { "type": "duration", "seconds": 600 },
                 "modalities": [{ "type": ty, "params": params, "interval": { "intervalOnSeconds": 0 } }] },
        "clinicianSockets": clinician, "nowUnix": now, "sessionUuidHex": UUID,
    });
    compile_json(&req.to_string())
}

fn refused(r: Result<Vec<u8>, String>, needle: &str) {
    let e = r.expect_err("must be refused");
    assert!(e.contains(needle), "{e:?} does not mention {needle:?}");
}

// @verifies REQ-NPPS-01
#[test]
fn a_missing_intensity_is_refused_not_compiled_to_the_ceiling() {
    refused(compile("bes_tacs", json!({ "frequencyHz": 10, "waveform": "sine" })), "intensityMilliamps is required");
    refused(compile("tdcs", json!({ "electrodePairs": [["F3", "F4"]] })), "intensityMilliamps is required");
    refused(compile("tdcs", json!({ "intensityMilliamps": 1 })), "electrodeAreaCm2 is required");
    refused(compile("pbm_deep_1170nm", json!({ "frequencyHz": 10, "dutyCyclePercent": 50 })), "intensityMWcm2 is required");
    refused(compile("vibrotactile_40hz", json!({})), "intensityG is required");
    refused(compile("vns_hrv", json!({ "frequencyHz": 10 })), "intensityMilliamps is required");
    refused(compile("cervical_vns", json!({ "frequencyHz": 10 })), "intensityMilliamps is required");
    refused(compile("clinical_tacs", json!({ "frequencyHz": 10, "channelCount": 4 })), "intensityMilliamps is required");
    refused(compile("hd_tdcs", json!({})), "intensityMilliamps is required");
    refused(compile("tms", json!({ "frequencyHz": 1, "pulseCount": 10 })), "intensityPercentMT is required");
}

// @verifies REQ-NPPS-06
#[test]
fn an_out_of_range_value_is_refused_not_clamped() {
    refused(compile("bes_tacs", json!({ "frequencyHz": 10, "intensityMilliamps": 3 })), "refused, not reduced to fit");
    refused(compile("tdcs", json!({ "intensityMilliamps": 3, "electrodeAreaCm2": 25 })), "refused, not reduced to fit");
    refused(compile("tdcs", json!({ "intensityMilliamps": 1, "electrodeAreaCm2": 70 })), "electrodeAreaCm2");
    refused(compile("tdcs", json!({ "intensityMilliamps": 1, "electrodeAreaCm2": 0 })), "electrodeAreaCm2");
    refused(compile("pbm_deep_1170nm", json!({ "intensityMWcm2": 1500, "frequencyHz": 10, "dutyCyclePercent": 50 })), "refused, not reduced to fit");
    refused(compile("vibrotactile_40hz", json!({ "intensityG": 2 })), "refused, not reduced to fit");
    refused(compile("clinical_tacs", json!({ "frequencyHz": 10, "intensityMilliamps": 1, "channelCount": 24 })), "channelCount");
    refused(compile("bes_tacs", json!({ "frequencyHz": 10, "intensityMilliamps": -1 })), "refused, not reduced to fit");
}

// @verifies REQ-NPPS-04
#[test]
fn an_in_range_value_still_compiles() {
    let b = compile("bes_tacs", json!({ "frequencyHz": 10, "intensityMilliamps": 1, "waveform": "sine" })).unwrap();
    // params follow the 14-byte command header after the 64-byte descriptor header: [0, freq mHz (LE), amp µA (LE), waveform].
    assert_eq!(&b[64 + 14..64 + 14 + 5], &[0, 0x10, 0x27, 0xE8, 0x03]);
}

// @verifies REQ-NPPS-04
#[test]
fn a_socket_or_clock_that_does_not_fit_32_bits_is_refused_not_wrapped() {
    let pbm = json!({ "zones": "clinician_selected", "wavelength": "660nm", "irradianceMWcm2": 100, "frequencyHz": 10, "dutyCyclePercent": 50 });
    // 2^32 + 1 used to truncate to socket 1.
    refused(compile_with("pbm_transcranial", pbm.clone(), json!([4294967297u64]), json!(1)), "socket id");
    refused(compile_with("pbm_transcranial", pbm.clone(), json!([1]), json!(4294967297u64)), "nowUnix");
    assert!(compile_with("pbm_transcranial", pbm, json!([1]), json!(1)).is_ok());
}

// @verifies REQ-NPPS-01
#[test]
fn the_validator_reports_a_missing_required_parameter() {
    use neurone_npps_core::api::validate_json;
    let check = |ty: &str, params: Value| -> Vec<Value> {
        let req = json!({ "entry": { "kind": "single", "protocol": { "timingMode": { "type": "duration", "seconds": 600 },
            "modalities": [{ "type": ty, "params": params, "interval": {} }] } }, "limits": {} });
        let out: Value = serde_json::from_str(&validate_json(&req.to_string()).unwrap()).unwrap();
        out["issues"].as_array().unwrap().iter().filter(|i| i["message"]["key"] == "VALIDATE_MSG_GENERAL_REQUIRED_PARAMETER").cloned().collect()
    };
    let missing = check("bes_tacs", json!({ "frequencyHz": 10 }));
    assert_eq!(missing.len(), 1);
    assert_eq!(missing[0]["severity"], "error");
    assert_eq!(missing[0]["parameterKey"], "intensityMilliamps");
    assert_eq!(check("pbm_deep_1170nm", json!({ "frequencyHz": 10 })).len(), 1);
    assert_eq!(check("vibrotactile_40hz", json!({})).len(), 1);
    // Every compiler-required parameter is reported, and a complete block reports none.
    assert_eq!(check("clinical_tacs", json!({})).len(), 3);
    assert!(check("bes_tacs", json!({ "frequencyHz": 10, "intensityMilliamps": 1 })).is_empty());
}

// SWR-SS-01: a session is finite and non-zero. The descriptor carries its length at byte 60 (little-endian u32, ms).

fn compile_timed(timing: Value, interval: Value) -> Result<Vec<u8>, String> {
    let req = json!({
        "def": { "timingMode": timing,
                 "modalities": [{ "type": "bes_tacs", "params": { "frequencyHz": 10, "intensityMilliamps": 1, "waveform": "sine" }, "interval": interval }] },
        "nowUnix": 1, "sessionUuidHex": UUID,
    });
    compile_json(&req.to_string())
}

fn length_ms(blob: &[u8]) -> u32 {
    u32::from_le_bytes(blob[60..64].try_into().unwrap())
}

// @verifies SWR-SS-01
#[test]
fn a_zero_or_negative_duration_is_refused_by_the_compiler() {
    for seconds in [0, -60] {
        refused(compile_timed(json!({ "type": "duration", "seconds": seconds }), json!({ "intervalOnSeconds": 0 })), "greater than zero");
    }
}

// @verifies SWR-SS-01
#[test]
fn a_very_long_finite_duration_is_not_refused_for_its_length() {
    // 30 days: far past anything a session needs, and still finite.
    let b = compile_timed(json!({ "type": "duration", "seconds": 2_592_000 }), json!({ "intervalOnSeconds": 0 })).unwrap();
    assert_eq!(length_ms(&b), 2_592_000_000);
}

// @verifies SWR-SS-01
#[test]
fn an_interval_count_session_carries_the_length_computed_from_its_intervals() {
    // 60 s on, 30 s off, three times: the stops fall at 60, 150 and 240 s, so the session ends at 240 s.
    let b = compile_timed(
        json!({ "type": "interval_count", "count": 3 }),
        json!({ "intervalOnSeconds": 60, "intervalOffSeconds": 30, "repeatCount": 3 }),
    )
    .unwrap();
    assert_eq!(length_ms(&b), 240_000);
    // A start offset moves the end with it.
    let b = compile_timed(
        json!({ "type": "interval_count", "count": 1 }),
        json!({ "intervalOnSeconds": 60, "intervalOffSeconds": 0, "startOffsetSeconds": 10 }),
    )
    .unwrap();
    assert_eq!(length_ms(&b), 70_000);
}

// @verifies SWR-SS-01
#[test]
fn an_interval_count_session_with_a_block_that_never_stops_is_refused_as_infinite() {
    let e = compile_timed(json!({ "type": "interval_count", "count": 3 }), json!({ "intervalOnSeconds": 0 })).expect_err("infinite");
    assert!(e.contains("Block 1 (bes_tacs)") && e.contains("cannot be infinite"), "{e:?}");
}

// @verifies REQ-NPPS-22
#[test]
fn interval_count_sets_how_many_times_each_interval_repeats() {
    // 60 s on, 30 s off, no repeat of its own: count 4 gives four on-periods, ending at 3 * 90 + 60 = 330 s.
    let b = compile_timed(json!({ "type": "interval_count", "count": 4 }), json!({ "intervalOnSeconds": 60, "intervalOffSeconds": 30 })).unwrap();
    assert_eq!(length_ms(&b), 330_000);
    assert_eq!(b[7], 8, "four starts and four stops");
    // A repeat the block states itself wins over the protocol's count.
    let b = compile_timed(json!({ "type": "interval_count", "count": 4 }), json!({ "intervalOnSeconds": 60, "intervalOffSeconds": 30, "repeatCount": 2 })).unwrap();
    assert_eq!(length_ms(&b), 150_000);
}

// @verifies REQ-NPPS-22
#[test]
fn an_interval_count_that_is_not_a_whole_number_of_one_or_more_is_refused() {
    for count in [json!(0), json!(-1), json!(2.5)] {
        refused(compile_timed(json!({ "type": "interval_count", "count": count }), json!({ "intervalOnSeconds": 60, "intervalOffSeconds": 30 })), "whole number of 1 or more");
    }
}
