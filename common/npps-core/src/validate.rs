//! The protocol validator (OI-NPPS-CORE-01): the checks of the web, iOS and Android validators, once.
//!
//! A runtime that validates for itself gives a different answer for the same file (the iOS and Android
//! validators divided charge by a sum of electrode areas the safety MCU never uses, and attributed limits
//! differently), so the checks are written here and every app and the simulator calls them.
//!
//! **No text is produced.** The core does not know a locale (CLAUDE.md §17). Each issue carries locale keys
//! and positional arguments; the caller resolves them with its own `t()`. A message is either a plain string
//! (a unit label or a value, never prose) or `{"key": "VALIDATE_…", "args": [message…]}`, and an argument
//! can itself be a message (a layer's prefix wraps the issue it prefixes). Numbers in arguments are already
//! rendered the way JavaScript renders them (`String(n)`, `toFixed(1)`), so every runtime prints the same
//! digits.
//!
//! The limits a protocol is checked against are the hardware ceilings (`common/npps/constants.json`, as
//! `constants::hardware_limits`) and the caller's resolved `NPLimitsSet`.

use crate::js::num_string;
use crate::wavelength::{parse_pbm_wavelength, Parsed};
use serde_json::{json, Map, Value};

use crate::constants::hardware_limits as hw;
use crate::constants::validation as threshold;

// ─── Messages ──────────────────────────────────────────────────────────────────

fn msg(key: &str, args: Vec<Value>) -> Value {
    if args.is_empty() { json!({ "key": key }) } else { json!({ "key": key, "args": args }) }
}

fn n(x: f64) -> Value {
    Value::String(num_string(x))
}

/// JavaScript's `toFixed(1)`: exact decimal value, ties rounded up (Rust's `{:.1}` rounds ties to even).
fn fixed1(x: f64) -> String {
    let exact = format!("{x:.64}");
    let (int, frac) = exact.split_once('.').unwrap_or((&exact, ""));
    let tail = &frac[1.min(frac.len())..];
    let tie = tail.starts_with('5') && tail[1..].bytes().all(|b| b == b'0');
    if tie && x >= 0.0 {
        let scaled = (format!("{int}{}", &frac[..1]).parse::<f64>().unwrap_or(0.0) + 1.0) / 10.0;
        return format!("{scaled:.1}");
    }
    format!("{x:.1}")
}

fn join(list: &Value, sep: &str) -> String {
    list.as_array().map(|a| a.iter().map(crate::js::value_string).collect::<Vec<_>>().join(sep)).unwrap_or_default()
}

fn includes(list: &Value, v: &str) -> bool {
    list.as_array().map_or(false, |a| a.iter().any(|x| x.as_str() == Some(v)))
}

// ─── Issue accumulation ────────────────────────────────────────────────────────

#[derive(Clone, Copy, PartialEq)]
enum Sev {
    Error,
    Warning,
}

struct Issues(Vec<Value>);

impl Issues {
    #[allow(clippy::too_many_arguments)]
    fn push(&mut self, sev: Sev, modality: Option<&str>, key: &str, param: &str, actual: Value, limit: Value, source: &str, message: Value) {
        let mut o = Map::new();
        o.insert("severity".into(), json!(if sev == Sev::Error { "error" } else { "warning" }));
        if let Some(m) = modality {
            o.insert("modality".into(), json!(m));
        }
        o.insert("parameterKey".into(), json!(key));
        o.insert("parameterName".into(), msg(param, vec![]));
        o.insert("actualValueDescription".into(), actual);
        o.insert("limitValueDescription".into(), limit);
        o.insert("limitSource".into(), json!(source));
        o.insert("message".into(), message);
        self.0.push(Value::Object(o));
    }
}

fn s(x: impl Into<String>) -> Value {
    Value::String(x.into())
}

/// A parameter of a modality block; a missing one is NaN, which fails every comparison as `undefined` does.
fn f(p: &Value, k: &str) -> f64 {
    p[k].as_f64().unwrap_or(f64::NAN)
}

/// A dosage ceiling at this limits level, or None when it is not set.
fn lim(l: &Value, k: &str) -> Option<f64> {
    l[k].as_f64()
}

/// `1m 30s`, as every runtime's session-duration messages have always written it.
fn fmt_seconds(s: f64) -> String {
    if s < 60.0 {
        return format!("{}s", num_string(s));
    }
    let (m, sec) = ((s / 60.0).floor(), s % 60.0);
    if sec == 0.0 { format!("{}m", num_string(m)) } else { format!("{}m {}s", num_string(m), num_string(sec)) }
}

fn fmt_hz(hz: f64) -> String {
    format!("{} Hz", num_string(hz))
}

fn is_continuous(interval: &Value) -> bool {
    interval["intervalOnSeconds"].as_f64().unwrap_or(0.0) == 0.0 && interval["intervalOffSeconds"].as_f64().unwrap_or(0.0) == 0.0
}

/// A configured `maxSessionDurationSeconds` against one on-interval (a continuous block has no interval to
/// exceed). `message` is the modality's own key, which names the modality in its wording.
fn session_duration(out: &mut Issues, kind: &str, l: &Value, interval: &Value, message: &str) {
    let Some(max) = lim(l, "maxSessionDurationSeconds") else { return };
    let on = interval["intervalOnSeconds"].as_f64().unwrap_or(0.0);
    if !is_continuous(interval) && on > max {
        out.push(Sev::Error, Some(kind), "sessionDuration", "VALIDATE_PARAM_SESSION_DURATION", s(fmt_seconds(on)), s(fmt_seconds(max)),
            "@maxSessionDurationSeconds", msg(message, vec![s(fmt_seconds(on)), s(fmt_seconds(max))]));
    }
}

/// `frequency: 0` is continuous wave, and CW has no duty cycle (OI-SESPWR-03), so a CW block whose duty is
/// anything but 100 % contradicts itself. The parser refuses the same block; this catches one built in an
/// editor, which never passes the parser.
fn continuous_wave(out: &mut Issues, kind: &str, p: &Value) {
    let (freq, duty) = (f(p, "frequencyHz"), f(p, "dutyCyclePercent"));
    if freq == 0.0 && duty != threshold::CW_DUTY_CYCLE_PERCENT as f64 {
        out.push(Sev::Error, Some(kind), "dutyCyclePercent", "VALIDATE_PARAM_DUTY_CYCLE", s(format!("{}%", num_string(duty))), s(format!("{}%", threshold::CW_DUTY_CYCLE_PERCENT)), "hardware",
            msg("VALIDATE_MSG_PBM_CW_DUTY", vec![n(duty)]));
    }
}

