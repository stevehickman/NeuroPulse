//! The JSON surface every binding layer wraps (JNI, a C ABI, WASM): text in, text or bytes
//! out, errors as the message the reference runtime would give. A binding does marshalling and
//! nothing else, so there is exactly one place the contract lives.

use crate::compiler::{compile_protocol, CompileOptions};
use crate::parser::{parse_npps, Entry};
use crate::wavelength::{Channel, ChannelRule, WavelengthRules};
use serde_json::{json, Value};
use std::collections::HashMap;

/// Parse NPPS text into a JSON array of entries: `{"kind":"single","protocol":{…}}` for a
/// `protocol`, `{"kind":"skipped","what":"zone"}` for a block v0 does not interpret. The error
/// is the refusal message, `Line N: …` as the web parser writes it.
pub fn parse_json(source: &str) -> Result<String, String> {
    let entries = parse_npps(source).map_err(|e| e.to_string())?;
    let out: Vec<Value> = entries
        .into_iter()
        .map(|e| match e {
            Entry::Single(p) => json!({ "kind": "single", "protocol": p }),
            Entry::Skipped(what) => json!({ "kind": "skipped", "what": what }),
        })
        .collect();
    Ok(Value::Array(out).to_string())
}

fn hex_decode(s: &str) -> Result<Vec<u8>, String> {
    if s.len() % 2 != 0 || !s.is_ascii() {
        return Err(format!("not hex: {s}"));
    }
    (0..s.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(&s[i..i + 2], 16).map_err(|_| format!("not hex: {s}")))
        .collect()
}

fn sockets(v: &Value) -> Result<Vec<u32>, String> {
    v.as_array()
        .ok_or("sockets must be an array")?
        .iter()
        .map(|n| n.as_u64().map(|x| x as u32).ok_or_else(|| "a socket id must be a whole number".to_string()))
        .collect()
}

fn rules(v: &Value) -> Result<Option<WavelengthRules>, String> {
    if v.is_null() {
        return Ok(None);
    }
    let name = v["name"].as_str().unwrap_or("user wavelength rules").to_string();
    let mut channels = Vec::new();
    for c in v["channels"].as_array().ok_or("wavelengthRules.channels must be an array")? {
        let element = match c["element"].as_str() {
            Some("led_660") => Channel::Led660,
            Some("led_808") => Channel::Led808,
            Some("led_1064") => Channel::Led1064,
            other => return Err(format!("unknown channel {other:?}")),
        };
        let n = |k: &str| c[k].as_f64().ok_or_else(|| format!("wavelengthRules {k} must be a number"));
        channels.push(ChannelRule { element, nominal_nm: n("nominalNm")?, min_nm: n("minNm")?, max_nm: n("maxNm")? });
    }
    Ok(Some(WavelengthRules { name, channels }))
}

/// Compile a protocol. `request` is
/// `{"def":{timingMode,modalities}, "zones":{name:[socket…]}, "clinicianSockets":[…]|null,
///   "deviceSerialHex":"…"|null, "nowUnix":n, "sessionUuidHex":"…32 hex…",
///   "wavelengthRules":{name,channels:[{element,nominalNm,minNm,maxNm}]}|null,
///   "autonomous":true|false (optional, default false: sets the Mode 3 header flag)}`.
/// Returns the descriptor with a zeroed 64-byte signature slot, which the caller signs.
/// The error is the compiler's refusal message.
pub fn compile_json(request: &str) -> Result<Vec<u8>, String> {
    let req: Value = serde_json::from_str(request).map_err(|e| format!("compile request is not JSON: {e}"))?;
    let zones: Option<HashMap<String, Vec<u32>>> = if req["zones"].is_null() {
        None
    } else {
        let mut m = HashMap::new();
        for (k, v) in req["zones"].as_object().ok_or("zones must be an object")? {
            m.insert(k.clone(), sockets(v)?);
        }
        Some(m)
    };
    let clinician = if req["clinicianSockets"].is_null() { None } else { Some(sockets(&req["clinicianSockets"])?) };
    let serial = match req["deviceSerialHex"].as_str() {
        Some(h) => Some(hex_decode(h)?),
        None => None,
    };
    let uuid_bytes = hex_decode(req["sessionUuidHex"].as_str().ok_or("sessionUuidHex is required")?)?;
    let session_uuid: [u8; 16] = uuid_bytes.try_into().map_err(|_| "sessionUuidHex must be 16 bytes".to_string())?;
    let wl = rules(&req["wavelengthRules"])?;
    let opts = CompileOptions {
        device_serial: serial.as_deref(),
        zones: zones.as_ref(),
        clinician_sockets: clinician.as_deref(),
        wavelength_rules: wl.as_ref(),
        autonomous: req["autonomous"].as_bool().unwrap_or(false),
        now_unix: req["nowUnix"].as_u64().ok_or("nowUnix is required")? as u32,
        session_uuid,
    };
    compile_protocol(&req["def"], &opts).map(|c| c.blob)
}
