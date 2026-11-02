//! A composite as one timed plan (`OI-NPPS-COMP-01`, option A; `SWR-PL-01`).
//!
//! A composite layers named protocols on one timeline. [`plan`] turns it into the definition of a single protocol: the
//! same shape the protocol compiler and the identity hash read, with every block placed on the composite's clock, cut
//! at the end of its layer, and its dose scaled. The compiler builds the descriptor from that definition and the
//! identity hashes it, so a composite's identity is the identity of what it delivers and not of how it was written.
//!
//! **The rules, each of which refuses and none of which reshapes (`CLAUDE.md` §3):**
//! * `merge`: layers run at their authored starts, at once where they overlap.
//! * `sequential`: layer N starts when layer N-1 ends. The authored start is not read.
//! * `override`: where a later layer (by start, then by authored order) overlaps an earlier one, the earlier layer's
//!   blocks of every modality type the later layer has stop at the later layer's start.
//! * A layer runs for its `durationSeconds`, or for its protocol's own length when it states none. It may not run longer
//!   than its protocol, and a protocol timed by `interval_count` has no length, so its layer must state one.
//! * `intensityScale` is above 0 and at most 1 (the validator's range, `VALIDATE_PARAM_LAYER_INTENSITY_SCALE`). It scales
//!   the dose field of each block: irradiance, stimulation current, vibration gain, and an audio level as sound
//!   pressure (a dB figure moves by 20 log10 of the scale). A block with no dose to scale (acquisition) is left alone.
//!   A modality with a dose the compiler cannot scale refuses any scale other than 1.
//! * Two blocks of one modality type that overlap in time and come from different layers are refused, except
//!   transcranial PBM on sockets that share none. That check needs the zone namespace, so it is the compiler's
//!   (`compiler::compile_composite`), not the plan's: an identity is not a validity check.

use serde_json::{json, Map, Value};

type R<T> = Result<T, String>;

