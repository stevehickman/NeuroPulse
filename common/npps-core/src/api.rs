//! The JSON surface every binding layer wraps (JNI, a C ABI, WASM): text in, text or bytes
//! out, errors as the message the reference runtime would give. A binding does marshalling and
//! nothing else, so there is exactly one place the contract lives.

use crate::compiler::{compile_protocol, CompileOptions};
use crate::parser::{parse_file, Entry};
use crate::wavelength::{Channel, ChannelRule, WavelengthRules};
use serde_json::{json, Value};
use std::collections::HashMap;

/// Parse NPPS text into everything the file declares, as JSON:
/// `{"entries":[{"kind":"single","protocol":{…}} | {"kind":"composite","composite":{…}}],
///   "zones":[…], "conditions":[…], "wavelengthRules":[…], "limits":[…]}`,
/// the content of `parseNPPSFile` and `parseNPPSLimits` in the web reference, in file order. Ids and
/// timestamps the reference generates per parse are left out: the caller that builds a model supplies
/// them. The error is the refusal message, `Line N: …` as the web parser writes it.
pub fn parse_json(source: &str) -> Result<String, String> {
    let f = parse_file(source).map_err(|e| e.to_string())?;
    let entries: Vec<Value> = f
        .entries
        .into_iter()
        .map(|e| match e {
            Entry::Single(p) => json!({ "kind": "single", "protocol": p }),
            Entry::Composite(c) => json!({ "kind": "composite", "composite": c }),
        })
        .collect();
    Ok(json!({
        "entries": entries,
        "zones": f.zones,
        "conditions": f.conditions,
        "wavelengthRules": f.wavelength_rules,
        "limits": f.limits,
    })
    .to_string())
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

/// Fold several parsed files into one namespace and check its cross-references: `buildNamespace` and
/// `validateNamespaceReferences` of the web reference. `request` is `{"files":[<a parse_json result>…]}`
/// in load order. Returns
/// `{"entries":[…], "zones":[…], "conditions":[…], "errors":[…], "referenceErrors":[…]}`:
/// the entries of every file; the zones and conditions that are defined exactly once; the duplicate-name
/// errors (a name two files define is left undefined, whatever the read order); and the protocols or
/// composites that name a zone or condition that is not defined.
pub fn namespace_json(request: &str) -> Result<String, String> {
    let req: Value = serde_json::from_str(request).map_err(|e| format!("namespace request is not JSON: {e}"))?;
    let files = req["files"].as_array().ok_or("namespace request needs a files array")?;

    let mut entries: Vec<Value> = Vec::new();
    let mut zones: Vec<Value> = Vec::new();
    let mut conditions: Vec<Value> = Vec::new();
    let mut errors: Vec<String> = Vec::new();
    // Names seen at least twice stay out, so a third definition cannot re-bind a name known to collide.
    let mut collided_zones: Vec<String> = Vec::new();
    let mut collided_conditions: Vec<String> = Vec::new();

    fn fold(
        defs: &[Value],
        kind: &str,
        into: &mut Vec<Value>,
        collided: &mut Vec<String>,
        errors: &mut Vec<String>,
    ) {
        for d in defs {
            let name = d["name"].as_str().unwrap_or("").to_string();
            if collided.contains(&name) {
                continue;
            }
            if let Some(i) = into.iter().position(|x| x["name"].as_str() == Some(name.as_str())) {
                errors.push(format!(
                    "Duplicate {kind} name '{name}' — defined in more than one file; {kind} names must be \
                 unique across the protocol directory. The name is left undefined."
                ));
                into.remove(i);
                collided.push(name);
                continue;
            }
            into.push(d.clone());
        }
    }

    for f in files {
        entries.extend(f["entries"].as_array().cloned().unwrap_or_default());
        fold(f["zones"].as_array().map(Vec::as_slice).unwrap_or(&[]), "zone", &mut zones, &mut collided_zones, &mut errors);
        fold(
            f["conditions"].as_array().map(Vec::as_slice).unwrap_or(&[]),
            "condition",
            &mut conditions,
            &mut collided_conditions,
            &mut errors,
        );
    }

    let defined = |set: &[Value], name: &str| set.iter().any(|x| x["name"].as_str() == Some(name));
    let mut reference_errors: Vec<String> = Vec::new();
    for e in &entries {
        let single = e["kind"] == "single";
        let def = if single { &e["protocol"] } else { &e["composite"] };
        let name = def["name"].as_str().unwrap_or("");
        for c in def["conditions"].as_array().into_iter().flatten() {
            let c = c.as_str().unwrap_or("");
            if !defined(&conditions, c) {
                reference_errors.push(format!("Protocol '{name}' references undefined condition '{c}'"));
            }
        }
        if single {
            for m in def["modalities"].as_array().into_iter().flatten() {
                if m["type"] == "pbm_transcranial" {
                    for z in m["params"]["zoneRefs"].as_array().into_iter().flatten() {
                        let z = z.as_str().unwrap_or("");
                        if !defined(&zones, z) {
                            reference_errors.push(format!("Protocol '{name}' references undefined zone '{z}'"));
                        }
                    }
                }
            }
        }
    }

    Ok(json!({
        "entries": entries,
        "zones": zones,
        "conditions": conditions,
        "errors": errors,
        "referenceErrors": reference_errors,
    })
    .to_string())
}

/// Write models as `.npps` text: `request` is `{"items":[<item>…]}`, each item as in
/// `serialize::serialize_item` (the shapes `parse_json` returns). Items are separated by a blank line, which is
/// what `serializeNPPS` wrote. The error is the refusal message; a model that cannot be written is a bug in
/// whatever built it, so it is refused rather than serialized into a file the parser rejects.
pub fn serialize_json(request: &str) -> Result<String, String> {
    let req: Value = serde_json::from_str(request).map_err(|e| format!("serialize request is not JSON: {e}"))?;
    let items = req["items"].as_array().ok_or("serialize request needs an items array")?;
    let texts = items.iter().map(crate::serialize::serialize_item).collect::<Result<Vec<_>, _>>()?;
    Ok(texts.join("\n\n"))
}

/// Validate an entry against the resolved limits (`validateEntry` of the web reference).
/// `request` is `{"entry":{"kind":"single","protocol":…}|{"kind":"composite","composite":…},
/// "limits":<resolved NPLimitsSet>, "allProtocols":[<entry>…]|null, "zones":{name:[socket…]}|null (optional: check
/// PBM targets against the namespace), "limitSources":{<modalityProperty>:{<limitField>:tier}}|null (optional)}`. Returns
/// `{"issues":[…], "isValid":bool, "hasWarnings":bool}`; an issue is
/// `{severity, modality?, parameterKey, parameterName, actualValueDescription, limitValueDescription,
/// limitSource, message}` where every text is a plain string or a `{key, args}` message for the caller to
/// localize (see `validate`). Ids are the caller's to mint.
pub fn validate_json(request: &str) -> Result<String, String> {
    let req: Value = serde_json::from_str(request).map_err(|e| format!("validate request is not JSON: {e}"))?;
    if req["entry"].is_null() {
        return Err("validate request needs an entry".into());
    }
    let all: Option<Vec<Value>> = req["allProtocols"].as_array().cloned();
    let limits = if req["limits"].is_null() { json!({}) } else { req["limits"].clone() };
    let ctx = crate::validate::Context {
        zones: Some(&req["zones"]).filter(|z| z.is_object()),
        limit_sources: Some(&req["limitSources"]).filter(|z| z.is_object()),
    };
    let issues = crate::validate::validate_entry(&req["entry"], &limits, all.as_deref(), &ctx);
    let is_valid = !issues.iter().any(|i| i["severity"] == "error");
    let has_warnings = issues.iter().any(|i| i["severity"] == "warning");
    Ok(json!({ "issues": issues, "isValid": is_valid, "hasWarnings": has_warnings }).to_string())
}
