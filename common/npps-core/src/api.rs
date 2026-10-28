//! The JSON surface every binding layer wraps (JNI, a C ABI, WASM): text in, text or bytes
//! out, errors as the message the reference runtime would give. A binding does marshalling and
//! nothing else, so there is exactly one place the contract lives.

use crate::compiler::{compile_protocol, CompileOptions};
use crate::identity;
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
        .map(|n| n.as_u64().and_then(|x| u32::try_from(x).ok()).ok_or_else(|| "a socket id must be a whole number".to_string()))
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

/// The zone namespace a request carries, resolved for the protocol. `zonesByVersion` (`{name:{"1":[…],"2":[…]}}`) is
/// used when the request names the target helmet's `hardwareVersion`, and a named zone with no list for it is refused
/// (`SWR-HV-05`). The version is the helmet's, an input of the call; a protocol states none;
/// otherwise `zones` (`{name:[…]}`, already for one layout) is used as it always was.
fn request_zones(req: &Value) -> Result<Option<HashMap<String, Vec<u32>>>, String> {
    if let (Some(by_v), Some(v)) = (req["zonesByVersion"].as_object(), req["hardwareVersion"].as_u64()) {
        let mut all: HashMap<String, HashMap<u32, Vec<u32>>> = HashMap::new();
        for (name, lists) in by_v {
            let mut one = HashMap::new();
            for (ver, ids) in lists.as_object().ok_or("zonesByVersion entries must be objects")? {
                let ver: u32 = ver.parse().map_err(|_| format!("zonesByVersion[{name}]: '{ver}' is not a hardware version"))?;
                one.insert(ver, sockets(ids)?);
            }
            all.insert(name.clone(), one);
        }
        if let Some(z) = identity::zones_without_version_list(&req["def"], v as u32, &all).first() {
            let name = req["def"]["name"].as_str().unwrap_or("");
            return Err(identity::zone_missing_message(name, v, z));
        }
        let v = u32::try_from(v).map_err(|_| "hardwareVersion does not fit in 32 bits".to_string())?;
        return Ok(Some(identity::zones_for_version(&all, v)));
    }
    if req["zones"].is_null() {
        return Ok(None);
    }
    let mut m = HashMap::new();
    for (k, v) in req["zones"].as_object().ok_or("zones must be an object")? {
        m.insert(k.clone(), sockets(v)?);
    }
    Ok(Some(m))
}

