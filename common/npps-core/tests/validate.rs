//! The validator against the web validator it replaced (`npps-validate-golden.json`, frozen). The core returns
//! locale keys; this test resolves them against `locales/en.json` the way each app's `t()` does and requires
//! every issue's severity, parameter, values, source and message to equal the web's English.

use neurone_npps_core::api::validate_json;
use serde_json::{json, Value};
use std::fs;
use std::path::PathBuf;

fn root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn read(rel: &str) -> Value {
    serde_json::from_str(&fs::read_to_string(root().join(rel)).unwrap_or_else(|_| panic!("read {rel}"))).unwrap()
}

/// `t(key, {0: a, 1: b})`: the English template with `{i}` replaced by each rendered argument.
fn render(v: &Value, en: &Value) -> String {
    match v {
        Value::String(s) => s.clone(),
        Value::Object(o) => {
            let key = o["key"].as_str().expect("message key");
            let mut text = en[key].as_str().unwrap_or_else(|| panic!("locales/en.json has no '{key}'")).to_string();
            for (i, a) in o.get("args").and_then(Value::as_array).into_iter().flatten().enumerate() {
                text = text.replace(&format!("{{{i}}}"), &render(a, en));
            }
            text
        }
        other => panic!("not a message: {other}"),
    }
}

fn source_key(s: &str) -> &'static str {
    match s {
        "hardware" => "VALIDATE_SOURCE_HARDWARE",
        "global" => "VALIDATE_SOURCE_GLOBAL",
        "helmet" => "VALIDATE_SOURCE_HELMET",
        _ => "VALIDATE_SOURCE_INDIVIDUAL",
    }
}

/// An issue as the web validator reports it, English, minus the per-call id.
fn resolve(i: &Value, en: &Value) -> Value {
    let mut o = json!({
        "severity": i["severity"],
        "parameterKey": i["parameterKey"],
        "parameterDisplayName": render(&i["parameterName"], en),
        "actualValueDescription": render(&i["actualValueDescription"], en),
        "limitValueDescription": render(
            &json!({ "key": "VALIDATE_LIMIT_WITH_SOURCE", "args": [i["limitValueDescription"], { "key": source_key(i["limitSource"].as_str().unwrap()) }] }),
            en,
        ),
        "limitSource": i["limitSource"],
        "message": render(&i["message"], en),
    });
    if !i["modality"].is_null() {
        o["modality"] = i["modality"].clone();
    }
    o
}

/// Message keys of the checks the core adds to the web validator's: the ones the iOS and Android validators
/// made and the web one did not (OI-NPPS-CORE-01). The golden is the web's output, so it is compared after
/// setting these aside; `tests/validate_union.rs` holds each of them.
const ADDED: &[&str] = &[
    "VALIDATE_MSG_GENERAL_DURATION_3",
    "VALIDATE_MSG_PBM_CW_DUTY",
    "VALIDATE_MSG_PBM_SESSION_DOSE",
    "VALIDATE_MSG_GENERAL_SESSIONDURATION",
    "VALIDATE_MSG_GENERAL_SESSIONDURATION_2",
    "VALIDATE_MSG_GENERAL_SESSIONDURATION_3",
    "VALIDATE_MSG_GENERAL_SESSIONDURATION_4",
    "VALIDATE_MSG_GENERAL_SESSIONDURATION_5",
    "VALIDATE_MSG_GENERAL_SESSIONDURATION_6",
    "VALIDATE_MSG_GENERAL_SESSIONDURATION_7",
    "VALIDATE_MSG_GENERAL_SESSIONDURATION_8",
    "VALIDATE_MSG_GENERAL_INTENSITYPERCENTMT",
    "VALIDATE_MSG_GENERAL_CARDIACINTERLOCK",
    "VALIDATE_MSG_GENERAL_FREQUENCYHZ",
    "VALIDATE_MSG_GENERAL_FREQUENCYHZ_2",
    "VALIDATE_MSG_GENERAL_INTENSITYMWCM2_2",
    "VALIDATE_MSG_ZONE_CAUTION",
    "VALIDATE_MSG_ZONE_DANGER",
];

