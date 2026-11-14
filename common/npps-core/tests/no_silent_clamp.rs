//! The compiler refuses a value it used to clamp, wrap or cut (NP-NPPS-REF-001 REQ-NPPS-33; CLAUDE.md §3: a ceiling
//! refuses a protocol and never reshapes one). Each site is tested three ways: exactly at the limit compiles to the
//! bytes authored, one step over is refused, and the refusal names the field, the value and the range.
//!
//! The last tests read the hub's own limit constants out of its headers and check that a value at each limit compiles
//! to a register the hub's load-time check (`np_cmd_limits_admit`, REQ-FWHUB-78 (g)) accepts, and that one step over is
//! refused here, so nothing the core signs is refused by the hub for a limit.

use neurone_npps_core::api::compile_json;
use serde_json::{json, Value};

const UUID: &str = "00112233445566778899aabbccddeeff";
const HEADER: usize = 107;
const CMD_HDR: usize = 14;

fn request(ty: &str, params: Value, interval: Value, seconds: f64) -> Value {
    json!({
        "def": { "timingMode": { "type": "duration", "seconds": seconds },
                 "modalities": [{ "type": ty, "params": params, "interval": interval }] },
        "clinicianSockets": [1], "nowUnix": 1, "sessionUuidHex": UUID,
    })
}

fn compile(ty: &str, params: Value) -> Result<Vec<u8>, String> {
    compile_json(&request(ty, params, json!({ "intervalOnSeconds": 0 }), 600.0).to_string())
}

/// The parameter block of the first command: after the header, the command header and the target block.
fn params(blob: &[u8]) -> &[u8] {
    let len = u16::from_le_bytes([blob[HEADER + 10], blob[HEADER + 11]]) as usize;
    let target = blob[HEADER + 13] as usize;
    &blob[HEADER + CMD_HDR + target..HEADER + CMD_HDR + target + len]
}

fn ok(ty: &str, p: Value) -> Vec<u8> {
    let blob = compile(ty, p.clone()).unwrap_or_else(|e| panic!("{ty} {p} must compile: {e}"));
    params(&blob).to_vec()
}

fn u16_at(p: &[u8], at: usize) -> u16 {
    u16::from_le_bytes([p[at], p[at + 1]])
}

/// Refused, and the message carries every needle (the field, the value, the range).
fn refused(r: Result<Vec<u8>, String>, needles: &[&str]) {
    let e = r.expect_err("must be refused");
    for n in needles {
        assert!(e.contains(n), "{e:?} does not mention {n:?}");
    }
}

fn pbm(hz: f64, duty: f64, irr: f64) -> Value {
    json!({ "zones": "clinician_selected", "wavelength": "808nm", "irradianceMWcm2": irr, "frequencyHz": hz, "dutyCyclePercent": duty })
}
fn nasal(hz: f64, duty: f64, irr: f64) -> Value {
    json!({ "wavelength": "660nm", "irradianceMWcm2": irr, "frequencyHz": hz, "dutyCyclePercent": duty })
}
fn deep(hz: f64, duty: f64) -> Value {
    json!({ "intensityMWcm2": 500, "frequencyHz": hz, "dutyCyclePercent": duty })
}
fn tdcs(ma: f64, ramp: Value) -> Value {
    json!({ "intensityMilliamps": ma, "electrodePairs": [["F3", "F4"]], "rampSeconds": ramp, "electrodeAreaCm2": 25 })
}
fn tms(protocol: &str, hz: f64, pulses: f64) -> Value {
    json!({ "tmsProtocol": protocol, "frequencyHz": hz, "intensityPercentMT": 80, "target": "M1_R", "pulseCount": pulses })
}
fn audio(extra: Value) -> Value {
    let mut p = json!({ "carrierHz": 440, "volumeDb": 60, "eegAdaptive": false, "boneConductionPacer": false });
    for (k, v) in extra.as_object().unwrap() {
        p[k] = v.clone();
    }
    p
}
fn visual(hz: Value, cadence: Value) -> Value {
    json!({ "frequencyHz": hz, "mode": "binocular", "emdrCadenceHz": cadence, "enableModeF": false })
}