// ─── Per-modality ──────────────────────────────────────────────────────────────

fn modality(kind: &str, p: &Value, interval: &Value, duration: Option<f64>, limits: &Value, out: &mut Issues) {
    use Sev::{Error, Warning};
    let k = Some(kind);
    match kind {
        "pbm_transcranial" => {
            let l = &limits["pbmTranscranial"];
            let w = p["wavelength"].as_str().unwrap_or("");
            // One wavelength ("810nm"); the retired combined channel names are invalid. Whether the fitted
            // helmet can deliver it is eligibility, not validity.
            if matches!(parse_pbm_wavelength(w), Parsed::Invalid) {
                out.push(Error, k, "wavelength", "VALIDATE_PARAM_WAVELENGTH", s(w), s("\"810nm\""), "hardware",
                    msg("VALIDATE_MSG_PBM_TRANSCRANIAL_WAVELENGTH", vec![s(w)]));
            }
            let duty = f(p, "dutyCyclePercent");
            let freq = f(p, "frequencyHz");
            let irr = f(p, "irradianceMWcm2");
            continuous_wave(out, kind, p);
            if duty > hw::PBM_DUTY_CYCLE_MAX_PERCENT {
                out.push(Error, k, "dutyCyclePercent", "VALIDATE_PARAM_DUTY_CYCLE", s(format!("{}%", num_string(duty))),
                    s(format!("{}%", num_string(hw::PBM_DUTY_CYCLE_MAX_PERCENT))), "hardware",
                    msg("VALIDATE_MSG_PBM_TRANSCRANIAL_DUTYCYCLEPERCENT", vec![n(duty), n(hw::PBM_DUTY_CYCLE_MAX_PERCENT)]));
            }
            if freq < 0.0 {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", s(format!("{} Hz", num_string(freq))), s("≥0 Hz"), "hardware",
                    msg("VALIDATE_MSG_PBM_TRANSCRANIAL_FREQUENCYHZ", vec![]));
            }
            // Irradiance is absolute, so the 400 mW/cm² peak (CLAUDE.md §3) is checked directly.
            if irr > hw::PBM_PULSED_PEAK_MW_CM2 {
                out.push(Error, k, "irradianceMWcm2", "VALIDATE_PARAM_IRRADIANCE", s(format!("{} mW/cm²", num_string(irr))),
                    s(format!("{} mW/cm²", num_string(hw::PBM_PULSED_PEAK_MW_CM2))), "hardware",
                    msg("VALIDATE_MSG_PBM_TRANSCRANIAL_IRRADIANCE", vec![n(irr), n(hw::PBM_PULSED_PEAK_MW_CM2)]));
            }
            if let Some(m) = lim(l, "maxIrradianceMWcm2").filter(|m| irr > *m) {
                out.push(Error, k, "irradianceMWcm2", "VALIDATE_PARAM_IRRADIANCE", s(format!("{} mW/cm²", num_string(irr))),
                    s(format!("{} mW/cm²", num_string(m))), "@maxIrradianceMWcm2", msg("VALIDATE_MSG_PBM_TRANSCRANIAL_IRRADIANCE", vec![n(irr), n(m)]));
            }
            if let Some(m) = lim(l, "maxFrequencyHz").filter(|m| freq > *m) {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", s(format!("{} Hz", num_string(freq))),
                    s(format!("{} Hz", num_string(m))), "@maxFrequencyHz", msg("VALIDATE_MSG_PBM_TRANSCRANIAL_FREQUENCYHZ_2", vec![n(freq), n(m)]));
            }
            if let Some(m) = lim(l, "maxDutyCyclePercent").filter(|m| duty > *m) {
                out.push(Error, k, "dutyCyclePercent", "VALIDATE_PARAM_DUTY_CYCLE", s(format!("{}%", num_string(duty))),
                    s(format!("{}%", num_string(m))), "@maxDutyCyclePercent", msg("VALIDATE_MSG_PBM_TRANSCRANIAL_DUTYCYCLEPERCENT_2", vec![n(duty), n(m)]));
            }
            // Estimated session dose against the configured ceiling: the average irradiance over the session.
            if let (Some(max), Some(dur)) = (lim(l, "maxSessionDoseJCm2"), duration.filter(|d| *d > 0.0)) {
                let average = if freq == 0.0 { irr } else { irr * duty / 100.0 };
                let dose = average * dur / 1000.0;
                if dose > max {
                    out.push(Error, k, "sessionDoseJCm2", "VALIDATE_PARAM_SESSION_DOSE", s(format!("{} J/cm²", fixed1(dose))),
                        s(format!("{} J/cm²", fixed1(max))), "@maxSessionDoseJCm2",
                        msg("VALIDATE_MSG_PBM_SESSION_DOSE", vec![s(fixed1(dose)), s(fixed1(max))]));
                }
            }
        }
        "pbm_intranasal" => {
            let l = &limits["pbmIntranasal"];
            let duty = f(p, "dutyCyclePercent");
            let irr = f(p, "irradianceMWcm2");
            continuous_wave(out, kind, p);
            if duty > hw::PBM_DUTY_CYCLE_MAX_PERCENT {
                out.push(Error, k, "dutyCyclePercent", "VALIDATE_PARAM_DUTY_CYCLE", s(format!("{}%", num_string(duty))),
                    s(format!("{}%", num_string(hw::PBM_DUTY_CYCLE_MAX_PERCENT))), "hardware",
                    msg("VALIDATE_MSG_PBM_INTRANASAL_DUTYCYCLEPERCENT", vec![n(duty), n(hw::PBM_DUTY_CYCLE_MAX_PERCENT)]));
            }
            if let Some(m) = lim(l, "maxIrradianceMWcm2").filter(|m| irr > *m) {
                out.push(Error, k, "irradianceMWcm2", "VALIDATE_PARAM_IRRADIANCE", s(format!("{} mW/cm²", num_string(irr))),
                    s(format!("{} mW/cm²", num_string(m))), "@maxIrradianceMWcm2", msg("VALIDATE_MSG_PBM_INTRANASAL_IRRADIANCE", vec![n(irr), n(m)]));
            }
            session_duration(out, kind, l, interval, "VALIDATE_MSG_GENERAL_SESSIONDURATION_8");
        }
        "eeg_neurofeedback" => {
            let l = &limits["eegNeurofeedback"];
            if l["requireClosedLoop"].as_bool() == Some(true) && p["closedLoopEnabled"].as_bool() != Some(true) {
                out.push(Error, k, "closedLoopEnabled", "VALIDATE_PARAM_CLOSED_LOOP", s("Disabled"), s("Required"), "@requireClosedLoop",
                    msg("VALIDATE_MSG_EEG_NEUROFEEDBACK_CLOSEDLOOPENABLED", vec![]));
            }
            let band = p["band"].as_str().unwrap_or("");
            if l["allowedBands"].is_array() && !includes(&l["allowedBands"], band) {
                out.push(Error, k, "band", "VALIDATE_PARAM_BAND", s(band), s(join(&l["allowedBands"], "/")), "@allowedBands",
                    msg("VALIDATE_MSG_EEG_NEUROFEEDBACK_BAND", vec![s(band), s(join(&l["allowedBands"], ", "))]));
            }
        }
        "bes_tacs" => {
            let l = &limits["besTacs"];
            let ma = f(p, "intensityMilliamps");
            let freq = f(p, "frequencyHz");
            if ma > hw::BES_TACS_MAX_MILLIAMPS {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(hw::BES_TACS_MAX_MILLIAMPS))), "hardware",
                    msg("VALIDATE_MSG_BES_TACS_INTENSITYMILLIAMPS", vec![n(ma), n(hw::BES_TACS_MAX_MILLIAMPS)]));
            }
            if freq < hw::BES_TACS_MIN_HZ || freq > hw::BES_TACS_MAX_HZ {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", s(format!("{} Hz", num_string(freq))),
                    s(format!("{}–{} Hz", num_string(hw::BES_TACS_MIN_HZ), num_string(hw::BES_TACS_MAX_HZ))), "hardware",
                    msg("VALIDATE_MSG_BES_TACS_FREQUENCYHZ", vec![n(freq), n(hw::BES_TACS_MIN_HZ), n(hw::BES_TACS_MAX_HZ)]));
            }
            if let Some(m) = lim(l, "maxIntensityMilliamps").filter(|m| ma > *m) {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(m))), "@maxIntensityMilliamps", msg("VALIDATE_MSG_BES_TACS_INTENSITYMILLIAMPS_2", vec![n(ma), n(m)]));
            }
            if let Some(m) = lim(l, "maxFrequencyHz").filter(|m| freq > *m) {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", s(format!("{} Hz", num_string(freq))),
                    s(format!("{} Hz", num_string(m))), "@maxFrequencyHz", msg("VALIDATE_MSG_BES_TACS_FREQUENCYHZ_2", vec![n(freq), n(m)]));
            }
            if let Some(m) = lim(l, "minFrequencyHz").filter(|m| freq < *m) {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", s(format!("{} Hz", num_string(freq))),
                    s(format!("≥{} Hz", num_string(m))), "@minFrequencyHz", msg("VALIDATE_MSG_BES_TACS_FREQUENCYHZ_3", vec![n(freq), n(m)]));
            }
            session_duration(out, kind, l, interval, "VALIDATE_MSG_GENERAL_SESSIONDURATION_7");
        }
        "tdcs" => {
            let l = &limits["tdcs"];
            let ma = f(p, "intensityMilliamps");
            let pairs = p["electrodePairs"].as_array().map_or(0, Vec::len) as f64;
            let ramp = f(p, "rampSeconds");
            let area = f(p, "electrodeAreaCm2");
            if ma < hw::TDCS_MIN_MILLIAMPS || ma > hw::TDCS_MAX_MILLIAMPS {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{}–{} mA", num_string(hw::TDCS_MIN_MILLIAMPS), num_string(hw::TDCS_MAX_MILLIAMPS))), "hardware",
                    msg("VALIDATE_MSG_TDCS_INTENSITYMILLIAMPS", vec![n(ma), n(hw::TDCS_MIN_MILLIAMPS), n(hw::TDCS_MAX_MILLIAMPS)]));
            }
            if pairs > hw::TDCS_MAX_ELECTRODE_PAIRS {
                out.push(Error, k, "electrodePairs", "VALIDATE_PARAM_ELECTRODE_PAIRS", s(num_string(pairs)),
                    s(format!("≤{}", num_string(hw::TDCS_MAX_ELECTRODE_PAIRS))), "hardware",
                    msg("VALIDATE_MSG_TDCS_ELECTRODEPAIRS", vec![n(pairs), n(hw::TDCS_MAX_ELECTRODE_PAIRS)]));
            }
            if ramp < hw::TDCS_RAMP_SECONDS {
                out.push(Error, k, "rampSeconds", "VALIDATE_PARAM_RAMP_TIME", s(format!("{}s", num_string(ramp))),
                    s(format!("{}s", num_string(hw::TDCS_RAMP_SECONDS))), "hardware",
                    msg("VALIDATE_MSG_TDCS_RAMPSECONDS", vec![n(ramp), n(hw::TDCS_RAMP_SECONDS)]));
            }
            // OI-CHARGE-04: the declared pad geometry the charge-density ceiling divides by. An undeclared or
            // unencodable area is an error, not a fallback: the hub refuses a 0 area and the safety MCU's
            // geometry gate holds tDCS off.
            if !(area > 0.0) || area > hw::TDCS_MAX_ELECTRODE_AREA_CM2 {
                out.push(Error, k, "electrodeAreaCm2", "VALIDATE_PARAM_ELECTRODE_AREA", s(format!("{} cm²", num_string(area))),
                    s(format!("0 < A ≤ {} cm²", num_string(hw::TDCS_MAX_ELECTRODE_AREA_CM2))), "hardware",
                    msg("VALIDATE_MSG_TDCS_ELECTRODEAREA", vec![n(area), n(hw::TDCS_MAX_ELECTRODE_AREA_CM2)]));
            }
            if let Some(m) = lim(l, "maxIntensityMilliamps").filter(|m| ma > *m) {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(m))), "@maxIntensityMilliamps", msg("VALIDATE_MSG_TDCS_INTENSITYMILLIAMPS_2", vec![n(ma), n(m)]));
            }
            session_duration(out, kind, l, interval, "VALIDATE_MSG_GENERAL_SESSIONDURATION_6");
        }
        "vns_hrv" => {
            let l = &limits["vnsHrv"];
            let ma = f(p, "intensityMilliamps");
            let freq = f(p, "frequencyHz");
            if ma > hw::VNS_MAX_MILLIAMPS {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(hw::VNS_MAX_MILLIAMPS))), "hardware",
                    msg("VALIDATE_MSG_VNS_HRV_INTENSITYMILLIAMPS", vec![n(ma), n(hw::VNS_MAX_MILLIAMPS)]));
            }
            if freq < hw::VNS_MIN_HZ || freq > hw::VNS_MAX_HZ {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", s(format!("{} Hz", num_string(freq))),
                    s(format!("{}–{} Hz", num_string(hw::VNS_MIN_HZ), num_string(hw::VNS_MAX_HZ))), "hardware",
                    msg("VALIDATE_MSG_VNS_HRV_FREQUENCYHZ", vec![n(freq), n(hw::VNS_MIN_HZ), n(hw::VNS_MAX_HZ)]));
            }
            if let Some(m) = lim(l, "maxIntensityMilliamps").filter(|m| ma > *m) {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(m))), "@maxIntensityMilliamps", msg("VALIDATE_MSG_VNS_HRV_INTENSITYMILLIAMPS_2", vec![n(ma), n(m)]));
            }
            if let Some(m) = lim(l, "maxFrequencyHz").filter(|m| freq > *m) {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", s(format!("{} Hz", num_string(freq))),
                    s(format!("{} Hz", num_string(m))), "@maxFrequencyHz", msg("VALIDATE_MSG_VNS_HRV_FREQUENCYHZ_2", vec![n(freq), n(m)]));
            }
            session_duration(out, kind, l, interval, "VALIDATE_MSG_GENERAL_SESSIONDURATION_5");
            let proto = p["hrvProtocol"].as_str().unwrap_or("");
            if l["allowedProtocols"].is_array() && !includes(&l["allowedProtocols"], proto) {
                out.push(Error, k, "hrvProtocol", "VALIDATE_PARAM_HRV_PROTOCOL", s(proto), s(join(&l["allowedProtocols"], "/")), "@allowedProtocols",
                    msg("VALIDATE_MSG_VNS_HRV_HRVPROTOCOL", vec![s(proto), s(join(&l["allowedProtocols"], ", "))]));
            }
        }
        "audio_entrainment" => {
            let l = &limits["audioEntrainment"];
            let vol = f(p, "volumeDb");
            if let Some(m) = lim(l, "maxVolumeDb").filter(|m| vol > *m) {
                out.push(Error, k, "volumeDb", "VALIDATE_PARAM_VOLUME", s(format!("{} dB SPL", num_string(vol))),
                    s(format!("{} dB SPL", num_string(m))), "@maxVolumeDb", msg("VALIDATE_MSG_AUDIO_ENTRAINMENT_VOLUMEDB", vec![n(vol), n(m)]));
            }
            if let (Some(m), Some(b)) = (lim(l, "maxBinauralBeatsHz"), p["binauralBeatsHz"].as_f64()) {
                if b > m {
                    out.push(Error, k, "binauralBeatsHz", "VALIDATE_PARAM_BINAURAL_BEAT", s(format!("{} Hz", num_string(b))),
                        s(format!("{} Hz", num_string(m))), "@maxBinauralBeatsHz", msg("VALIDATE_MSG_AUDIO_ENTRAINMENT_BINAURALBEATSHZ", vec![n(b), n(m)]));
                }
            }
            if let (Some(m), Some(b)) = (lim(l, "maxIsochronicTonesHz"), p["isochronicTonesHz"].as_f64()) {
                if b > m {
                    out.push(Error, k, "isochronicTonesHz", "VALIDATE_PARAM_ISOCHRONIC_TONE", s(format!("{} Hz", num_string(b))),
                        s(format!("{} Hz", num_string(m))), "@maxIsochronicTonesHz", msg("VALIDATE_MSG_AUDIO_ENTRAINMENT_ISOCHRONICTONESHZ", vec![n(b), n(m)]));
                }
            }
        }
        "visual_stimulation" => {
            let l = &limits["visualStimulation"];
            let freq = f(p, "frequencyHz");
            let fs = s(format!("{} Hz", num_string(freq)));
            if freq > hw::VISUAL_MAX_HZ {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", fs.clone(), s(format!("{} Hz", num_string(hw::VISUAL_MAX_HZ))), "hardware",
                    msg("VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ", vec![n(freq), n(hw::VISUAL_MAX_HZ)]));
            }
            // Photoparoxysmal risk zone (3–30 Hz).
            if freq >= hw::VISUAL_HIGH_RISK_MIN_HZ && freq <= hw::VISUAL_HIGH_RISK_MAX_HZ {
                let (lo, hi) = (n(hw::VISUAL_HIGH_RISK_MIN_HZ), n(hw::VISUAL_HIGH_RISK_MAX_HZ));
                if l["blockHighRiskRange"].as_bool() == Some(true) {
                    out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", fs.clone(),
                        msg("VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_2_ARG1", vec![lo.clone(), hi.clone()]), "@blockHighRiskRange",
                        msg("VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_2", vec![n(freq), lo, hi]));
                } else {
                    out.push(Warning, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", fs.clone(),
                        msg("VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_3_ARG1", vec![lo.clone(), hi.clone()]), "hardware",
                        msg("VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_3", vec![n(freq), lo, hi]));
                }
            }
            if let Some(m) = lim(l, "maxFrequencyHz").filter(|m| freq > *m) {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", fs.clone(), s(format!("{} Hz", num_string(m))), "@maxFrequencyHz",
                    msg("VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_4", vec![n(freq), n(m)]));
            }
            if let Some(m) = lim(l, "minFrequencyHz").filter(|m| freq < *m) {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", fs, s(format!("≥{} Hz", num_string(m))), "@minFrequencyHz",
                    msg("VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_5", vec![n(freq), n(m)]));
            }
            let mode = p["mode"].as_str().unwrap_or("");
            if l["allowedModes"].is_array() && !includes(&l["allowedModes"], mode) {
                out.push(Error, k, "mode", "VALIDATE_PARAM_MODE", s(mode), s(join(&l["allowedModes"], "/")), "@allowedModes",
                    msg("VALIDATE_MSG_VISUAL_STIMULATION_MODE", vec![s(mode), s(join(&l["allowedModes"], ", "))]));
            }
        }
        // No dedicated hardware limits for qEEG beyond electrode safety, and no dosage limits are defined.
        "qeeg_21ch" => {}
        "tms" => {
            let l = &limits["tms"];
            let pct = f(p, "intensityPercentMT");
            let pulses = f(p, "pulseCount");
            if let Some(m) = lim(l, "maxIntensityPercentMT").filter(|m| pct > *m) {
                out.push(Error, k, "intensityPercentMT", "VALIDATE_PARAM_INTENSITY", s(format!("{}% MT", num_string(pct))),
                    s(format!("{}% MT", num_string(m))), "@maxIntensityPercentMT", msg("VALIDATE_MSG_TMS_INTENSITYPERCENTMT", vec![n(pct), n(m)]));
            }
            if let Some(m) = lim(l, "maxPulsesPerSession").filter(|m| pulses > *m) {
                out.push(Error, k, "pulseCount", "VALIDATE_PARAM_PULSES", s(num_string(pulses)), s(num_string(m)), "@maxPulsesPerSession",
                    msg("VALIDATE_MSG_TMS_PULSECOUNT", vec![n(pulses), n(m)]));
            }
            let proto = p["tmsProtocol"].as_str().unwrap_or("");
            if l["allowedProtocols"].is_array() && !includes(&l["allowedProtocols"], proto) {
                out.push(Error, k, "tmsProtocol", "VALIDATE_PARAM_PROTOCOL", s(proto), s(join(&l["allowedProtocols"], "/")), "@allowedProtocols",
                    msg("VALIDATE_MSG_TMS_TMSPROTOCOL", vec![s(proto), s(join(&l["allowedProtocols"], ", "))]));
            }
            let target = p["target"].as_str().unwrap_or("");
            if l["allowedTargets"].is_array() && !includes(&l["allowedTargets"], target) {
                out.push(Error, k, "target", "VALIDATE_PARAM_TARGET", s(target), s(join(&l["allowedTargets"], "/")), "@allowedTargets",
                    msg("VALIDATE_MSG_TMS_TARGET", vec![s(target), s(join(&l["allowedTargets"], ", "))]));
            }
            if pct > threshold::TMS_HIGH_INTENSITY_PERCENT_MT as f64 {
                out.push(Warning, k, "intensityPercentMT", "VALIDATE_PARAM_INTENSITY_MT", s(format!("{}% MT", num_string(pct))), s(format!("≤{}% MT", threshold::TMS_HIGH_INTENSITY_PERCENT_MT)), "hardware",
                    msg("VALIDATE_MSG_GENERAL_INTENSITYPERCENTMT", vec![n(pct)]));
            }
        }
        "pbm_deep_1170nm" => {
            let l = &limits["pbmDeep1170nm"];
            let i = f(p, "intensityMWcm2");
            let duty = f(p, "dutyCyclePercent");
            if i > hw::PBM_DEEP_MAX_MW_CM2 {
                out.push(Error, k, "intensityMWcm2", "VALIDATE_PARAM_INTENSITY", s(format!("{} mW/cm²", num_string(i.trunc()))),
                    s(format!("{} mW/cm²", num_string(hw::PBM_DEEP_MAX_MW_CM2))), "hardware",
                    msg("VALIDATE_MSG_GENERAL_INTENSITYMWCM2_2", vec![n(i.trunc()), n(hw::PBM_DEEP_MAX_MW_CM2)]));
            }
            continuous_wave(out, kind, p);
            if let Some(m) = lim(l, "maxIntensityMWcm2").filter(|m| i > *m) {
                out.push(Error, k, "intensityMWcm2", "VALIDATE_PARAM_INTENSITY", s(format!("{} mW/cm²", num_string(i))),
                    s(format!("{} mW/cm²", num_string(m))), "@maxIntensityMWcm2", msg("VALIDATE_MSG_PBM_DEEP_1170NM_INTENSITYMWCM2", vec![n(i), n(m)]));
            }
            if duty > hw::PBM_DUTY_CYCLE_MAX_PERCENT {
                out.push(Error, k, "dutyCyclePercent", "VALIDATE_PARAM_DUTY_CYCLE", s(format!("{}%", num_string(duty))),
                    s(format!("{}%", num_string(hw::PBM_DUTY_CYCLE_MAX_PERCENT))), "hardware",
                    msg("VALIDATE_MSG_PBM_DEEP_1170NM_DUTYCYCLEPERCENT", vec![n(duty), n(hw::PBM_DUTY_CYCLE_MAX_PERCENT)]));
            }
            session_duration(out, kind, l, interval, "VALIDATE_MSG_GENERAL_SESSIONDURATION_4");
        }
        "clinical_tacs" => {
            let l = &limits["clinicalTacs"];
            let ma = f(p, "intensityMilliamps");
            let ch = f(p, "channelCount");
            if ma > hw::CLINICAL_TACS_MAX_MILLIAMPS {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(hw::CLINICAL_TACS_MAX_MILLIAMPS))), "hardware",
                    msg("VALIDATE_MSG_CLINICAL_TACS_INTENSITYMILLIAMPS", vec![n(ma), n(hw::CLINICAL_TACS_MAX_MILLIAMPS)]));
            }
            // OI-TACS-01: an out-of-range count is an error, not a quiet reduction: a clinician who authored 24
            // gets told, not obeyed in part.
            if ch > hw::CLINICAL_TACS_MAX_CHANNELS || ch < 1.0 {
                out.push(Error, k, "channelCount", "VALIDATE_PARAM_CHANNEL_COUNT", s(num_string(ch)),
                    s(format!("1–{}", num_string(hw::CLINICAL_TACS_MAX_CHANNELS))), "hardware",
                    msg("VALIDATE_MSG_CLINICAL_TACS_CHANNELCOUNT", vec![n(ch), n(hw::CLINICAL_TACS_MAX_CHANNELS)]));
            }
            if let Some(m) = lim(l, "maxIntensityMilliamps").filter(|m| ma > *m) {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(m))), "@maxIntensityMilliamps", msg("VALIDATE_MSG_CLINICAL_TACS_INTENSITYMILLIAMPS_2", vec![n(ma), n(m)]));
            }
            session_duration(out, kind, l, interval, "VALIDATE_MSG_GENERAL_SESSIONDURATION_3");
        }
        "hd_tdcs" => {
            let l = &limits["hdTdcs"];
            let ma = f(p, "intensityMilliamps");
            if ma > hw::HD_TDCS_MAX_MILLIAMPS_PER_ELECTRODE {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(hw::HD_TDCS_MAX_MILLIAMPS_PER_ELECTRODE))), "hardware",
                    msg("VALIDATE_MSG_HD_TDCS_INTENSITYMILLIAMPS", vec![n(ma), n(hw::HD_TDCS_MAX_MILLIAMPS_PER_ELECTRODE)]));
            }
            if let Some(m) = lim(l, "maxIntensityMilliamps").filter(|m| ma > *m) {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(m))), "@maxIntensityMilliamps", msg("VALIDATE_MSG_HD_TDCS_INTENSITYMILLIAMPS_2", vec![n(ma), n(m)]));
            }
            session_duration(out, kind, l, interval, "VALIDATE_MSG_GENERAL_SESSIONDURATION_2");
            let montage = p["montage"].as_str().unwrap_or("");
            if l["allowedMontages"].is_array() && !includes(&l["allowedMontages"], montage) {
                out.push(Error, k, "montage", "VALIDATE_PARAM_MONTAGE", s(montage), s(join(&l["allowedMontages"], "/")), "@allowedMontages",
                    msg("VALIDATE_MSG_HD_TDCS_MONTAGE", vec![s(montage), s(join(&l["allowedMontages"], ", "))]));
            }
        }
        "cervical_vns" => {
            let l = &limits["cervicalVns"];
            let ma = f(p, "intensityMilliamps");
            let freq = f(p, "frequencyHz");
            // The cardiac interlock is always active at firmware level.
            if ma > hw::CERVICAL_VNS_MAX_MILLIAMPS {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(hw::CERVICAL_VNS_MAX_MILLIAMPS))), "hardware",
                    msg("VALIDATE_MSG_CERVICAL_VNS_INTENSITYMILLIAMPS", vec![n(ma)]));
            }
            if let Some(m) = lim(l, "maxIntensityMilliamps").filter(|m| ma > *m) {
                out.push(Error, k, "intensityMilliamps", "VALIDATE_PARAM_INTENSITY", s(format!("{} mA", num_string(ma))),
                    s(format!("{} mA", num_string(m))), "@maxIntensityMilliamps", msg("VALIDATE_MSG_CERVICAL_VNS_INTENSITYMILLIAMPS_2", vec![n(ma), n(m)]));
            }
            if freq < hw::VNS_MIN_HZ || freq > hw::VNS_MAX_HZ {
                out.push(Error, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", s(fmt_hz(freq)),
                    s(format!("{}–{}", fmt_hz(hw::VNS_MIN_HZ), fmt_hz(hw::VNS_MAX_HZ))), "hardware",
                    msg("VALIDATE_MSG_GENERAL_FREQUENCYHZ_2", vec![s(fmt_hz(freq)), n(hw::VNS_MIN_HZ), n(hw::VNS_MAX_HZ)]));
            }
            session_duration(out, kind, l, interval, "VALIDATE_MSG_GENERAL_SESSIONDURATION");
            // Always say so: the interlock is the safety MCU's and no limit set can lift it.
            out.push(Warning, k, "cardiacInterlock", "VALIDATE_PARAM_CARDIAC_INTERLOCK", msg("VALIDATE_ACTUAL_ALWAYS_ON", vec![]),
                msg("VALIDATE_LIMIT_NON_OVERRIDABLE", vec![]), "hardware", msg("VALIDATE_MSG_GENERAL_CARDIACINTERLOCK", vec![]));
        }
        "vibrotactile_40hz" => {
            let l = &limits["vibrotactile40hz"];
            let g = f(p, "intensityG");
            if g > hw::VIBROTACTILE_MAX_G || g < hw::VIBROTACTILE_MIN_G {
                out.push(Error, k, "intensityG", "VALIDATE_PARAM_INTENSITY", s(format!("{} G", num_string(g))),
                    s(format!("{}–{} G", num_string(hw::VIBROTACTILE_MIN_G), num_string(hw::VIBROTACTILE_MAX_G))), "hardware",
                    msg("VALIDATE_MSG_VIBROTACTILE_40HZ_INTENSITYG", vec![n(g), n(hw::VIBROTACTILE_MIN_G), n(hw::VIBROTACTILE_MAX_G)]));
            }
            // The firmware locks the drive to 40 Hz; a model that states another frequency is told so. A `.npps`
            // file cannot state one, so this only fires for a model built in an editor.
            if let Some(freq) = p["frequencyHz"].as_f64().filter(|x| (x - hw::VIBROTACTILE_FREQUENCY_HZ).abs() > hw::VIBROTACTILE_FREQ_TOLERANCE_HZ) {
                out.push(Warning, k, "frequencyHz", "VALIDATE_PARAM_FREQUENCY", s(fmt_hz(freq)),
                    s(format!("{} Hz ±{} Hz", num_string(hw::VIBROTACTILE_FREQUENCY_HZ), num_string(hw::VIBROTACTILE_FREQ_TOLERANCE_HZ))), "hardware",
                    msg("VALIDATE_MSG_GENERAL_FREQUENCYHZ", vec![s(fmt_hz(freq))]));
            }
            if let Some(m) = lim(l, "maxIntensityG").filter(|m| g > *m) {
                out.push(Error, k, "intensityG", "VALIDATE_PARAM_INTENSITY", s(format!("{} G", num_string(g))),
                    s(format!("{} G", num_string(m))), "@maxIntensityG", msg("VALIDATE_MSG_VIBROTACTILE_40HZ_INTENSITYG_2", vec![n(g), n(m)]));
            }
        }
        _ => {}
    }
}