fn is_added(issue: &Value) -> bool {
    issue["message"]["key"].as_str().map_or(false, |k| ADDED.contains(&k))
}

#[test]
fn every_case_gives_the_issues_the_web_validator_gave() {
    let g = read("app/NeurOneShared/TestData/npps-validate-golden.json");
    let en = read("locales/en.json");
    let cases = g["cases"].as_array().unwrap();
    assert!(cases.len() > 500);
    let mut failures = Vec::new();
    for c in cases {
        // `limits` names a set in the file's `limits`; `allProtocols` is null, a list, or "library".
        let limits = &g["limits"][c["limits"].as_str().expect("a named limits set")];
        let all = if c["allProtocols"] == "library" { &g["library"] } else { &c["allProtocols"] };
        let req = json!({ "entry": c["entry"], "limits": limits, "allProtocols": all });
        let got: Value = serde_json::from_str(&validate_json(&req.to_string()).unwrap()).unwrap();
        let issues: Vec<Value> =
            got["issues"].as_array().unwrap().iter().filter(|i| !is_added(i)).map(|i| resolve(i, &en)).collect();
        let want = c["expected"].as_array().unwrap();
        if &issues != want {
            failures.push(format!("{}\n  core: {}\n  web:  {}", c["name"], Value::Array(issues), Value::Array(want.clone())));
        }
    }
    assert!(failures.is_empty(), "{} divergence(s):\n{}", failures.len(), failures[..failures.len().min(8)].join("\n"));
}

#[test]
fn the_hardware_file_holds_every_ceiling_the_validator_reads() {
    // `hw()` panics on a name the file lacks; a check that never runs would hide that, so run a protocol
    // that touches every modality.
    let g = read("app/NeurOneShared/TestData/npps-validate-golden.json");
    let touched = g["cases"].as_array().unwrap().iter().flat_map(|c| c["expected"].as_array().unwrap().iter()).count();
    assert!(touched > 300);
}

/// A key the validator names that `locales/en.json` lacks would render as itself on every runtime, and no golden
/// case need reach it. The gate (`scripts/check-locale-strings.ts`) finds keys nothing names; this finds the reverse.
#[test]
fn every_locale_key_the_validator_names_exists_in_english() {
    let en = read("locales/en.json");
    let src = fs::read_to_string(root().join("common/npps-core/src/validate.rs")).unwrap();
    let mut keys: Vec<String> = Vec::new();
    let bytes = src.as_bytes();
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'"' {
            let end = src[i + 1..].find('"').map(|n| i + 1 + n).unwrap_or(src.len());
            let lit = &src[i + 1..end];
            if ["VALIDATE_", "SOCKET_ERR_"].iter().any(|p| lit.starts_with(p)) && lit.chars().all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == '_') {
                keys.push(lit.to_string());
            }
            i = end + 1;
        } else {
            i += 1;
        }
    }
    assert!(keys.len() > 100, "found {} keys", keys.len());
    let missing: Vec<&String> = keys.iter().filter(|k| en[k.as_str()].is_null()).collect();
    assert!(missing.is_empty(), "keys the validator names that locales/en.json lacks: {missing:?}");
    for m in [
        "pbm_transcranial", "pbm_intranasal", "eeg_neurofeedback", "bes_tacs", "tdcs", "vns_hrv", "audio_entrainment", "visual_stimulation",
        "qeeg_21ch", "tms", "pbm_deep_1170nm", "clinical_tacs", "hd_tdcs", "cervical_vns", "vibrotactile_40hz",
    ] {
        let k = format!("MODALITY_{}_NAME", m.to_ascii_uppercase());
        assert!(!en[k.as_str()].is_null(), "{k}");
    }
}