// ── (a) a value over a limit is refused, not encoded as the limit ───────────────────────────────────────────────────

// @verifies REQ-NPPS-33#a
#[test]
fn a_pbm_duty_above_25_percent_is_refused_on_every_pbm_modality_and_25_compiles() {
    // Byte 1 of a transcranial block, byte 2 of an intranasal one and byte 3 of a 1170 nm one is the duty register.
    assert_eq!(ok("pbm_transcranial", pbm(10.0, 25.0, 100.0))[1], 0x32);
    assert_eq!(ok("pbm_intranasal", nasal(10.0, 25.0, 30.0))[2], 0x32);
    assert_eq!(ok("pbm_deep_1170nm", deep(10.0, 25.0))[3], 0x32);
    assert_eq!(ok("pbm_transcranial", pbm(10.0, 12.5, 100.0))[1], 25);
    refused(compile("pbm_transcranial", pbm(10.0, 25.5, 100.0)), &["PBM", "duty cycle 25.5%", "maximum of 25%", "not reduced to fit"]);
    refused(compile("pbm_intranasal", nasal(10.0, 26.0, 30.0)), &["Intranasal PBM", "duty cycle 26%", "maximum of 25%"]);
    refused(compile("pbm_deep_1170nm", deep(10.0, 50.0)), &["1170 nm PBM", "duty cycle 50%", "maximum of 25%"]);
}

// @verifies REQ-NPPS-33#a
#[test]
fn continuous_wave_is_refused_not_encoded_as_25_percent_duty() {
    // 100 % duty used to be written as register 0x32, which is 25 %.
    refused(compile("pbm_transcranial", pbm(0.0, 100.0, 100.0)), &["duty cycle 100%", "maximum of 25%", "Continuous wave", "OI-HEXTILE-31"]);
    refused(compile("pbm_intranasal", nasal(0.0, 100.0, 30.0)), &["duty cycle 100%", "Continuous wave"]);
    refused(compile("pbm_deep_1170nm", deep(0.0, 100.0)), &["duty cycle 100%", "Continuous wave"]);
}

// @verifies REQ-NPPS-33#a
#[test]
fn a_tdcs_ramp_under_30_seconds_or_missing_is_refused_not_lengthened() {
    let p = ok("tdcs", tdcs(1.0, json!(30)));
    assert_eq!(u16_at(&p, 4), 30);
    assert_eq!(u16_at(&ok("tdcs", tdcs(1.0, json!(45))), 4), 45);
    assert_eq!(u16_at(&ok("tdcs", tdcs(1.0, json!(65535))), 4), 65535);
    refused(compile("tdcs", tdcs(1.0, json!(29))), &["tDCS", "rampSeconds 29 s", "minimum of 30 s", "not lengthened"]);
    refused(compile("tdcs", tdcs(1.0, json!(29.9))), &["rampSeconds 29.9 s"]);
    refused(compile("tdcs", tdcs(1.0, json!(0))), &["rampSeconds 0 s"]);
    refused(compile("tdcs", tdcs(1.0, Value::Null)), &["tDCS", "rampSeconds is required"]);
    refused(compile("tdcs", tdcs(1.0, json!(65536))), &["rampSeconds 65536 s", "0–65535 s"]);
}