// ─── Protocol ──────────────────────────────────────────────────────────────────

/// The locale key of a modality's display name, as `MODALITY_<ID>_NAME` (CLAUDE.md §17).
fn modality_name_key(kind: &str) -> String {
    format!("MODALITY_{}_NAME", kind.to_ascii_uppercase())
}

/// What a validation may be told beyond the protocol and the resolved limits.
#[derive(Default)]
pub struct Context<'a> {
    /// Zone name → its socket ids (the namespace, as the compiler takes it). When present, a PBM block that
    /// names a zone the namespace lacks, or names only empty zones, is an error; when absent that is not checked.
    pub zones: Option<&'a Value>,
    /// Where each configured limit came from, as `{"<modalityProperty>": {"<limitField>": "global"|"helmet"|"individual"}}`
    /// (iOS's `NPLimitSourceMap`). A limit with no entry takes the resolved set's own `level`.
    pub limit_sources: Option<&'a Value>,
}

/// The tier a configured limit is attributed to: its entry in the source map, else the resolved set's level.
fn dosage_source(ctx: &Context, limits: &Value, modality: &str, field: &str) -> String {
    let named = ctx
        .limit_sources
        .and_then(|m| m[crate::parser::blocks::camel_modality(modality)][field].as_str())
        .filter(|t| matches!(*t, "global" | "helmet" | "individual"));
    named.or_else(|| limits["level"].as_str().filter(|t| matches!(*t, "global" | "helmet" | "individual"))).unwrap_or("global").to_string()
}

