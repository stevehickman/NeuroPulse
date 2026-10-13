//! The protocol library's rules, once, for every app (`SWR-PL-02/03/06`, `SWR-UP-02`, `SWR-WO-01/02/03`,
//! `SWR-FM-03…07`, `SWR-HV-04`; issue #569 package 4, `CLAUDE.md` §20).
//!
//! The library is storage and screens on each platform. What it may and may not do is decided here, so the five
//! platforms cannot disagree: whether a save is allowed, what a clone is, what an upload outcome marks, which refusal an
//! upload meets. Every operation is a pure function of the JSON it is given; none reads or writes a store, and none
//! produces text for a person (a refusal is a reason code, and the screen words it from the locale files, `CLAUDE.md` §17).
//!
//! # Entries
//! A library entry is summarised as `{"id", "familyId"|null, "used", "version"?, "scope"?, "hardwareVersions"?}`. A
//! protocol with no family id is a family of one, keyed by its own id. A family is used when any variant of it is
//! (`SWR-FM-03`): the flag the library stores is one bit per family (`SWR-WO-02`), which `used_record` shapes.
//!
//! # What a protocol's hardware version is
//! A protocol states none (`NP-NPPS-REF-001` Rev 30). The versions it supports are the layouts in the layout table
//! on which every zone it names has a socket list and every socket the operator chose exists (`supported_versions`).
//! With one layout the table has one entry and every protocol supports it.

use crate::identity;
use serde_json::{json, Map, Value};
use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::sync::OnceLock;

type R<T> = Result<T, String>;

/// `hardware/np_socket_map.json` is the layout of hardware version 1. A later version adds its own file and a line here;
/// the core is the only reader, and every app asks it (`layouts`), so there is one copy.
const LAYOUT_FILES: [(u32, &str); 1] = [(1, include_str!("../../../hardware/np_socket_map.json"))];

fn layouts_table() -> &'static BTreeMap<u32, Vec<Value>> {
    static T: OnceLock<BTreeMap<u32, Vec<Value>>> = OnceLock::new();
    T.get_or_init(|| {
        LAYOUT_FILES
            .iter()
            .map(|(v, text)| {
                let doc: Value = serde_json::from_str(text).expect("the layout file is JSON");
                let sockets = doc["sockets"]
                    .as_array()
                    .expect("a layout file has sockets")
                    .iter()
                    .map(|s| json!({ "id": s["id"], "xMm": s["xMm"], "yMm": s["yMm"], "zMm": s["zMm"] }))
                    .collect();
                (*v, sockets)
            })
            .collect()
    })
}

fn layout_ids(version: u32) -> Option<BTreeSet<u64>> {
    layouts_table().get(&version).map(|s| s.iter().filter_map(|x| x["id"].as_u64()).collect())
}

// ── Entries and families ────────────────────────────────────────────────────────────────────────

fn entries(req: &Value) -> R<&Vec<Value>> {
    req["entries"].as_array().ok_or_else(|| "entries must be an array".to_string())
}

fn id_of(e: &Value) -> &str {
    e["id"].as_str().unwrap_or("")
}

/// The key of the family an entry belongs to: its family id, or its own id when it has none.
fn family_of(e: &Value) -> &str {
    e["familyId"].as_str().filter(|s| !s.is_empty()).unwrap_or_else(|| id_of(e))
}

fn members<'a>(es: &'a [Value], key: &str) -> Vec<&'a Value> {
    es.iter().filter(|e| family_of(e) == key).collect()
}

fn family_used(es: &[Value], key: &str) -> bool {
    members(es, key).iter().any(|e| e["used"] == true)
}

fn find<'a>(es: &'a [Value], id: &str) -> Option<&'a Value> {
    es.iter().find(|e| id_of(e) == id)
}

fn versions_of(e: &Value) -> Vec<u64> {
    e["hardwareVersions"].as_array().into_iter().flatten().filter_map(Value::as_u64).collect()
}