// @verifies REQ-NPPS-33#a
#[test]
fn a_tms_frequency_pulse_count_or_interval_the_field_cannot_carry_is_refused_not_capped() {
    let p = ok("tms", tms("rTMS", 65.535, 65535.0));
    assert_eq!(u16_at(&p, 2), 65535);
    assert_eq!(u16_at(&p, 5), 65535);
    assert_eq!(u16_at(&p, 7), 15); // 1000 / 65.535 Hz
    refused(compile("tms", tms("rTMS", 65.536, 600.0)), &["TMS", "frequencyHz 65.536 Hz", "0–65.535 Hz"]);
    refused(compile("tms", tms("iTBS", 66.0, 600.0)), &["frequencyHz 66 Hz", "0–65.535 Hz"]);
    refused(compile("tms", tms("rTMS", 10.0, 65536.0)), &["TMS", "pulseCount 65536", "0–65535"]);
    refused(compile("tms", tms("rTMS", 10.0, 10.5)), &["pulseCount 10.5"]);
    refused(compile("tms", tms("rTMS", 10.0, -1.0)), &["pulseCount -1"]);
    // rTMS: the interval is one period. 0.016 Hz is 62500 ms and fits; 0.015 Hz is 66667 ms and does not; it used to be
    // computed at 0.1 Hz whatever was authored below that.
    assert_eq!(u16_at(&ok("tms", tms("rTMS", 0.016, 600.0)), 7), 62500);
    assert_eq!(u16_at(&ok("tms", tms("rTMS", 0.05, 600.0)), 7), 20000);
    refused(compile("tms", tms("rTMS", 0.015, 600.0)), &["TMS", "frequencyHz 0.015 Hz", "inter-train interval", "65535 ms"]);
    refused(compile("tms", tms("rTMS", 0.0, 600.0)), &["frequencyHz 0 Hz", "inter-train interval"]);
    // A missing frequency or pulse count used to be written as 65535.
    refused(compile("tms", json!({ "tmsProtocol": "rTMS", "intensityPercentMT": 80, "pulseCount": 600 })), &["TMS", "frequencyHz is required"]);
    refused(compile("tms", json!({ "tmsProtocol": "rTMS", "frequencyHz": 10, "intensityPercentMT": 80 })), &["TMS", "pulseCount is required"]);
}

// @verifies REQ-NPPS-33#a
#[test]
fn an_audio_beat_or_carrier_the_field_cannot_carry_is_refused_not_capped() {
    let p = ok("audio_entrainment", audio(json!({ "binauralBeatsHz": 65.535, "carrierHz": 440 })));
    assert_eq!(u16_at(&p, 1), 440);
    assert_eq!(u16_at(&p, 3), 65535);
    assert_eq!(u16_at(&ok("audio_entrainment", audio(json!({ "isochronicTonesHz": 65.535 }))), 3), 65535);
    refused(compile("audio_entrainment", audio(json!({ "binauralBeatsHz": 65.536 }))), &["Audio", "binauralBeatsHz 65.536 Hz", "0–65.535 Hz"]);
    refused(compile("audio_entrainment", audio(json!({ "isochronicTonesHz": 100 }))), &["Audio", "isochronicTonesHz 100 Hz", "0–65.535 Hz"]);
    refused(compile("audio_entrainment", audio(json!({ "noiseType": "pink", "carrierHz": 65536 }))), &["Audio", "carrierHz 65536 Hz", "0–65535 Hz"]);
    refused(compile("audio_entrainment", audio(json!({ "binauralBeatsHz": -1 }))), &["binauralBeatsHz -1 Hz"]);
    // A missing carrier used to be written as 65535 Hz.
    refused(compile("audio_entrainment", json!({ "noiseType": "pink", "volumeDb": 60 })), &["Audio", "carrierHz is required"]);
}

// @verifies REQ-NPPS-33#a
#[test]
fn a_visual_frequency_or_emdr_cadence_over_its_limit_is_refused_not_capped() {
    let p = ok("visual_stimulation", visual(json!(100), json!(25.5)));
    assert_eq!(p[1], 100);
    assert_eq!(p[6], 255);
    assert_eq!(ok("visual_stimulation", visual(json!(40), json!(1.5)))[1..7], [40, 50, 0x02, 0xFF, 0x0F, 15]);
    refused(compile("visual_stimulation", visual(json!(101), json!(1))), &["Visual", "frequencyHz 101 Hz", "0–100 Hz"]);
    refused(compile("visual_stimulation", visual(json!(40), json!(25.6))), &["Visual", "emdrCadenceHz 25.6 Hz", "0–25.5 Hz"]);
    refused(compile("visual_stimulation", visual(json!(-1), json!(1))), &["frequencyHz -1 Hz"]);
    // A missing frequency used to be written as 100 Hz and a missing cadence as 25.5 Hz.
    refused(compile("visual_stimulation", visual(Value::Null, json!(1))), &["Visual", "frequencyHz is required"]);
    refused(compile("visual_stimulation", visual(json!(40), Value::Null)), &["Visual", "emdrCadenceHz is required"]);
}

// ── (b) a value is never wrapped modulo the field's width ───────────────────────────────────────────────────────────