/// A PBM block's named zones against the namespace: the first the namespace lacks, or none resolving to a socket.
fn zone_target(out: &mut Issues, params: &Value, zones: &Value) {
    if params["zones"] == "clinician_selected" {
        return; // unresolvable by design until the operator picks sockets at session start
    }
    let refs: Vec<&str> = params["zoneRefs"].as_array().map(|a| a.iter().filter_map(Value::as_str).collect()).unwrap_or_default();
    let display = refs.join(", ");
    if let Some(unknown) = refs.iter().find(|z| zones[**z].is_null()) {
        out.push(Sev::Error, Some("pbm_transcranial"), "target", "VALIDATE_PARAM_TARGET_ZONES", s(display), msg("VALIDATE_LIMIT_NAMED_ZONE", vec![]), "hardware",
            msg("SOCKET_ERR_UNKNOWN_ZONE", vec![s(*unknown)]));
        return;
    }
    let any = refs.iter().any(|z| zones[*z].as_array().map_or(false, |a| !a.is_empty()));
    if !any {
        out.push(Sev::Error, Some("pbm_transcranial"), "target", "VALIDATE_PARAM_TARGET_ZONES", s(display.clone()), msg("VALIDATE_LIMIT_ONE_SOCKET", vec![]), "hardware",
            msg("VALIDATE_MSG_GENERAL_TARGET_2", vec![s(display)]));
    }
}