/// How a modality's dose is scaled by a layer.
enum Dose {
    /// A linear dose field: the value is multiplied.
    Linear(&'static str),
    /// A level in dB of sound pressure: the value moves by `20 log10(scale)`.
    Decibel(&'static str),
    /// No dose (acquisition only).
    None,
    /// A dose the compiler has no rule for.
    Unscalable,
}

fn dose_of(ty: &str) -> Dose {
    match ty {
        "pbm_transcranial" | "pbm_intranasal" => Dose::Linear("irradianceMWcm2"),
        "pbm_deep_1170nm" => Dose::Linear("intensityMWcm2"),
        "bes_tacs" | "tdcs" | "vns_hrv" => Dose::Linear("intensityMilliamps"),
        "vibrotactile_40hz" => Dose::Linear("intensityG"),
        "audio_entrainment" => Dose::Decibel("volumeDb"),
        "eeg_neurofeedback" | "qeeg_21ch" => Dose::None,
        _ => Dose::Unscalable,
    }
}

fn scaled_params(ty: &str, params: &Value, scale: f64, layer: &str) -> R<Value> {
    if scale == 1.0 {
        return Ok(params.clone());
    }
    let mut p = params.as_object().cloned().unwrap_or_default();
    let (key, db) = match dose_of(ty) {
        Dose::Linear(k) => (k, false),
        Dose::Decibel(k) => (k, true),
        Dose::None => return Ok(params.clone()),
        Dose::Unscalable => {
            return Err(format!(
                "Layer \"{layer}\" scales a {ty} block by {scale}, and the compiler has no rule for scaling that modality's dose. Use a scale of 1, or leave the modality out of the layer's protocol."
            ))
        }
    };
    let v = p.get(key).and_then(Value::as_f64).ok_or_else(|| format!("Layer \"{layer}\": the {ty} block has no {key} to scale."))?;
    let out = if db { v + 20.0 * scale.log10() } else { v * scale };
    p.insert(key.into(), json!(out));
    Ok(Value::Object(p))
}

struct Block {
    layer: usize,
    ty: String,
    params: Value,
    interval: Map<String, Value>,
    start_s: f64,
    end_s: f64,
}

/// The single-protocol definition of `composite`. `members` maps a protocol name to its definition (the shape
/// `compile_json` takes as `def`). The result has `timingMode` (a duration, the end of the last block) and `modalities`,
/// each with `type`, `params`, an `interval` that states its absolute `startOffsetSeconds` and its `endOffsetSeconds`,
/// and the `layer` it came from.
pub fn plan(composite: &Value, members: &Map<String, Value>) -> R<Value> {
    let mode = composite["conflictResolution"].as_str().unwrap_or("merge");
    if !matches!(mode, "merge" | "sequential" | "override") {
        return Err(format!("Composite conflict resolution \"{mode}\" is not merge, sequential or override."));
    }
    let layers = composite["layers"].as_array().filter(|l| !l.is_empty()).ok_or("A composite needs at least one layer.")?;
    let mut blocks: Vec<Block> = Vec::new();
    let mut cursor = 0.0f64;
    let mut windows: Vec<(f64, f64, Vec<String>)> = Vec::new();
    for (li, layer) in layers.iter().enumerate() {
        let name = layer["protocolName"].as_str().ok_or("A composite layer has no protocol name.")?;
        let member = members.get(name).ok_or_else(|| format!("Composite layer \"{name}\" names a protocol that is not in the library."))?;
        let scale = layer["intensityScale"].as_f64().unwrap_or(1.0);
        if !(scale > 0.0 && scale <= 1.0) {
            return Err(format!("Composite layer \"{name}\" has intensity scale {scale}; it must be above 0 and at most 1. The layer is refused, not adjusted."));
        }
        let natural = (member["timingMode"]["type"] == "duration").then(|| member["timingMode"]["seconds"].as_f64()).flatten();
        let length = match (layer["durationSeconds"].as_f64(), natural) {
            (Some(d), Some(n)) if d > n => {
                return Err(format!("Composite layer \"{name}\" runs for {d} s, longer than its protocol's {n} s. A layer is cut short, never repeated or stretched."))
            }
            (Some(d), _) => d,
            (None, Some(n)) => n,
            (None, None) => {
                return Err(format!("Composite layer \"{name}\" names a protocol timed by interval_count, which has no length of its own. Give the layer a duration."))
            }
        };
        if !(length > 0.0) {
            return Err(format!("Composite layer \"{name}\" has no length."));
        }
        let start = if mode == "sequential" { cursor } else { layer["startOffsetSeconds"].as_f64().unwrap_or(0.0) };
        if !(start >= 0.0) {
            return Err(format!("Composite layer \"{name}\" starts before the composite does."));
        }
        cursor = start + length;
        let end = start + length;
        let count = (member["timingMode"]["type"] == "interval_count").then(|| member["timingMode"]["count"].clone());
        let mut types = Vec::new();
        for m in member["modalities"].as_array().into_iter().flatten() {
            if m["enabled"] == false {
                continue;
            }
            let ty = m["type"].as_str().ok_or("A modality has no type.")?.to_string();
            let mut interval = m["interval"].as_object().cloned().unwrap_or_default();
            let offset = interval.get("startOffsetSeconds").and_then(Value::as_f64).unwrap_or(0.0);
            if let (Some(c), false) = (&count, interval.contains_key("repeatCount")) {
                interval.insert("repeatCount".into(), c.clone());
            }
            types.push(ty.clone());
            blocks.push(Block { layer: li, params: scaled_params(&ty, &m["params"], scale, name)?, ty, interval, start_s: start + offset, end_s: end });
        }
        windows.push((start, end, types));
    }
    if mode == "override" {
        // Layers in the order they take over: by start, then as authored. A later layer cuts an earlier one's blocks of the
        // types it has itself.
        let mut order: Vec<usize> = (0..windows.len()).collect();
        order.sort_by(|&a, &b| windows[a].0.partial_cmp(&windows[b].0).unwrap_or(std::cmp::Ordering::Equal).then(a.cmp(&b)));
        for (pos, &later) in order.iter().enumerate() {
            let (cut, _, types) = (windows[later].0, windows[later].1, &windows[later].2);
            for &earlier in &order[..pos] {
                for b in blocks.iter_mut().filter(|b| b.layer == earlier && types.contains(&b.ty)) {
                    b.end_s = b.end_s.min(cut);
                }
            }
        }
    }
    blocks.retain(|b| b.end_s > b.start_s);
    if blocks.is_empty() {
        return Err("The composite delivers nothing: every block is cut before it starts.".into());
    }
    let total = blocks.iter().map(|b| b.end_s).fold(0.0, f64::max);
    let modalities: Vec<Value> = blocks
        .into_iter()
        .map(|b| {
            let mut interval = b.interval;
            interval.insert("startOffsetSeconds".into(), json!(b.start_s));
            interval.insert("endOffsetSeconds".into(), json!(b.end_s));
            json!({ "type": b.ty, "enabled": true, "params": b.params, "interval": interval, "layer": b.layer })
        })
        .collect();
    Ok(json!({ "timingMode": { "type": "duration", "seconds": total }, "modalities": modalities }))
}