/// Compile a protocol. `request` is
/// `{"def":{timingMode,modalities}, "zones":{name:[socket…]}, "clinicianSockets":[…]|null,
///   "deviceSerialHex":"…"|null, "nowUnix":n, "sessionUuidHex":"…32 hex…",
///   "wavelengthRules":{name,channels:[{element,nominalNm,minNm,maxNm}]}|null,
///   "autonomous":true|false (optional, default false: sets the Mode 3 header flag),
///   "hardwareVersion":n, "zonesByVersion":{name:{"1":[…],…}} (optional, together: the target helmet's hardware version, and
///   the zone lists per version, used instead of "zones"; a named zone with no list for that version is refused, SWR-HV-05).
///   "hardwareVersion" is also the version the descriptor is stamped with (SWR-HV-08, 1 to 15, 1 when absent)}`.
/// Returns the descriptor with a zeroed 64-byte signature slot, which the caller signs.
/// The error is the compiler's refusal message.
pub fn compile_json(request: &str) -> Result<Vec<u8>, String> {
    let req: Value = serde_json::from_str(request).map_err(|e| format!("compile request is not JSON: {e}"))?;
    let zones = request_zones(&req)?;
    let clinician = if req["clinicianSockets"].is_null() { None } else { Some(sockets(&req["clinicianSockets"])?) };
    let serial = match req["deviceSerialHex"].as_str() {
        Some(h) => Some(hex_decode(h)?),
        None => None,
    };
    let uuid_bytes = hex_decode(req["sessionUuidHex"].as_str().ok_or("sessionUuidHex is required")?)?;
    let session_uuid: [u8; 16] = uuid_bytes.try_into().map_err(|_| "sessionUuidHex must be 16 bytes".to_string())?;
    let wl = rules(&req["wavelengthRules"])?;
    let acknowledged: Vec<String> = req["acknowledgedCautions"]
        .as_array()
        .map(|a| a.iter().filter_map(|v| v.as_str().map(str::to_string)).collect())
        .unwrap_or_default();
    // The version of the helmet the descriptor is for (SWR-HV-08). 1 when the request names none, the same default as the
    // hub and the apps (SWR-HV-07, removed with it); one that is not a whole number in 1 to 15 is refused by the compiler.
    let hardware_version = match req["hardwareVersion"].as_u64() {
        None if req["hardwareVersion"].is_null() => 1,
        Some(v) => u8::try_from(v).unwrap_or(0),
        None => 0,
    };
    let opts = CompileOptions {
        device_serial: serial.as_deref(),
        zones: zones.as_ref(),
        clinician_sockets: clinician.as_deref(),
        wavelength_rules: wl.as_ref(),
        autonomous: req["autonomous"].as_bool().unwrap_or(false),
        now_unix: req["nowUnix"].as_u64().and_then(|x| u32::try_from(x).ok()).ok_or("nowUnix is required and must fit in 32 bits")?,
        session_uuid,
        hardware_version,
        acknowledged_cautions: &acknowledged,
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
                        if !zones.iter().any(|x| x["name"].as_str() == Some(z)) {
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
/// `{"issues":[…], "isValid":bool, "hasWarnings":bool, "zone":"safe"|"caution"|"danger", "zones":[…]}`; an issue is
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
    // The zone model's reading of a single protocol: its worst zone, and every axis behind it.
    let blocks = if req["entry"]["kind"] == "single" { crate::zones::evaluate(&req["entry"]["protocol"]) } else { Vec::new() };
    Ok(json!({
        "issues": issues, "isValid": is_valid, "hasWarnings": has_warnings,
        "zone": crate::zones::protocol_zone(&blocks).name(), "zones": crate::zones::to_json(&blocks),
    })
    .to_string())
}

/// Resolve the three limit tiers into the effective limits and where each value came from. `request` is
/// `{"global":<limits set>|null, "helmet":…|null, "individual":…|null}`; returns `{"limits":{level, <modality blocks>},
/// "sources":{<modalityProperty>:{<limitField>:"individual"|"helmet"|"global"}}}`. `sources` is what `validate_json`
/// takes as `limitSources`. The caller supplies the resolved set's id, name and timestamps.
pub fn resolve_limits_json(request: &str) -> Result<String, String> {
    let req: Value = serde_json::from_str(request).map_err(|e| format!("resolve request is not JSON: {e}"))?;
    let (limits, sources) = crate::resolve::resolve_limits(&req["global"], &req["helmet"], &req["individual"]);
    Ok(json!({ "limits": limits, "sources": sources }).to_string())
}

/// A protocol's identity: the hash of its compiled content (`SWR-HV-01`; the form and what it leaves out are in
/// `identity`). `request` is `{"def":…, "zones" | "hardwareVersion" + "zonesByVersion", "clinicianSockets":[…]|null}`, the same zone and socket
/// inputs as `compile_json`; the serial, rules, Mode 3 flag, cautions, time and session id are not inputs. Returns 64
/// lowercase hex digits.
pub fn content_hash_json(request: &str) -> Result<String, String> {
    let req: Value = serde_json::from_str(request).map_err(|e| format!("content hash request is not JSON: {e}"))?;
    let zones = request_zones(&req)?.unwrap_or_default();
    let clinician = if req["clinicianSockets"].is_null() { None } else { Some(sockets(&req["clinicianSockets"])?) };
    identity::content_hash(&req["def"], req["hardwareVersion"].as_u64().and_then(|v| u32::try_from(v).ok()), &zones, clinician.as_deref())
}

/// The protocol library's rules (`SWR-PL-02/03/06`, `SWR-UP-02`, `SWR-WO-01/02/03`, `SWR-FM-03…07`, `SWR-HV-04`): one entry
/// point for every binding, `{"op": …}` in and JSON out. The operations and their shapes are in `library`.
pub fn library_json(request: &str) -> Result<String, String> {
    crate::library::library_json(request)
}

/// Device-replacement migration and UHDR backup rules (`SWR-MG-01…10`, `SWR-MG-12`): one entry point for every binding,
/// `{"op": …}` in and JSON out. The operations and their shapes are in `migration`.
pub fn migration_json(request: &str) -> Result<String, String> {
    crate::migration::migration_json(request)
}
