//! The checks the core adds to the web validator's (OI-NPPS-CORE-01): what the iOS and Android validators
//! checked and the web one did not. A shared validator that only had the web's checks would have weakened
//! the apps that had more, so it has all of them. Each test names the iOS behaviour it holds.

use neurone_npps_core::api::validate_json;
use serde_json::{json, Value};

fn run(def: Value, limits: Value, extra: Value) -> Vec<Value> {
    let mut req = json!({ "entry": { "kind": "single", "protocol": def }, "limits": limits });
    for (k, v) in extra.as_object().cloned().unwrap_or_default() {
        req[k] = v;
    }
    let out: Value = serde_json::from_str(&validate_json(&req.to_string()).unwrap()).unwrap();
    out["issues"].as_array().unwrap().clone()
}

fn block(kind: &str, params: Value, interval: Value) -> Value {
    json!({ "type": kind, "enabled": true, "params": params, "interval": interval })
}

fn proto(seconds: f64, modalities: Vec<Value>) -> Value {
    json!({ "name": "P", "timingMode": { "type": "duration", "seconds": seconds }, "modalities": modalities })
}

fn keys(issues: &[Value]) -> Vec<String> {
    issues.iter().map(|i| i["message"]["key"].as_str().unwrap_or("").to_string()).collect()
}

fn find<'a>(issues: &'a [Value], key: &str) -> &'a Value {
    issues.iter().find(|i| i["message"]["key"] == key).unwrap_or_else(|| panic!("no {key} in {:?}", keys(issues)))
}

const CONT: fn() -> Value = || json!({ "intervalOnSeconds": 0, "intervalOffSeconds": 0 });

#[test]
fn a_zero_or_negative_duration_is_an_error_not_a_short_session_warning() {
    for d in [0.0, -5.0] {
        let i = run(proto(d, vec![block("qeeg_21ch", json!({}), CONT())]), json!({}), json!({}));
        assert_eq!(find(&i, "VALIDATE_MSG_GENERAL_DURATION_3")["severity"], "error");
        assert!(!keys(&i).contains(&"VALIDATE_MSG_GENERAL_DURATION".to_string()), "no short-session warning as well");
    }
    let short = run(proto(30.0, vec![block("qeeg_21ch", json!({}), CONT())]), json!({}), json!({}));
    assert_eq!(find(&short, "VALIDATE_MSG_GENERAL_DURATION")["severity"], "warning");
}

#[test]
fn a_cw_block_with_a_duty_other_than_100_contradicts_itself() {
    let i = run(
        proto(600.0, vec![block("pbm_intranasal", json!({ "wavelength": "660nm", "irradianceMWcm2": 25, "frequencyHz": 0, "dutyCyclePercent": 50 }), CONT())]),
        json!({}),
        json!({}),
    );
    let cw = find(&i, "VALIDATE_MSG_PBM_CW_DUTY");
    assert_eq!(cw["severity"], "error");
    assert_eq!(cw["message"]["args"], json!(["50"]));
    assert_eq!(cw["limitValueDescription"], "100%");
}

#[test]
fn a_pbm_session_dose_over_the_configured_ceiling_is_an_error() {
    // 100 mW/cm² at 25 % duty for 1000 s = 25 J/cm²; the ceiling is 20.
    let i = run(
        proto(1000.0, vec![block("pbm_transcranial", json!({ "wavelength": "808nm", "irradianceMWcm2": 100, "frequencyHz": 40, "dutyCyclePercent": 25, "zones": "clinician_selected" }), CONT())]),
        json!({ "level": "global", "pbmTranscranial": { "maxSessionDoseJCm2": 20 } }),
        json!({}),
    );
    let d = find(&i, "VALIDATE_MSG_PBM_SESSION_DOSE");
    assert_eq!(d["message"]["args"], json!(["25.0", "20.0"]));
    // CW counts the whole irradiance: 100 mW/cm² for 250 s = 25 J/cm².
    let cw = run(
        proto(250.0, vec![block("pbm_transcranial", json!({ "wavelength": "808nm", "irradianceMWcm2": 100, "frequencyHz": 0, "dutyCyclePercent": 100, "zones": "clinician_selected" }), CONT())]),
        json!({ "level": "global", "pbmTranscranial": { "maxSessionDoseJCm2": 20 } }),
        json!({}),
    );
    assert_eq!(find(&cw, "VALIDATE_MSG_PBM_SESSION_DOSE")["message"]["args"], json!(["25.0", "20.0"]));
}