fn protocol(def: &Value, limits: &Value, ctx: &Context) -> Vec<Value> {
    use Sev::{Error, Warning};
    let mut out = Issues(Vec::new());
    let enabled: Vec<&Value> = def["modalities"].as_array().map(|a| a.iter().filter(|m| m["enabled"] != false).collect()).unwrap_or_default();
    let kind_of = |m: &Value| m["type"].as_str().unwrap_or("").to_string();

    if enabled.is_empty() {
        out.push(Error, None, "modalities", "VALIDATE_PARAM_MODALITIES", s("0"), s("≥1"), "hardware", msg("VALIDATE_MSG_GENERAL_MODALITIES", vec![]));
    }

    let timing = &def["timingMode"];
    let duration = if timing["type"] == "duration" { timing["seconds"].as_f64() } else { None };
    if let Some(dur) = duration {
        if dur <= 0.0 {
            // A zero or negative duration is nonsensical (ISC-47): an error, where a short one is only a warning.
            out.push(Error, None, "duration", "VALIDATE_PARAM_DURATION", s(format!("{}s", num_string(dur))), s("> 0s"), "hardware",
                msg("VALIDATE_MSG_GENERAL_DURATION_3", vec![]));
        } else if dur < threshold::SESSION_SHORT_WARNING_SECONDS as f64 {
            out.push(Warning, None, "duration", "VALIDATE_PARAM_DURATION", s(format!("{}s", num_string(dur))), s(format!("{}s", threshold::SESSION_SHORT_WARNING_SECONDS)), "hardware",
                msg("VALIDATE_MSG_GENERAL_DURATION", vec![]));
        }
        if dur > threshold::SESSION_LONG_WARNING_SECONDS as f64 {
            out.push(Warning, None, "duration", "VALIDATE_PARAM_DURATION", s(format!("{}m", num_string((dur / 60.0).floor()))), s(format!("{}m", threshold::SESSION_LONG_WARNING_SECONDS / 60)), "hardware",
                msg("VALIDATE_MSG_GENERAL_DURATION_2", vec![]));
        }
    }

    // DC per-session charge density (OI-CHARGE-04, OI-CHARGE-05). It is PER ELECTRODE: the full session
    // current passes through each electrode of a pair, so the denominator is one electrode's area, not the
    // sum. `I(mA) × t(s) / A(cm²)` is mC/cm², and the check is `>=` because the safety MCU trips at `>=`
    // (np_charge_monitor.c), so a protocol landing exactly on the ceiling would be cut by the enforcer.
    if let Some(dur) = duration.filter(|d| *d > 0.0) {
        for m in &enabled {
            if kind_of(m) != "tdcs" {
                continue;
            }
            let area = f(&m["params"], "electrodeAreaCm2");
            if !(area > 0.0) {
                continue; // already reported by the per-modality check
            }
            let density = (f(&m["params"], "intensityMilliamps") * dur) / area;
            if density >= hw::TDCS_MAX_SESSION_CHARGE_DENSITY_MC_CM2 {
                out.push(Error, Some("tdcs"), "chargeDensityMCcm2", "VALIDATE_PARAM_CHARGE_DENSITY",
                    s(format!("{} mC/cm²", fixed1(density))), s(format!("{} mC/cm²", num_string(hw::TDCS_MAX_SESSION_CHARGE_DENSITY_MC_CM2))), "hardware",
                    msg("VALIDATE_MSG_TDCS_CHARGEDENSITY", vec![s(fixed1(density)), n(hw::TDCS_MAX_SESSION_CHARGE_DENSITY_MC_CM2)]));
            }
        }
    }

    // Pulsed / AC per-phase charge density (OI-CHARGE-05 (b)). BES/tACS, VNS, cervical VNS and clinical tACS
    // are charge-balanced biphasic, so what has a damage threshold is charge PER PHASE, which is what the
    // safety MCU enforces. The phase is the half-period of a periodic waveform and the pulse width of a pulse
    // train; VNS and cervical VNS author no pulse width, so the firmware's own 250 µs default is used.
    {
        // A sinusoid delivers 2/π of a rectangular phase of the same duration and peak; the safety MCU applies
        // the same factor, and omitting it would make the app 57 % stricter than the enforcer at 0.5 Hz.
        let sine_phase_factor = 2.0 / std::f64::consts::PI;
        for m in &enabled {
            let kind = kind_of(m);
            let p = &m["params"];
            let half_period = |hz: f64| if hz > 0.0 { 1.0 / (2.0 * hz) } else { 0.0 };
            let (amplitude, phase, area, waveform) = match kind.as_str() {
                "bes_tacs" => (f(p, "intensityMilliamps"), half_period(f(p, "frequencyHz")), hw::BES_ELECTRODE_AREA_CM2, p["waveform"].as_str().unwrap_or("")),
                "clinical_tacs" => (f(p, "intensityMilliamps"), half_period(f(p, "frequencyHz")), hw::BES_ELECTRODE_AREA_CM2, p["waveform"].as_str().unwrap_or("")),
                "vns_hrv" => (f(p, "intensityMilliamps"), hw::VNS_DEFAULT_PULSE_WIDTH_SECONDS, hw::VNS_ELECTRODE_AREA_CM2, "square"),
                "cervical_vns" => (f(p, "intensityMilliamps"), hw::VNS_DEFAULT_PULSE_WIDTH_SECONDS, hw::CERVICAL_VNS_ELECTRODE_AREA_CM2, "square"),
                _ => continue,
            };
            if !(amplitude > 0.0) || !(phase > 0.0) || !(area > 0.0) {
                continue;
            }
            // mA × s = mC; × 1000 → µC.
            let rectangular = amplitude * phase * 1000.0;
            let phase_uc = if waveform == "sinusoidal" { rectangular * sine_phase_factor } else { rectangular };
            let density = phase_uc / area;
            // `>=`, matching the safety MCU's comparator exactly.
            if density >= hw::PULSED_MAX_PHASE_CHARGE_DENSITY_UC_CM2 {
                out.push(Error, Some(&kind), "phaseChargeDensityUCcm2", "VALIDATE_PARAM_PHASE_CHARGE_DENSITY",
                    s(format!("{} µC/cm²", fixed1(density))), s(format!("{} µC/cm²", num_string(hw::PULSED_MAX_PHASE_CHARGE_DENSITY_UC_CM2))), "hardware",
                    msg("VALIDATE_MSG_PULSED_PHASECHARGE", vec![msg(&modality_name_key(&kind), vec![]), s(fixed1(density)), n(hw::PULSED_MAX_PHASE_CHARGE_DENSITY_UC_CM2)]));
            }
        }
    }

    // Cross-modality checks.
    let has = |t: &str| enabled.iter().any(|m| kind_of(m) == t);
    let (bes, tdcs, tms, clin) = (has("bes_tacs"), has("tdcs"), has("tms"), has("clinical_tacs"));
    if bes && tdcs {
        out.push(Warning, None, "cross_modality", "VALIDATE_PARAM_CROSS_MODALITY", s("BES + tDCS"), s("Separate electrode paths"), "hardware",
            msg("VALIDATE_MSG_GENERAL_CROSS_MODALITY", vec![]));
    }
    if tms && (bes || tdcs || clin) {
        out.push(Warning, None, "cross_modality_tms", "VALIDATE_PARAM_CROSS_MODALITY", s("TMS + electrical stim"), s("Sequential recommended"), "hardware",
            msg("VALIDATE_MSG_GENERAL_CROSS_MODALITY_TMS", vec![]));
    }

    for m in &enabled {
        let kind = kind_of(m);
        if let (Some(zones), "pbm_transcranial") = (ctx.zones, kind.as_str()) {
            zone_target(&mut out, &m["params"], zones);
        }
        modality(&kind, &m["params"], &m["interval"], duration, limits, &mut out);
    }
    // A configured limit is attributed to a tier now that the modality it belongs to is known.
    for i in &mut out.0 {
        if let Some(field) = i["limitSource"].as_str().and_then(|t| t.strip_prefix('@')).map(str::to_string) {
            let modality = i["modality"].as_str().unwrap_or("").to_string();
            i["limitSource"] = json!(dosage_source(ctx, limits, &modality, &field));
        }
    }
    out.0
}

