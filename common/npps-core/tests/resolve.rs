//! `resolve_limits` against the web `resolveLimits` it replaced (`npps-resolve-golden.json`, frozen: 300 tier
//! combinations), and the source map the web never produced.

use neurone_npps_core::api::resolve_limits_json;
use serde_json::{json, Value};
use std::fs;
use std::path::PathBuf;

fn golden() -> Value {
    let p = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../app/NeurOneShared/TestData/npps-resolve-golden.json");
    serde_json::from_str(&fs::read_to_string(p).expect("golden file")).expect("golden JSON")
}

fn resolve(g: &Value, h: &Value, i: &Value) -> Value {
    let req = json!({ "global": g, "helmet": h, "individual": i });
    serde_json::from_str(&resolve_limits_json(&req.to_string()).unwrap()).unwrap()
}

#[test]
fn every_tier_combination_resolves_as_the_web_function_did() {
    let g = golden();
    let cases = g["cases"].as_array().unwrap();
    assert!(cases.len() >= 300);
    let mut failures = Vec::new();
    for c in cases {
        let got = resolve(&c["global"], &c["helmet"], &c["individual"]);
        let mut blocks = got["limits"].as_object().unwrap().clone();
        assert_eq!(blocks.remove("level"), Some(json!("global")), "{}", c["name"]);
        if Value::Object(blocks.clone()) != c["expected"] {
            failures.push(format!("{}\n  core: {}\n  web:  {}", c["name"], Value::Object(blocks), c["expected"]));
        }
    }
    assert!(failures.is_empty(), "{} divergence(s):\n{}", failures.len(), failures[..failures.len().min(5)].join("\n"));
}

#[test]
fn each_value_names_the_tier_that_supplied_it() {
    let global = json!({ "besTacs": { "maxIntensityMilliamps": 1.0, "maxFrequencyHz": 40, "minFrequencyHz": 1 } });
    let helmet = json!({ "besTacs": { "maxIntensityMilliamps": 0.8, "maxFrequencyHz": 30 } });
    let individual = json!({ "besTacs": { "maxIntensityMilliamps": 0.5 }, "tms": { "maxPulsesPerSession": 100 } });
    let r = resolve(&global, &helmet, &individual);
    assert_eq!(r["limits"]["besTacs"], json!({ "maxIntensityMilliamps": 0.5, "maxFrequencyHz": 30, "minFrequencyHz": 1 }));
    assert_eq!(
        r["sources"],
        json!({
            "besTacs": { "maxIntensityMilliamps": "individual", "maxFrequencyHz": "helmet", "minFrequencyHz": "global" },
            "tms": { "maxPulsesPerSession": "individual" }
        })
    );
}

#[test]
fn a_limit_of_zero_is_a_limit_and_an_omitted_block_is_none() {
    let r = resolve(&json!({ "tdcs": { "maxIntensityMilliamps": 0 } }), &Value::Null, &Value::Null);
    assert_eq!(r["limits"]["tdcs"], json!({ "maxIntensityMilliamps": 0 }));
    assert!(r["limits"].get("tms").is_none(), "no tier states a tms block");
    let none = resolve(&Value::Null, &Value::Null, &Value::Null);
    assert_eq!(none["limits"], json!({ "level": "global" }));
    // A null field is not stated: the next tier supplies it.
    let n = resolve(&json!({ "tdcs": { "maxIntensityMilliamps": 2 } }), &Value::Null, &json!({ "tdcs": { "maxIntensityMilliamps": null } }));
    assert_eq!(n["sources"]["tdcs"]["maxIntensityMilliamps"], "global");
}