/// `major.minor` as a sortable pair; anything unreadable sorts lowest.
fn version_key(e: &Value) -> (u64, u64) {
    let s = e["version"].as_str().unwrap_or("");
    let mut it = s.split('.');
    let a = it.next().and_then(|x| x.parse().ok()).unwrap_or(0);
    let b = it.next().and_then(|x| x.parse().ok()).unwrap_or(0);
    (a, b)
}

// ── Operations ──────────────────────────────────────────────────────────────────────────────────

/// `SWR-PL-02/03`, `SWR-FM-03`: a save to an entry of a used family is refused and offers a clone of the whole family;
/// a save to an unused one, or to an entry the library does not hold yet, is allowed.
fn save_check(req: &Value) -> R<Value> {
    let es = entries(req)?;
    let id = req["id"].as_str().ok_or("id is required")?;
    let Some(e) = find(es, id) else { return Ok(json!({ "allowed": true })) };
    let key = family_of(e);
    if family_used(es, key) {
        let ids: Vec<&str> = members(es, key).iter().map(|m| id_of(m)).collect();
        return Ok(json!({ "allowed": false, "reason": "family_used", "offer": "clone_family", "familyId": key, "members": ids }));
    }
    Ok(json!({ "allowed": true }))
}

/// `SWR-FM-05`: a family with a used variant cannot be deleted and can be archived. An unused one can be deleted.
fn delete_check(req: &Value) -> R<Value> {
    let es = entries(req)?;
    let id = req["id"].as_str().ok_or("id is required")?;
    let Some(e) = find(es, id) else { return Ok(json!({ "allowed": true })) };
    if family_used(es, family_of(e)) {
        return Ok(json!({ "allowed": false, "reason": "family_used", "alternative": "archive" }));
    }
    Ok(json!({ "allowed": true }))
}

/// `SWR-FM-03`: the clone of a family is a new family from the moment it is made. It holds a copy of every variant
/// (the caller copies the definitions and gives each the new id and the new family id), none of them used, and records the
/// family it came from for provenance only. `newIds` must supply one id per member, in the order returned.
fn clone_family(req: &Value) -> R<Value> {
    let es = entries(req)?;
    let id = req["id"].as_str().ok_or("id is required")?;
    let e = find(es, id).ok_or_else(|| format!("no entry '{id}'"))?;
    let key = family_of(e);
    let new_family = req["newFamilyId"].as_str().filter(|s| !s.is_empty()).ok_or("newFamilyId is required")?;
    if new_family == key {
        return Err("the clone's family id must be new".into());
    }
    let new_ids: Vec<&str> = req["newIds"].as_array().ok_or("newIds must be an array")?.iter().filter_map(Value::as_str).collect();
    let ms = members(es, key);
    if new_ids.len() != ms.len() {
        return Err(format!("the family has {} variant(s) and {} new id(s) were given", ms.len(), new_ids.len()));
    }
    let mut seen = BTreeSet::new();
    for n in &new_ids {
        if find(es, n).is_some() || !seen.insert(*n) {
            return Err(format!("new id '{n}' is not unique"));
        }
    }
    let clones: Vec<Value> = ms
        .iter()
        .zip(new_ids)
        .map(|(m, n)| json!({ "sourceId": id_of(m), "newId": n, "familyId": new_family, "used": false, "clonedFromFamily": key }))
        .collect();
    Ok(json!({ "clones": clones }))
}

/// `SWR-FM-06`: a variant may be added to a used family, only for a hardware version it has no variant for.
fn add_variant_check(req: &Value) -> R<Value> {
    let es = entries(req)?;
    let key = req["familyId"].as_str().ok_or("familyId is required")?;
    let v = req["hardwareVersion"].as_u64().ok_or("hardwareVersion is required")?;
    let ms = members(es, key);
    if ms.is_empty() {
        return Err(format!("no family '{key}'"));
    }
    if ms.iter().any(|m| versions_of(m).contains(&v)) {
        return Ok(json!({ "allowed": false, "reason": "variant_exists" }));
    }
    // Immutable from creation when the family is used: the screen shows this before the save.
    Ok(json!({ "allowed": true, "immutableFromCreation": family_used(es, key) }))
}