// @verifies REQ-NPPS-33#b
#[test]
fn a_pbm_frequency_the_code_byte_cannot_carry_is_refused_not_wrapped() {
    assert_eq!(ok("pbm_transcranial", pbm(255.0, 25.0, 100.0))[0], 255);
    assert_eq!(ok("pbm_transcranial", pbm(1.0, 25.0, 100.0))[0], 1);
    // 300 Hz used to be written as code 44 (300 mod 256), and 256 Hz as 0, which is continuous wave.
    refused(compile("pbm_transcranial", pbm(300.0, 25.0, 100.0)), &["PBM", "frequencyHz 300 Hz", "1–255 Hz", "wrapped"]);
    refused(compile("pbm_transcranial", pbm(256.0, 25.0, 100.0)), &["frequencyHz 256 Hz"]);
    refused(compile("pbm_intranasal", nasal(300.0, 25.0, 30.0)), &["Intranasal PBM", "frequencyHz 300 Hz"]);
    refused(compile("pbm_deep_1170nm", deep(300.0, 25.0)), &["1170 nm PBM", "frequencyHz 300 Hz"]);
    // A negative frequency used to become continuous wave, and a negative duty wrapped to a large register.
    refused(compile("pbm_transcranial", pbm(-10.0, 25.0, 100.0)), &["frequencyHz -10 Hz"]);
    refused(compile("pbm_transcranial", pbm(10.0, -1.0, 100.0)), &["duty cycle -1%"]);
    // A missing frequency used to become continuous wave.
    let mut p = pbm(10.0, 25.0, 100.0);
    p.as_object_mut().unwrap().remove("frequencyHz");
    refused(compile("pbm_transcranial", p), &["PBM", "frequencyHz is required"]);
}

// @verifies REQ-NPPS-33#b
#[test]
fn a_time_the_millisecond_field_cannot_carry_is_refused_not_wrapped() {
    let bes = json!({ "frequencyHz": 10, "intensityMilliamps": 1, "waveform": "sine" });
    let run = |interval: Value, seconds: f64| compile_json(&request("bes_tacs", bes.clone(), interval, seconds).to_string());
    // The largest session the field carries is 2^32 - 1 ms; one millisecond more used to wrap to 0.
    let max_s = 4_294_967_295.0 / 1000.0;
    let blob = run(json!({ "intervalOnSeconds": 0 }), max_s).unwrap();
    assert_eq!(u32::from_le_bytes(blob[60..64].try_into().unwrap()), u32::MAX);
    refused(run(json!({ "intervalOnSeconds": 0 }), 4_294_967_296.0 / 1000.0), &["session length", "4294967296 ms", "0–4294967295 ms", "wrapped"]);
    // A negative start used to wrap to a time weeks into the session, and a negative on-time to a huge duration.
    refused(run(json!({ "intervalOnSeconds": 0, "startOffsetSeconds": -5 }), 600.0), &["Block 1", "start", "-5000 ms"]);
    refused(run(json!({ "intervalOnSeconds": -5, "intervalOffSeconds": 20, "repeatCount": 1 }), 600.0), &["Block 1", "-5000 ms"]);
    // An on-time under 1 ms used to be written as duration 0, which means "until stopped".
    refused(run(json!({ "intervalOnSeconds": 0.0004, "intervalOffSeconds": 10, "repeatCount": 1 }), 600.0), &["Block 1", "duration", "1 ms resolution"]);
    let blob = run(json!({ "intervalOnSeconds": 0.001, "intervalOffSeconds": 10, "repeatCount": 1 }), 600.0).unwrap();
    assert_eq!(u32::from_le_bytes(blob[HEADER + 6..HEADER + 10].try_into().unwrap()), 1);
}

// ── (c) a value above zero is never raised to the first step or written as zero ─────────────────────────────────────