/// Validate one entry against the resolved limits. `all_protocols` is the library a composite's layers
/// resolve against; `None` skips layer-reference checks, as the web does when it has no library to offer.
pub fn validate_entry(entry: &Value, limits: &Value, all_protocols: Option<&[Value]>, ctx: &Context) -> Vec<Value> {
    use Sev::Error;
    if entry["kind"] == "single" {
        return protocol(&entry["protocol"], limits, ctx);
    }
    if entry["kind"] != "composite" {
        return Vec::new();
    }
    let c = &entry["composite"];
    let mut out = Issues(Vec::new());
    let layers = c["layers"].as_array().cloned().unwrap_or_default();
    if layers.is_empty() {
        out.push(Error, None, "layers", "VALIDATE_PARAM_LAYERS", s("0"), s("≥1"), "hardware", msg("VALIDATE_MSG_GENERAL_LAYERS", vec![]));
    }
    for layer in &layers {
        let name = layer["protocolName"].as_str().unwrap_or("");
        let found = all_protocols.and_then(|all| all.iter().find(|p| p["kind"] == "single" && p["protocol"]["name"].as_str() == Some(name)));
        match (found, all_protocols) {
            (None, Some(_)) => out.push(Error, None, "layer_ref", "VALIDATE_PARAM_LAYER_REFERENCE", s(name), s("Known protocol"), "hardware",
                msg("VALIDATE_MSG_GENERAL_LAYER_REF", vec![s(name)])),
            (Some(p), _) => {
                for mut i in protocol(&p["protocol"], limits, ctx) {
                    let inner = i["message"].take();
                    i["message"] = msg("VALIDATE_LAYER_PREFIX", vec![s(name), inner]);
                    out.0.push(i);
                }
            }
            (None, None) => {}
        }
        let scale = f(layer, "intensityScale");
        if scale < 0.0 || scale > 1.0 {
            out.push(Error, None, "layer_intensity_scale", "VALIDATE_PARAM_LAYER_INTENSITY_SCALE", s(num_string(scale)), s("0–1"), "hardware",
                msg("VALIDATE_MSG_GENERAL_LAYER_INTENSITY_SCALE", vec![s(name), n(scale)]));
        }
    }
    out.0
}