/// `SWR-HV-04`: the variants of a family for the helmet's hardware version, latest first. None: the person decides
/// whether to create one.
fn variant_for(req: &Value) -> R<Value> {
    let es = entries(req)?;
    let key = req["familyId"].as_str().ok_or("familyId is required")?;
    let v = req["helmetVersion"].as_u64().ok_or("helmetVersion is required")?;
    let mut hits: Vec<&Value> = members(es, key).into_iter().filter(|m| versions_of(m).contains(&v)).collect();
    hits.sort_by(|a, b| version_key(b).cmp(&version_key(a)));
    let ids: Vec<&str> = hits.iter().map(|m| id_of(m)).collect();
    Ok(json!({ "variants": ids, "offerCreate": ids.is_empty() }))
}

/// The layouts a protocol can be sent to: every layout in the table on which each named zone has a socket list and
/// each socket the operator chose exists.
fn supported_versions(req: &Value) -> R<Value> {
    let by_v = zones_by_version(&req["zonesByVersion"])?;
    let chosen: Vec<u64> = req["clinicianSockets"].as_array().into_iter().flatten().filter_map(Value::as_u64).collect();
    let mut out = Vec::new();
    for v in layouts_table().keys() {
        let ids = layout_ids(*v).unwrap_or_default();
        if identity::zones_without_version_list(&req["def"], *v, &by_v).is_empty() && chosen.iter().all(|s| ids.contains(s)) {
            out.push(*v);
        }
    }
    Ok(json!({ "versions": out }))
}

fn zones_by_version(v: &Value) -> R<HashMap<String, HashMap<u32, Vec<u32>>>> {
    let mut all = HashMap::new();
    let Some(obj) = v.as_object() else { return Ok(all) };
    for (name, lists) in obj {
        let mut one = HashMap::new();
        for (ver, ids) in lists.as_object().ok_or("zonesByVersion entries must be objects")? {
            let ver: u32 = ver.parse().map_err(|_| format!("zonesByVersion[{name}]: '{ver}' is not a hardware version"))?;
            let ids = ids.as_array().ok_or("a socket list must be an array")?.iter().filter_map(|n| n.as_u64().and_then(|x| u32::try_from(x).ok())).collect();
            one.insert(ver, ids);
        }
        all.insert(name.clone(), one);
    }
    Ok(all)
}

/// `SWR-HV-04` (H4a): how two layouts overlap. A socket id in both is `both`; one only the first has is `onlyA`. The
/// screen draws both with the positions `layouts` gives.
fn layout_diff(req: &Value) -> R<Value> {
    let a = req["a"].as_u64().and_then(|x| u32::try_from(x).ok()).ok_or("a is required")?;
    let b = req["b"].as_u64().and_then(|x| u32::try_from(x).ok()).ok_or("b is required")?;
    let ia = layout_ids(a).ok_or_else(|| format!("no layout for hardware version {a}"))?;
    let ib = layout_ids(b).ok_or_else(|| format!("no layout for hardware version {b}"))?;
    Ok(json!({
        "both": ia.intersection(&ib).collect::<Vec<_>>(),
        "onlyA": ia.difference(&ib).collect::<Vec<_>>(),
        "onlyB": ib.difference(&ia).collect::<Vec<_>>(),
    }))
}

fn layouts(_: &Value) -> R<Value> {
    let m: Map<String, Value> = layouts_table().iter().map(|(v, s)| (v.to_string(), json!({ "sockets": s }))).collect();
    Ok(Value::Object(m))
}

