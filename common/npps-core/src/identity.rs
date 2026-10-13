//! A protocol's identity (`SWR-HV-01`, `OI-UI-KMP-10` H1): the hash of its compiled content.
//!
//! **What the hash covers.** Everything the compiler reads to decide what the helmet delivers, and where: the
//! hardware version, the timing, and for each enabled modality its type, its parameters (the wavelength *as the
//! protocol requests it*, never as a rules table maps it) and its interval, with a named or clinician-selected PBM
//! zone replaced by the sockets it resolves to for the protocol's hardware version.
//!
//! **What it leaves out, because the compiler's inputs are not in the canonical form at all:** the device serial,
//! the wavelength rules and their version, the Mode 3 flag, the acknowledged cautions, the session id and the
//! compile time. The family id, the used flag, the name, id, description, author, tags, release version, references
//! and conditions are metadata the compiler never reads, so they are not in it either. `SWR-WO-03`: the used flag
//! cannot change a hash. `SWR-FM-01`: nor can the family id.
//!
//! **The canonical form** is JSON with every object's keys in sorted order (`serde_json`'s map is a `BTreeMap`,
//! pinned by a test), no whitespace, numbers as `serde_json` writes them, a PBM zone block as `sockets`: the sorted,
//! de-duplicated socket ids, and the disabled modalities dropped. It is prefixed by `DOMAIN` so a hash of this form
//! cannot be mistaken for a hash of any other object, and the digest is SHA-256, written as 64 lowercase hex
//! digits. Every platform reaches this through the core (`CLAUDE.md` §20), so there is one serializer.
//!
//! **It is not a validity check.** A hash is given to a protocol the compiler would refuse, because the hash
//! says which protocol it is, and acknowledging a caution or loading a rules table must not change that. The one
//! refusal it shares with the compiler is `SWR-HV-05`: a zone with no socket list for the protocol's hardware
//! version has no sockets to hash.

use serde_json::{json, Map, Value};
use sha2::{Digest, Sha256};
use std::collections::{BTreeSet, HashMap};

/// Prefix of the hashed bytes. Changing the canonical form changes this string, so two forms never share a hash.
pub const DOMAIN: &str = "NeurOne.ProtocolContent.1\n";

type R<T> = Result<T, String>;

/// The sockets of every zone that has a list for `version` (`SWR-HV-05`), from the form the namespace holds
/// (`socketsByVersion`: `{ "1": [..], "2": [..] }`). A zone with no list for `version` is simply absent, so a
/// protocol naming it fails to resolve with `zone_missing_message`.
pub fn zones_for_version(zones_by_version: &HashMap<String, HashMap<u32, Vec<u32>>>, version: u32) -> HashMap<String, Vec<u32>> {
    zones_by_version.iter().filter_map(|(name, by_v)| by_v.get(&version).map(|s| (name.clone(), s.clone()))).collect()
}

/// The refusal for a named zone the protocol's hardware version has no list for (same words as the namespace check).
pub fn zone_missing_message(protocol: &str, version: u64, zone: &str) -> String {
    format!("Protocol '{protocol}' is for hardware version {version}, and zone '{zone}' has no socket list for it")
}

/// Every named zone an enabled PBM block of `def` refers to that `zones_by_version` has no list for at the
/// protocol's hardware version. Empty when the protocol states no hardware version (it names none to resolve
/// against) or when every zone resolves. A zone that is not in the namespace at all is the compiler's own error.
pub fn zones_without_version_list(def: &Value, zones_by_version: &HashMap<String, HashMap<u32, Vec<u32>>>) -> Vec<String> {
    let Some(v) = def["hardwareVersion"].as_u64().and_then(|v| u32::try_from(v).ok()) else { return Vec::new() };
    let mut missing = Vec::new();
    for m in def["modalities"].as_array().into_iter().flatten() {
        if m["enabled"] == false || m["type"] != "pbm_transcranial" || m["params"]["zones"] != "named" {
            continue;
        }
        for z in m["params"]["zoneRefs"].as_array().into_iter().flatten().filter_map(Value::as_str) {
            if let Some(by_v) = zones_by_version.get(z) {
                if !by_v.contains_key(&v) && !missing.iter().any(|x| x == z) {
                    missing.push(z.to_string());
                }
            }
        }
    }
    missing
}

fn sorted_sockets(ids: impl IntoIterator<Item = u32>) -> Value {
    json!(ids.into_iter().collect::<BTreeSet<u32>>().into_iter().collect::<Vec<u32>>())
}

/// The canonical content of `def` (see the module note). `zones` is the namespace already resolved for the
/// protocol's hardware version; `clinician_sockets` is the operator's choice for a `clinician_selected` block.
pub fn canonical_content(def: &Value, zones: &HashMap<String, Vec<u32>>, clinician_sockets: Option<&[u32]>) -> R<Value> {
    let mut modalities = Vec::new();
    for m in def["modalities"].as_array().into_iter().flatten() {
        if m["enabled"] == false {
            continue;
        }
        let mut params = m["params"].as_object().cloned().unwrap_or_default();
        if m["type"] == "pbm_transcranial" {
            let sockets = match params.get("zones").and_then(Value::as_str) {
                Some("named") => {
                    let mut all: Vec<u32> = Vec::new();
                    for r in params.get("zoneRefs").and_then(Value::as_array).into_iter().flatten().filter_map(Value::as_str) {
                        let Some(z) = zones.get(r) else {
                            return Err(format!("PBM transcranial references zone \"{r}\", which is not in the zone namespace."));
                        };
                        all.extend(z);
                    }
                    all
                }
                Some("clinician_selected") => match clinician_sockets {
                    Some(c) if !c.is_empty() => c.to_vec(),
                    _ => return Err("PBM transcranial target is 'clinician_selected': the operator must choose the sockets first.".into()),
                },
                other => return Err(format!("PBM transcranial has an unhandled zone target: {other:?}")),
            };
            params.remove("zones");
            params.remove("zoneRefs");
            params.insert("sockets".into(), sorted_sockets(sockets));
        }
        let mut one = Map::new();
        one.insert("type".into(), m["type"].clone());
        one.insert("params".into(), Value::Object(params));
        one.insert("interval".into(), m["interval"].clone());
        modalities.push(Value::Object(one));
    }
    Ok(json!({
        "hardwareVersion": def["hardwareVersion"],
        "timingMode": def["timingMode"],
        "modalities": modalities,
    }))
}

/// The bytes the hash is taken over: `DOMAIN` then the canonical JSON.
pub fn canonical_bytes(content: &Value) -> Vec<u8> {
    let mut b = DOMAIN.as_bytes().to_vec();
    b.extend_from_slice(content.to_string().as_bytes());
    b
}

/// The protocol's identity: SHA-256 of `canonical_bytes`, as 64 lowercase hex digits.
pub fn content_hash(def: &Value, zones: &HashMap<String, Vec<u32>>, clinician_sockets: Option<&[u32]>) -> R<String> {
    let content = canonical_content(def, zones, clinician_sockets)?;
    let digest = Sha256::digest(canonical_bytes(&content));
    Ok(digest.iter().map(|b| format!("{b:02x}")).collect())
}