// @verifies REQ-NPPS-33#c
#[test]
fn a_value_under_half_a_register_step_is_refused_not_raised_to_the_first_step() {
    // 808 nm full scale is 403 mW/cm², so a step is 1.58 mW/cm² and register 1 starts at half of it, 0.79.
    assert_eq!(ok("pbm_transcranial", pbm(10.0, 25.0, 0.8))[3], 1);
    refused(compile("pbm_transcranial", pbm(10.0, 25.0, 0.78)), &["PBM irradiance 0.78 mW/cm²", "smallest step", "1.58 mW/cm²", "not raised"]);
    // The intranasal probe's full scale is 100 mW/cm²: a step is 0.39.
    assert_eq!(ok("pbm_intranasal", nasal(10.0, 25.0, 0.2))[3], 1);
    refused(compile("pbm_intranasal", nasal(10.0, 25.0, 0.19)), &["PBM irradiance 0.19 mW/cm²", "0.39 mW/cm²"]);
    // The audio level byte is 0.5 dB per step and 0 is "off".
    assert_eq!(ok("audio_entrainment", audio(json!({ "noiseType": "pink", "volumeDb": 0.25 })))[5], 1);
    refused(compile("audio_entrainment", audio(json!({ "noiseType": "pink", "volumeDb": 0.2 }))), &["Audio level 0.2 dB SPL", "smallest step", "0.5 dB"]);
    // The duty register is 0.5 % per step.
    assert_eq!(ok("pbm_transcranial", pbm(10.0, 0.25, 100.0))[1], 1);
    refused(compile("pbm_transcranial", pbm(10.0, 0.2, 100.0)), &["PBM", "dutyCyclePercent 0.2 %", "smallest step", "0.5 %"]);
}

// @verifies REQ-NPPS-33#c
#[test]
fn a_value_above_zero_that_would_be_written_as_zero_is_refused() {
    // Frequency code 0 is continuous wave: 0.4 Hz used to compile to it.
    assert_eq!(ok("pbm_transcranial", pbm(0.5, 25.0, 100.0))[0], 1);
    refused(compile("pbm_transcranial", pbm(0.4, 25.0, 100.0)), &["PBM", "frequencyHz 0.4 Hz", "continuous wave", "1 Hz"]);
    // A current under half a microamp used to compile to 0 µA.
    refused(compile("bes_tacs", json!({ "frequencyHz": 10, "intensityMilliamps": 0.0004 })), &["BES/tACS", "intensityMilliamps 0.0004 mA", "smallest step", "0.001 mA"]);
    refused(compile("visual_stimulation", visual(json!(0.4), json!(1))), &["Visual", "frequencyHz 0.4 Hz", "smallest step"]);
    refused(compile("visual_stimulation", visual(json!(40), json!(0.04))), &["Visual", "emdrCadenceHz 0.04 Hz", "0.1 Hz"]);
    // An electrode area under 0.001 cm² used to be declared as 0, "not declared".
    let mut p = tdcs(1.0, json!(30));
    p["electrodeAreaCm2"] = json!(0.0005);
    refused(compile("tdcs", p), &["tDCS", "electrodeAreaCm2 0.0005", "0.001 to 65.535 cm²"]);
    // Zero itself is what was authored and still travels as zero.
    assert_eq!(ok("visual_stimulation", visual(json!(0), json!(0)))[1], 0);
}

// ── (d) the device serial ───────────────────────────────────────────────────────────────────────────────────────────

// @verifies REQ-NPPS-33#d
#[test]
fn a_device_serial_longer_than_its_field_is_refused_not_cut() {
    let with = |hex: String| {
        let mut req = request("bes_tacs", json!({ "frequencyHz": 10, "intensityMilliamps": 1 }), json!({ "intervalOnSeconds": 0 }), 600.0);
        req["deviceSerialHex"] = json!(hex);
        compile_json(&req.to_string())
    };
    let blob = with("5a".repeat(32)).unwrap();
    assert_eq!(&blob[28..60], &[0x5a; 32]);
    // A shorter serial is zero-filled: the field's padding, not a change to the serial.
    let blob = with("5a".repeat(4)).unwrap();
    assert_eq!(&blob[28..32], &[0x5a; 4]);
    assert_eq!(&blob[32..60], &[0u8; 28]);
    refused(with("5a".repeat(33)), &["device serial", "33 bytes", "at most 32", "cut"]);
}

// ── (e) the compiler refuses what the hub's load-time check refuses ─────────────────────────────────────────────────