/// What an upload meets before any byte is sent. Hardware version first (the protocol cannot run on this helmet at all),
/// then the unmodified clone (`SWR-PL-06`). Each refusal is a local decision: nothing is sent and nothing is marked used.
/// `protocolVersions` is `supported_versions`' answer; `sourceHash` is the clone's source's, null when the protocol is not a clone.
fn upload_gate(req: &Value) -> R<Value> {
    let helmet = req["helmetVersion"].as_u64().ok_or("helmetVersion is required")?;
    let versions: Vec<u64> = req["protocolVersions"].as_array().ok_or("protocolVersions must be an array")?.iter().filter_map(Value::as_u64).collect();
    if !versions.contains(&helmet) {
        let diffs: Vec<Value> = versions
            .iter()
            .map(|pv| {
                let d = layout_diff(&json!({ "a": pv, "b": helmet })).unwrap_or(Value::Null);
                json!({ "protocolVersion": pv, "diff": d })
            })
            .collect();
        return Ok(json!({ "decision": "refuse", "reason": "hardware_mismatch", "helmetVersion": helmet,
                          "protocolVersions": versions, "layoutDiffs": diffs, "markUsed": false }));
    }
    let hash = req["contentHash"].as_str().ok_or("contentHash is required")?;
    if req["sourceHash"].as_str() == Some(hash) {
        return Ok(json!({ "decision": "refuse", "reason": "unmodified_clone",
                          "outcomes": ["upload_source", "edit_clone", "cancel"], "markUsed": false }));
    }
    Ok(json!({ "decision": "allow" }))
}

/// `SWR-UP-02`: the three outcomes of an upload that was sent, and what each marks. `helmetStatus` is the hub's
/// `UPLOAD_CHECK` verdict: `accepted`, `refused`, or absent when it never answered. A refusal the helmet gives for a
/// hardware version, an unmodified clone or a tier is not a failed upload and marks nothing (`helmetRefusal`).
fn upload_result(req: &Value) -> R<Value> {
    if req["refusedBeforeSend"] == true {
        return Ok(json!({ "outcome": "not_sent", "markUsed": false }));
    }
    let sent = req["sentHash"].as_str().ok_or("sentHash is required")?;
    let status = req["helmetStatus"].as_str();
    let hash = req["helmetHash"].as_str();
    if status == Some("refused") {
        if let Some(why) = req["helmetRefusal"].as_str() {
            if matches!(why, "hardware_version" | "unmodified_clone" | "tier") {
                return Ok(json!({ "outcome": "not_a_failed_upload", "markUsed": false, "reason": why }));
            }
        }
        return Ok(json!({ "outcome": "confirmed_failed", "markUsed": false }));
    }
    match (status, hash) {
        (Some("accepted"), Some(h)) if h == sent => Ok(json!({ "outcome": "confirmed", "markUsed": true })),
        (_, Some(h)) if h != sent => Ok(json!({ "outcome": "confirmed_failed", "markUsed": false })),
        // No reply, a dead link, a timeout, a verdict still pending: the helmet may hold it.
        _ => Ok(json!({ "outcome": "unknown", "markUsed": true })),
    }
}

/// `SWR-WO-03`: an import merges the used flag by OR. A protocol that arrives used is used; one that arrives unused but
/// whose content hash is already used locally stays used; an import never clears the flag.
fn merge_import(req: &Value) -> R<Value> {
    let local: BTreeSet<&str> = req["usedHashes"].as_array().into_iter().flatten().filter_map(Value::as_str).collect();
    let incoming = req["incoming"].as_array().ok_or("incoming must be an array")?;
    let out: Vec<Value> = incoming
        .iter()
        .map(|e| {
            let hash_used = e["hash"].as_str().is_some_and(|h| local.contains(h));
            json!({ "id": id_of(e), "used": e["used"] == true || hash_used })
        })
        .collect();
    Ok(json!({ "entries": out }))
}