#[test]
fn a_session_duration_limit_is_checked_against_the_on_interval_of_each_modality() {
    let interval = json!({ "intervalOnSeconds": 600, "intervalOffSeconds": 60 });
    let cases = [
        ("pbm_intranasal", json!({ "wavelength": "660nm", "irradianceMWcm2": 5, "frequencyHz": 10, "dutyCyclePercent": 10 }), "pbmIntranasal", "VALIDATE_MSG_GENERAL_SESSIONDURATION_8"),
        ("bes_tacs", json!({ "frequencyHz": 10, "intensityMilliamps": 0.5, "waveform": "square" }), "besTacs", "VALIDATE_MSG_GENERAL_SESSIONDURATION_7"),
        ("tdcs", json!({ "intensityMilliamps": 1, "electrodePairs": [["a", "b"]], "rampSeconds": 30, "electrodeAreaCm2": 25 }), "tdcs", "VALIDATE_MSG_GENERAL_SESSIONDURATION_6"),
        ("vns_hrv", json!({ "frequencyHz": 10, "intensityMilliamps": 1, "hrvProtocol": "standalone", "resonanceBreathingRate": 6 }), "vnsHrv", "VALIDATE_MSG_GENERAL_SESSIONDURATION_5"),
        ("pbm_deep_1170nm", json!({ "intensityMWcm2": 100, "frequencyHz": 40, "dutyCyclePercent": 25 }), "pbmDeep1170nm", "VALIDATE_MSG_GENERAL_SESSIONDURATION_4"),
        ("clinical_tacs", json!({ "frequencyHz": 10, "intensityMilliamps": 1, "channelCount": 4, "waveform": "square" }), "clinicalTacs", "VALIDATE_MSG_GENERAL_SESSIONDURATION_3"),
        ("hd_tdcs", json!({ "target": "dlpfc", "montage": "ring4", "intensityMilliamps": 1 }), "hdTdcs", "VALIDATE_MSG_GENERAL_SESSIONDURATION_2"),
        ("cervical_vns", json!({ "frequencyHz": 10, "intensityMilliamps": 1 }), "cervicalVns", "VALIDATE_MSG_GENERAL_SESSIONDURATION"),
    ];
    for (kind, params, prop, key) in cases {
        let limits = json!({ "level": "helmet", prop: { "maxSessionDurationSeconds": 300 } });
        let i = run(proto(1200.0, vec![block(kind, params.clone(), interval.clone())]), limits.clone(), json!({}));
        let d = find(&i, key);
        assert_eq!(d["message"]["args"], json!(["10m", "5m"]), "{kind}");
        assert_eq!(d["limitSource"], "helmet", "{kind}: attributed to the level the limits resolved at");
        // A continuous block has no interval to exceed, and one inside the limit is fine.
        let cont = run(proto(1200.0, vec![block(kind, params.clone(), CONT())]), limits.clone(), json!({}));
        assert!(!keys(&cont).contains(&key.to_string()), "{kind}: continuous");
        let ok = run(proto(1200.0, vec![block(kind, params, json!({ "intervalOnSeconds": 120, "intervalOffSeconds": 60 }))]), limits, json!({}));
        assert!(!keys(&ok).contains(&key.to_string()), "{kind}: inside");
    }
}

#[test]
fn tms_above_120_percent_mt_warns() {
    let i = run(
        proto(600.0, vec![block("tms", json!({ "tmsProtocol": "rtms_10hz", "frequencyHz": 10, "intensityPercentMT": 130, "target": "dlpfc", "pulseCount": 100 }), CONT())]),
        json!({}),
        json!({}),
    );
    let w = find(&i, "VALIDATE_MSG_GENERAL_INTENSITYPERCENTMT");
    assert_eq!((w["severity"].as_str(), w["message"]["args"].clone()), (Some("warning"), json!(["130"])));
}

#[test]
fn cervical_vns_checks_its_frequency_range_and_always_states_the_interlock() {
    let cv = |f: f64| run(proto(600.0, vec![block("cervical_vns", json!({ "frequencyHz": f, "intensityMilliamps": 1 }), CONT())]), json!({}), json!({}));
    let bad = cv(40.0);
    assert_eq!(find(&bad, "VALIDATE_MSG_GENERAL_FREQUENCYHZ_2")["message"]["args"], json!(["40 Hz", "1", "25"]));
    let fine = cv(10.0);
    assert!(!keys(&fine).contains(&"VALIDATE_MSG_GENERAL_FREQUENCYHZ_2".to_string()));
    for i in [&bad, &fine] {
        let w = find(i, "VALIDATE_MSG_GENERAL_CARDIACINTERLOCK");
        assert_eq!((w["severity"].as_str(), w["parameterKey"].as_str()), (Some("warning"), Some("cardiacInterlock")));
    }
}