/// `#define NAME value` from a firmware header, as the number the hub compiles with.
fn hub_const(header: &str, name: &str) -> u32 {
    let path = format!("{}/../../firmware/{header}", env!("CARGO_MANIFEST_DIR"));
    let text = std::fs::read_to_string(&path).unwrap_or_else(|e| panic!("{path}: {e}"));
    for line in text.lines() {
        let mut words = line.split_whitespace();
        if words.next() == Some("#define") && words.next() == Some(name) {
            let v = words.next().unwrap().trim_end_matches(['U', 'u']);
            return match v.strip_prefix("0x") {
                Some(hex) => u32::from_str_radix(hex, 16).unwrap(),
                None => v.parse().unwrap(),
            };
        }
    }
    panic!("{name} is not defined in {path}");
}

const LIMITS_H: &str = "hub_control/include/np_cmd_limits.h";

// @verifies REQ-NPPS-33#e
#[test]
fn a_value_at_each_hub_limit_compiles_to_a_register_the_hub_admits_and_one_step_over_is_refused() {
    // BES / tACS amplitude: np_cmd_limits_check refuses amplitude_ua > STIM_MAX_AMP_BES_UA.
    let bes_max = hub_const(LIMITS_H, "STIM_MAX_AMP_BES_UA");
    let bes = |ua: u32| json!({ "frequencyHz": 10, "intensityMilliamps": f64::from(ua) / 1000.0, "waveform": "sine" });
    assert_eq!(u32::from(u16_at(&ok("bes_tacs", bes(bes_max)), 3)), bes_max);
    refused(compile("bes_tacs", bes(bes_max + 1)), &["BES/tACS", "intensityMilliamps"]);

    // tDCS current and ramp: refuses current_ua > STIM_MAX_AMP_TDCS_UA or ramp_s < STIM_MIN_RAMP_S.
    let (tdcs_max, ramp_min) = (hub_const(LIMITS_H, "STIM_MAX_AMP_TDCS_UA"), hub_const(LIMITS_H, "STIM_MIN_RAMP_S"));
    let p = ok("tdcs", tdcs(f64::from(tdcs_max) / 1000.0, json!(ramp_min)));
    assert_eq!(u32::from(u16_at(&p, 1)), tdcs_max);
    assert_eq!(u32::from(u16_at(&p, 4)), ramp_min);
    refused(compile("tdcs", tdcs(f64::from(tdcs_max + 1) / 1000.0, json!(ramp_min))), &["tDCS", "intensityMilliamps"]);
    refused(compile("tdcs", tdcs(1.0, json!(ramp_min - 1))), &["tDCS", "rampSeconds"]);

    // Auricular VNS amplitude: refuses amplitude_ua > NP_VNS_MAX_CURRENT_UA.
    let vns_max = hub_const("common/include/np_shared_constants.h", "NP_VNS_MAX_CURRENT_UA");
    let vns = |ua: u32| json!({ "frequencyHz": 25, "intensityMilliamps": f64::from(ua) / 1000.0, "hrvProtocol": "standalone" });
    // The core is stricter here: the zone gate (docs/reference/safety-zones.md) refuses the hub's 40 mA for its current
    // density before the encoder's own limit is reached, so the hub's limit is never signed. Below the gate a current
    // travels as authored, and above the hub's limit the encoder refuses it by name.
    assert_eq!(u16_at(&ok("vns_hrv", vns(1500)), 5), 1500);
    refused(compile("vns_hrv", vns(vns_max)), &["danger zone"]);
    refused(compile("vns_hrv", vns(vns_max + 1)), &["VNS", "intensityMilliamps"]);

    // PBM and intranasal duty: refuses duty > NP_PBM_DUTY_MAX_REG / INS_DUTY_MAX. A register step is 0.5 %.
    let pbm_max = hub_const("pbm/include/np_pbm_config.h", "NP_PBM_DUTY_MAX_REG");
    let ins_max = hub_const(LIMITS_H, "INS_DUTY_MAX");
    let pct = |reg: u32| f64::from(reg) / 2.0;
    assert_eq!(u32::from(ok("pbm_transcranial", pbm(10.0, pct(pbm_max), 100.0))[1]), pbm_max);
    refused(compile("pbm_transcranial", pbm(10.0, pct(pbm_max + 1), 100.0)), &["PBM", "duty cycle"]);
    // The 1064 nm channel compiles to the smart-module block, which the hub holds to the same register.
    let mut smart = pbm(10.0, pct(pbm_max), 20.0);
    smart["wavelength"] = json!("1064nm");
    assert_eq!(u32::from(ok("pbm_transcranial", smart.clone())[1]), pbm_max);
    smart["dutyCyclePercent"] = json!(pct(pbm_max + 1));
    refused(compile("pbm_transcranial", smart), &["PBM", "duty cycle"]);
    assert_eq!(u32::from(ok("pbm_intranasal", nasal(10.0, pct(ins_max), 30.0))[2]), ins_max);
    refused(compile("pbm_intranasal", nasal(10.0, pct(ins_max + 1), 30.0)), &["Intranasal PBM", "duty cycle"]);
}