/// `SWR-WO-01`, `SWR-FM-05`: the set offered for use is the owner's, and archiving a family removes it from the offer
/// without losing a variant.
fn offered(req: &Value) -> R<Value> {
    let es = entries(req)?;
    let archived: BTreeSet<&str> = req["archivedFamilies"].as_array().into_iter().flatten().filter_map(Value::as_str).collect();
    let ids: Vec<&str> = es.iter().filter(|e| !archived.contains(family_of(e))).map(id_of).collect();
    Ok(json!({ "offered": ids }))
}

/// `SWR-WO-02`: the stored used flag is the family id and the bit, nothing else: no device, user, clinic, place, count
/// or time. Presence of the record is the bit, and it is never cleared. `valid` checks a stored record has no other field.
fn used_record(req: &Value) -> R<Value> {
    if let Some(rec) = req.get("check") {
        let ok = rec.as_object().is_some_and(|o| o.len() == 1 && o.get("familyId").is_some_and(Value::is_string));
        return Ok(json!({ "valid": ok }));
    }
    let key = req["familyId"].as_str().filter(|s| !s.is_empty()).ok_or("familyId is required")?;
    Ok(json!({ "record": { "familyId": key } }))
}

/// The keys that belong to a variant, not to the family's intent: where the light goes on a given layout
/// (`SWR-FM-04`). Everything else in a definition is intent: frequency, duty, irradiance, wavelength, durations, ordering.
const VARIANT_SPECIFIC: [&str; 4] = ["zones", "zoneRefs", "sockets", "customSockets"];

fn diff_into(path: &mut Vec<Value>, old: &Value, new: &Value, out: &mut Vec<Value>) {
    match (old, new) {
        (Value::Object(a), Value::Object(b)) => {
            let keys: BTreeSet<&String> = a.keys().chain(b.keys()).collect();
            for k in keys {
                if VARIANT_SPECIFIC.contains(&k.as_str()) {
                    continue;
                }
                path.push(json!(k));
                diff_into(path, a.get(k).unwrap_or(&Value::Null), b.get(k).unwrap_or(&Value::Null), out);
                path.pop();
            }
        }
        (Value::Array(a), Value::Array(b)) if a.len() == b.len() => {
            for (i, (x, y)) in a.iter().zip(b).enumerate() {
                // Modalities are matched by position, and only when the type is the same there.
                if x["type"].is_string() && x["type"] != y["type"] {
                    continue;
                }
                path.push(json!(i));
                diff_into(path, x, y, out);
                path.pop();
            }
        }
        _ => {
            if old != new {
                out.push(json!({ "path": path.clone(), "from": old, "to": new }));
            }
        }
    }
}

fn get_path<'a>(v: &'a Value, path: &[Value]) -> Option<&'a Value> {
    let mut cur = v;
    for p in path {
        cur = match p {
            Value::String(k) => cur.get(k)?,
            Value::Number(n) => cur.get(n.as_u64()? as usize)?,
            _ => return None,
        };
    }
    Some(cur)
}

fn set_path(v: &mut Value, path: &[Value], to: &Value) -> bool {
    let Some((last, head)) = path.split_last() else { return false };
    let mut cur = v;
    for p in head {
        let next = match p {
            Value::String(k) => cur.get_mut(k),
            Value::Number(n) => n.as_u64().and_then(|i| cur.get_mut(i as usize)),
            _ => None,
        };
        match next {
            Some(n) => cur = n,
            None => return false,
        }
    }
    match last {
        Value::String(k) => cur.as_object_mut().map(|o| o.insert(k.clone(), to.clone())).is_some(),
        Value::Number(n) => match (n.as_u64(), cur.as_array_mut()) {
            (Some(i), Some(a)) if (i as usize) < a.len() => {
                a[i as usize] = to.clone();
                true
            }
            _ => false,
        },
        _ => false,
    }
}