#[test]
fn a_vibrotactile_model_off_40_hz_warns_and_a_file_that_cannot_say_so_does_not() {
    let vib = |extra: Value| {
        let mut p = json!({ "intensityG": 0.9, "syncToAudio": false, "syncToVisual": false });
        for (k, v) in extra.as_object().cloned().unwrap_or_default() {
            p[k] = v;
        }
        run(proto(600.0, vec![block("vibrotactile_40hz", p, CONT())]), json!({}), json!({}))
    };
    assert_eq!(find(&vib(json!({ "frequencyHz": 45 })), "VALIDATE_MSG_GENERAL_FREQUENCYHZ")["message"]["args"], json!(["45 Hz"]));
    assert!(!keys(&vib(json!({ "frequencyHz": 40.3 }))).contains(&"VALIDATE_MSG_GENERAL_FREQUENCYHZ".to_string()), "inside ±0.5");
    assert!(!keys(&vib(json!({}))).contains(&"VALIDATE_MSG_GENERAL_FREQUENCYHZ".to_string()), "no frequency stated");
}

#[test]
fn deep_pbm_above_its_own_ceiling_is_a_hardware_error() {
    let i = run(
        proto(600.0, vec![block("pbm_deep_1170nm", json!({ "intensityMWcm2": 1500.7, "frequencyHz": 0, "dutyCyclePercent": 100 }), CONT())]),
        json!({}),
        json!({}),
    );
    let e = find(&i, "VALIDATE_MSG_GENERAL_INTENSITYMWCM2_2");
    assert_eq!((e["limitSource"].as_str(), e["message"]["args"].clone()), (Some("hardware"), json!(["1500", "1000"])));
}

#[test]
fn a_named_zone_is_checked_against_the_namespace_when_one_is_given() {
    let pbm = |refs: Value| proto(600.0, vec![block("pbm_transcranial", json!({ "wavelength": "808nm", "irradianceMWcm2": 100, "frequencyHz": 40, "dutyCyclePercent": 25, "zones": "named", "zoneRefs": refs }), CONT())]);
    let zones = json!({ "zones": { "Frontal": [1, 2], "Empty": [] } });
    let unknown = run(pbm(json!(["Nowhere"])), json!({}), zones.clone());
    assert_eq!(find(&unknown, "SOCKET_ERR_UNKNOWN_ZONE")["message"]["args"], json!(["Nowhere"]));
    let empty = run(pbm(json!(["Empty"])), json!({}), zones.clone());
    assert_eq!(find(&empty, "VALIDATE_MSG_GENERAL_TARGET_2")["message"]["args"], json!(["Empty"]));
    let fine = run(pbm(json!(["Frontal"])), json!({}), zones.clone());
    assert!(!keys(&fine).iter().any(|k| k == "SOCKET_ERR_UNKNOWN_ZONE" || k == "VALIDATE_MSG_GENERAL_TARGET_2"));
    // No namespace, no check: the web has none to give.
    let none = run(pbm(json!(["Nowhere"])), json!({}), json!({}));
    assert!(!keys(&none).contains(&"SOCKET_ERR_UNKNOWN_ZONE".to_string()));
    // clinician_selected is unresolvable by design.
    let sel = run(proto(600.0, vec![block("pbm_transcranial", json!({ "wavelength": "808nm", "irradianceMWcm2": 100, "frequencyHz": 40, "dutyCyclePercent": 25, "zones": "clinician_selected" }), CONT())]), json!({}), zones);
    assert!(!keys(&sel).contains(&"VALIDATE_MSG_GENERAL_TARGET_2".to_string()));
}

#[test]
fn each_limit_is_attributed_to_its_own_tier_when_a_source_map_says_so() {
    let p = proto(600.0, vec![block("bes_tacs", json!({ "frequencyHz": 30, "intensityMilliamps": 0.9, "waveform": "square" }), CONT())]);
    let limits = json!({ "level": "individual", "besTacs": { "maxFrequencyHz": 20, "maxIntensityMilliamps": 0.5 } });
    let map = json!({ "limitSources": { "besTacs": { "maxFrequencyHz": "global" } } });
    let i = run(p, limits, map);
    assert_eq!(find(&i, "VALIDATE_MSG_BES_TACS_FREQUENCYHZ_2")["limitSource"], "global", "from the map");
    assert_eq!(find(&i, "VALIDATE_MSG_BES_TACS_INTENSITYMILLIAMPS_2")["limitSource"], "individual", "from the level");
}
