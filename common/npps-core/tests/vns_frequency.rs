//! The auricular VNS frequency: no ceiling, a floor, and a field wide enough to carry what is authored.
//!
//! The 25 Hz ceiling was an equivalence bound carried over from the cervical predicate, with no derivation, and it is
//! removed (`OI-VNSCLIP-08`, the principal, 2026-10-29). The cervical device keeps its 25 Hz, which is its gammaCore
//! equivalence claim (`NP-REG-CVNS-001` §3).

use neurone_npps_core::api::{compile_json, parse_json, validate_json};
use serde_json::{json, Value};

const UUID: &str = "00112233445566778899aabbccddeeff";

fn protocol(kind: &str, block: &str) -> Value {
    let text = format!("protocol \"VNS\" {{\n    duration: 10m\n    {kind} {{\n{block}\n    }}\n}}\n");
    let out: Value = serde_json::from_str(&parse_json(&text).expect("parses")).unwrap();
    out["entries"][0]["protocol"].clone()
}

fn compile(p: &Value) -> Result<Vec<u8>, String> {
    compile_json(&json!({ "def": p, "nowUnix": 1, "sessionUuidHex": UUID }).to_string())
}

fn keys(p: &Value) -> Vec<String> {
    let req = json!({ "entry": { "kind": "single", "protocol": p }, "limits": {} });
    let out: Value = serde_json::from_str(&validate_json(&req.to_string()).unwrap()).unwrap();
    out["issues"].as_array().unwrap().iter()
        .filter(|i| i["severity"] == "error")
        .map(|i| i["message"]["key"].as_str().unwrap_or("").to_string())
        .collect()
}

fn auricular(freq: &str) -> Value {
    protocol("vns_hrv", &format!("        frequency: {freq}\n        intensity: 0.8mA"))
}

// @verifies REQ-VNSC-03
#[test]
fn an_auricular_frequency_above_the_old_ceiling_validates_and_compiles() {
    for f in ["26Hz", "30Hz", "100Hz"] {
        let p = auricular(f);
        assert!(!keys(&p).iter().any(|k| k == "VALIDATE_MSG_VNS_HRV_FREQUENCYHZ"), "{f} must not be refused");
        assert!(compile(&p).is_ok(), "{f} must compile");
    }
}

// @verifies REQ-VNSC-03
#[test]
fn the_frequency_travels_in_a_32_bit_field() {
    // 68-byte header + 14-byte command header, then side (1 byte) and the frequency in mHz (LE32).
    let b = compile(&auricular("100Hz")).unwrap();
    assert_eq!(b[83..87], 100_000u32.to_le_bytes(), "100 Hz = 100,000 mHz");
    // 70 Hz is 70,000 mHz, which a 16-bit field (65,535 max) could not hold: the case that made the widening necessary.
    let b = compile(&auricular("70Hz")).unwrap();
    assert_eq!(b[83..87], 70_000u32.to_le_bytes());
}

// @verifies REQ-VNSC-03
#[test]
fn any_positive_frequency_is_accepted_and_zero_is_refused() {
    for f in ["0.001Hz", "0.1Hz", "0.5Hz", "1Hz"] {
        assert!(!keys(&auricular(f)).iter().any(|k| k == "VALIDATE_MSG_VNS_HRV_FREQUENCYHZ"), "{f} must not be refused");
    }
    assert!(keys(&auricular("0Hz")).iter().any(|k| k == "VALIDATE_MSG_VNS_HRV_FREQUENCYHZ"));
    // Sub-hertz travels in the 32-bit millihertz field: 0.5 Hz = 500 mHz.
    assert_eq!(compile(&auricular("0.5Hz")).unwrap()[83..87], 500u32.to_le_bytes());
}

// @verifies REQ-VNSC-03
#[test]
fn a_pulse_that_cannot_fit_its_own_period_is_refused() {
    // A low current keeps the zone model's mean-current-density boundary out of the way, so what is refused is the geometry.
    let at = |f: &str, extra: &str| {
        protocol("vns_hrv", &format!("        frequency: {f}\n        intensity: 0.01mA{extra}"))
    };
    // Two 250 µs phases (the firmware default) need 500 µs, so 2,000 Hz is the highest frequency that holds them.
    assert!(compile(&at("1000Hz", "")).is_ok());
    assert!(compile(&at("2000Hz", "")).is_ok());
    let e = compile(&at("3000Hz", "")).expect_err("refused");
    assert!(e.contains("does not fit"), "{e}");
    // The authored width is used, not the default: 500 µs phases fit at 1,000 Hz and not at 1,001.
    assert!(compile(&at("1000Hz", "\n        pulse_width_us: 500")).is_ok());
    let e = compile(&at("1001Hz", "\n        pulse_width_us: 500")).expect_err("refused");
    assert!(e.contains("does not fit"), "{e}");
}

// @verifies REQ-VNSC-03
#[test]
fn the_zone_model_still_bounds_a_high_frequency_at_a_high_current() {
    // Frequency enters through the mean current density (REQ-ZONE, placeholder boundaries): removing the frequency
    // ceiling does not make 0.8 mA at 500 Hz compile. Those boundaries are themselves unjustified (UC-075, UC-076).
    let e = compile(&auricular("500Hz")).expect_err("refused");
    assert!(e.contains("danger zone"), "{e}");
}

// @verifies REQ-CVNS-04
#[test]
fn the_cervical_device_keeps_its_25_hz_range() {
    let cervical = |f: &str| protocol("cervical_vns", &format!("        frequency: {f}\n        intensity: 1mA"));
    let refused = |f: &str| keys(&cervical(f)).iter().any(|k| k.starts_with("VALIDATE_MSG_GENERAL_FREQUENCYHZ"));
    assert!(refused("30Hz"), "cervical 30 Hz stays refused");
    assert!(!refused("25Hz"), "cervical 25 Hz stays allowed");
}