/// `SWR-FM-04`: an edit to a variant of an editable family is offered to the other variants, per variant and per
/// field, showing what would change in each. Intent fields are offered; sockets and zone references stay with the variant.
/// A sibling on the other side of the line between an owner's protocols and a user's private ones (`scope`) is never offered.
/// A change of structure (a modality added, removed or of another type at its position) is not offered.
fn propagation_offer(req: &Value) -> R<Value> {
    let source_scope = req["scope"].as_str().unwrap_or("owner");
    let mut changes = Vec::new();
    diff_into(&mut Vec::new(), &req["old"], &req["new"], &mut changes);
    let siblings = req["siblings"].as_array().ok_or("siblings must be an array")?;
    let mut out = Vec::new();
    for s in siblings {
        if s["scope"].as_str().unwrap_or("owner") != source_scope {
            continue;
        }
        let offers: Vec<Value> = changes
            .iter()
            .filter_map(|c| {
                let path = c["path"].as_array()?;
                let current = get_path(&s["def"], path)?;
                if current == &c["to"] {
                    return None; // already the same: nothing to offer
                }
                Some(json!({ "path": path, "from": current, "to": c["to"] }))
            })
            .collect();
        if !offers.is_empty() {
            out.push(json!({ "id": id_of(s), "changes": offers }));
        }
    }
    Ok(json!({ "offers": out }))
}

/// Apply the changes the editor accepted to a sibling's definition. A path that does not exist there is refused, not created.
fn apply_changes(req: &Value) -> R<Value> {
    let mut def = req["def"].clone();
    for c in req["changes"].as_array().ok_or("changes must be an array")? {
        let path = c["path"].as_array().ok_or("a change needs a path")?;
        if path.iter().any(|p| p.as_str().is_some_and(|k| VARIANT_SPECIFIC.contains(&k))) {
            return Err("a socket or zone reference is never propagated".into());
        }
        if !set_path(&mut def, path, &c["to"]) {
            return Err("a change names a field the variant does not have".into());
        }
    }
    Ok(json!({ "def": def }))
}

/// `SWR-FM-07`: the creation of a variant is automated by mapping a source variant onto the target layout. There is
/// one layout, so there is nothing to map and the answer says so; the mapping is built when a second version exists.
fn map_variant(req: &Value) -> R<Value> {
    let from = req["sourceVersion"].as_u64().and_then(|x| u32::try_from(x).ok()).ok_or("sourceVersion is required")?;
    let to = req["targetVersion"].as_u64().and_then(|x| u32::try_from(x).ok()).ok_or("targetVersion is required")?;
    if from == to || layouts_table().len() < 2 {
        return Ok(json!({ "status": "nothing_to_map", "changes": [] }));
    }
    if !layouts_table().contains_key(&to) {
        return Err(format!("no layout for hardware version {to}"));
    }
    Ok(json!({ "status": "not_supported" }))
}

/// One entry point for every binding: `{"op": "...", ...}` in, JSON out. A binding does marshalling and nothing else.
pub fn library_json(request: &str) -> R<String> {
    let req: Value = serde_json::from_str(request).map_err(|e| format!("library request is not JSON: {e}"))?;
    let op = req["op"].as_str().ok_or("op is required")?;
    let out = match op {
        "saveCheck" => save_check(&req)?,
        "deleteCheck" => delete_check(&req)?,
        "cloneFamily" => clone_family(&req)?,
        "addVariantCheck" => add_variant_check(&req)?,
        "variantFor" => variant_for(&req)?,
        "supportedVersions" => supported_versions(&req)?,
        "layouts" => layouts(&req)?,
        "layoutDiff" => layout_diff(&req)?,
        "uploadGate" => upload_gate(&req)?,
        "uploadResult" => upload_result(&req)?,
        "mergeImport" => merge_import(&req)?,
        "offered" => offered(&req)?,
        "usedRecord" => used_record(&req)?,
        "propagationOffer" => propagation_offer(&req)?,
        "applyChanges" => apply_changes(&req)?,
        "mapVariant" => map_variant(&req)?,
        other => return Err(format!("unknown library operation '{other}'")),
    };
    Ok(out.to_string())
}