// @verifies REQ-NPPS-33#e
#[test]
fn the_cores_limits_are_the_hubs_limits() {
    use neurone_npps_core::constants::hardware_limits as hw;
    // The limit the compiler refuses at is the one the hub refuses at, so neither can drift without this failing.
    assert_eq!(hw::PBM_DUTY_CYCLE_MAX_PERCENT * 2.0, f64::from(hub_const("pbm/include/np_pbm_config.h", "NP_PBM_DUTY_MAX_REG")));
    assert_eq!(hw::PBM_DUTY_CYCLE_MAX_PERCENT * 2.0, f64::from(hub_const(LIMITS_H, "INS_DUTY_MAX")));
    assert_eq!(hw::TDCS_RAMP_SECONDS, f64::from(hub_const(LIMITS_H, "STIM_MIN_RAMP_S")));
    assert_eq!(hw::TDCS_MAX_MILLIAMPS * 1000.0, f64::from(hub_const(LIMITS_H, "STIM_MAX_AMP_TDCS_UA")));
    assert_eq!(hw::VNS_MAX_MILLIAMPS * 1000.0, f64::from(hub_const("common/include/np_shared_constants.h", "NP_VNS_MAX_CURRENT_UA")));
    assert_eq!(170, hub_const("hub_control/include/np_hub_config.h", "NP_AUDIO_MAX_LEVEL_HALF_DB"));
    assert_eq!(1000, hub_const("hub_control/include/np_hub_config.h", "NP_AUDIO_MAX_TONE_HZ"));
    // The audio level at the ceiling compiles to the hub's ceiling byte, and above it is refused.
    assert_eq!(ok("audio_entrainment", audio(json!({ "noiseType": "pink", "volumeDb": 85 })))[5], 170);
    refused(compile("audio_entrainment", audio(json!({ "noiseType": "pink", "volumeDb": 85.5 }))), &["Audio level 85.5 dB SPL"]);
}

// @verifies REQ-AUDIO-25
#[test]
fn a_planar_tone_above_1000_hz_is_refused_not_reduced() {
    let msg = &["Audio", "tone frequency", "1000 Hz", "REQ-AUDIO-25"];
    // The carrier alone.
    refused(compile("audio_entrainment", audio(json!({ "isochronicTonesHz": 40, "carrierHz": 10000 }))), msg);
    refused(compile("audio_entrainment", audio(json!({ "binauralBeatsHz": 10, "carrierHz": 1001 }))), msg);
    // The carrier plus the beat or pulse rate: 990 + 10.001 Hz is above the bound.
    refused(compile("audio_entrainment", audio(json!({ "binauralBeatsHz": 10.001, "carrierHz": 990 }))), msg);
    refused(compile("audio_entrainment", audio(json!({ "isochronicTonesHz": 10.001, "carrierHz": 990 }))), msg);
    // Exactly at the bound is allowed, in both modes.
    ok("audio_entrainment", audio(json!({ "binauralBeatsHz": 10, "carrierHz": 990 })));
    ok("audio_entrainment", audio(json!({ "isochronicTonesHz": 40, "carrierHz": 960 })));
    // A noise command produces no tone, so its carrier field is not a tone frequency.
    ok("audio_entrainment", audio(json!({ "noiseType": "pink", "carrierHz": 5000 })));
}
