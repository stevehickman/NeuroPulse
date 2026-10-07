//! The serializer: models to `.npps` text, the inverse of the parser (OI-NPPS-CORE-01).
//!
//! The input is the shape `api::parse_json` returns: a protocol or composite, a zone, a condition, a
//! `wavelength_rules` set or a `limits` set, in the camelCase the apps' models use. Ids and timestamps are
//! the model's own and are ignored except where the file carries them (`id:`). The output is what the
//! web serializer wrote before this replaced it, byte for byte for everything the shipped library contains
//! (`tests/serialize.rs` holds that to `npps-serialize-golden.json`), with two deliberate differences.
//!
//! A duration of whole hours is written in minutes: the lexer has no hours unit, and the web writer's `1h` was a
//! file its own parser refused.
//!
//! The web `limits` writer derived the key by snake-casing the property name,
//! which gave `max_irradiance_m_wcm2` and `max_frequency_hz`, spellings the parser does not read, so a
//! written ceiling was dropped on the next load. The keys here are the parser's own (the table in
//! `parser/blocks.rs`), so a limits set survives a round trip.

use crate::js::num_string;
use crate::parser::blocks::{camel_modality, describe_invalid, limits_fields, to_socket_set, Conv, MODALITY_LIMITS_KEYS, NP_SOCKET_ID_MAX, NP_SOCKET_ID_MIN};
use serde_json::Value;

const INDENT: &str = "    ";

type R<T> = Result<T, String>;

fn esc(s: &str) -> String {
    s.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n")
}

fn q(s: &str) -> String {
    format!("\"{}\"", esc(s))
}

fn str_arr(a: &[String]) -> String {
    if a.is_empty() {
        "[]".into()
    } else {
        format!("[{}]", a.iter().map(|s| q(s)).collect::<Vec<_>>().join(", "))
    }
}

fn format_time(seconds: f64) -> String {
    if seconds == 0.0 {
        "0s".into()
    } else if seconds % 60.0 == 0.0 {
        // No hours: the lexer's units are s and m, and the web writer's `1h` was a file its own parser refused.
        format!("{}m", num_string(seconds / 60.0))
    } else {
        format!("{}s", num_string(seconds))
    }
}

fn format_hz(hz: f64) -> String {
    format!("{}Hz", num_string(hz))
}

fn truthy(v: &Value) -> bool {
    match v {
        Value::Null => false,
        Value::Bool(b) => *b,
        Value::Number(n) => n.as_f64().map_or(false, |x| x != 0.0),
        Value::String(s) => !s.is_empty(),
        _ => true,
    }
}

/// A required field of a params object; a model without it cannot be written.
struct Params<'a> {
    modality: &'a str,
    v: &'a Value,
}

impl Params<'_> {
    fn field(&self, k: &str) -> R<&Value> {
        match &self.v[k] {
            Value::Null => Err(format!("{}: {k} is missing — an invalid params object, not a serialization choice.", self.modality)),
            v => Ok(v),
        }
    }
    fn num(&self, k: &str) -> R<f64> {
        self.field(k)?.as_f64().ok_or_else(|| format!("{}: {k} must be a number", self.modality))
    }
    fn n(&self, k: &str) -> R<String> {
        Ok(num_string(self.num(k)?))
    }
    fn s(&self, k: &str) -> R<&str> {
        self.field(k)?.as_str().ok_or_else(|| format!("{}: {k} must be a string", self.modality))
    }
    fn b(&self, k: &str) -> R<bool> {
        self.field(k)?.as_bool().ok_or_else(|| format!("{}: {k} must be true or false", self.modality))
    }
    fn hz(&self, k: &str) -> R<String> {
        Ok(format_hz(self.num(k)?))
    }
    fn opt_num(&self, k: &str) -> Option<f64> {
        self.v[k].as_f64()
    }
    fn strings(&self, k: &str) -> R<Vec<String>> {
        string_list(self.field(k)?, &format!("{}: {k}", self.modality))
    }
}

fn string_list(v: &Value, what: &str) -> R<Vec<String>> {
    v.as_array()
        .ok_or_else(|| format!("{what} must be a list"))?
        .iter()
        .map(|x| x.as_str().map(str::to_string).ok_or_else(|| format!("{what} must be a list of strings")))
        .collect()
}

fn modality_fields(kind: &str, params: &Value) -> R<Vec<String>> {
    let p = Params { modality: kind, v: params };
    Ok(match kind {
        "pbm_transcranial" => {
            let mut lines = vec![
                format!("wavelength: {}", q(p.s("wavelength")?)),
                format!("irradiance: {}mW_cm2", p.n("irradianceMWcm2")?),
                format!("frequency: {}", p.hz("frequencyHz")?),
                format!("duty_cycle: {}%", p.n("dutyCyclePercent")?),
            ];
            // `named` and `clinician_selected` are the internal discriminant and only the second is surface syntax.
            if params["zones"] == "clinician_selected" {
                lines.push("zones: clinician_selected".into());
            } else if params["zoneRefs"].as_array().map_or(false, |a| !a.is_empty()) {
                lines.push(format!("zones: {}", str_arr(&p.strings("zoneRefs")?)));
            } else {
                return Err("pbm_transcranial: zones is 'named' but zoneRefs is empty — a named target must list at least one zone. This is an invalid params object, not a serialization choice.".into());
            }
            lines
        }
        "pbm_intranasal" => vec![
            format!("wavelength: {}", q(p.s("wavelength")?)),
            format!("irradiance: {}mW_cm2", p.n("irradianceMWcm2")?),
            format!("frequency: {}", p.hz("frequencyHz")?),
            format!("duty_cycle: {}%", p.n("dutyCyclePercent")?),
        ],
        "eeg_neurofeedback" => {
            let mut lines = vec![format!("band: {}", p.s("band")?)];
            if params["channels"] == "custom" && truthy(&params["customChannels"]) {
                lines.push(format!("custom_channels: {}", str_arr(&p.strings("customChannels")?)));
            } else {
                lines.push(format!("channels: {}", p.s("channels")?));
            }
            lines.push(format!("closed_loop: {}", p.b("closedLoopEnabled")?));
            lines
        }
        "bes_tacs" => vec![
            format!("frequency: {}", p.hz("frequencyHz")?),
            format!("intensity: {}mA", p.n("intensityMilliamps")?),
            format!("waveform: {}", p.s("waveform")?),
        ],
        "tdcs" => {
            let pairs = p
                .field("electrodePairs")?
                .as_array()
                .ok_or("tdcs: electrodePairs must be a list")?
                .iter()
                .map(|pair| match (pair[0].as_str(), pair[1].as_str()) {
                    (Some(a), Some(b)) => Ok(format!("[{}, {}]", q(a), q(b))),
                    _ => Err("tdcs: an electrode pair is two names".to_string()),
                })
                .collect::<R<Vec<_>>>()?
                .join(", ");
            vec![
                format!("intensity: {}mA", p.n("intensityMilliamps")?),
                format!("electrode_pairs: [{pairs}]"),
                format!("electrode_area_cm2: {}", p.n("electrodeAreaCm2")?),
                format!("ramp: {}s", p.n("rampSeconds")?),
            ]
        }
        "vns_hrv" => {
            let mut lines = vec![
                format!("frequency: {}", p.hz("frequencyHz")?),
                format!("intensity: {}mA", p.n("intensityMilliamps")?),
                format!("hrv_protocol: {}", p.s("hrvProtocol")?),
                format!("breathing_rate: {}", p.n("resonanceBreathingRate")?),
            ];
            // The optional study parameters are written only when authored, so a block without them round-trips unchanged.
            for key in ["side", "trigger", "intensityBasis"] {
                if let Some(v) = params[key].as_str() {
                    lines.push(format!("{}: {v}", snake(key)));
                }
            }
            for key in ["pulseWidthUs", "burstSeconds", "intensityPercentOfThreshold"] {
                if let Some(n) = p.opt_num(key) {
                    lines.push(format!("{}: {}", snake(key), num_string(n)));
                }
            }
            lines
        }
        "audio_entrainment" => {
            let mut lines = Vec::new();
            if let Some(h) = p.opt_num("binauralBeatsHz") {
                lines.push(format!("binaural_hz: {}", format_hz(h)));
            }
            if let Some(h) = p.opt_num("isochronicTonesHz") {
                lines.push(format!("isochronic_hz: {}", format_hz(h)));
            }
            lines.push(match params["noiseType"].as_str() {
                Some(n) => format!("noise: {n}"),
                None => "noise: none".into(),
            });
            lines.push(format!("carrier_hz: {}", p.hz("carrierHz")?));
            lines.push(format!("volume: {}dB", p.n("volumeDb")?));
            lines.push(format!("eeg_adaptive: {}", p.b("eegAdaptive")?));
            lines.push(format!("bone_conduction_pacer: {}", p.b("boneConductionPacer")?));
            lines
        }
        "visual_stimulation" => vec![
            format!("frequency: {}", p.hz("frequencyHz")?),
            format!("mode: {}", p.s("mode")?),
            format!("emdr_cadence: {}", p.hz("emdrCadenceHz")?),
            format!("enable_mode_f: {}", p.b("enableModeF")?),
        ],
        "qeeg_21ch" => vec![
            format!("montage: {}", p.s("montage")?),
            format!("sloreta_enabled: {}", p.b("sloretaEnabled")?),
            format!("reference: {}", p.s("reference")?),
        ],
        "tms" => vec![
            format!("tms_protocol: {}", p.s("tmsProtocol")?),
            format!("frequency: {}", p.hz("frequencyHz")?),
            format!("intensity_percent_mt: {}", p.n("intensityPercentMT")?),
            format!("target: {}", p.s("target")?),
            format!("pulse_count: {}", p.n("pulseCount")?),
        ],
        "pbm_deep_1170nm" => vec![
            format!("intensity_mw_cm2: {}", p.n("intensityMWcm2")?),
            format!("frequency: {}", p.hz("frequencyHz")?),
            format!("duty_cycle: {}%", p.n("dutyCyclePercent")?),
        ],
        "clinical_tacs" => vec![
            format!("frequency: {}", p.hz("frequencyHz")?),
            format!("intensity: {}mA", p.n("intensityMilliamps")?),
            format!("channel_count: {}", p.n("channelCount")?),
            format!("waveform: {}", p.s("waveform")?),
        ],
        "hd_tdcs" => vec![
            format!("target: {}", p.s("target")?),
            format!("montage: {}", p.s("montage")?),
            format!("intensity: {}mA", p.n("intensityMilliamps")?),
        ],
        "cervical_vns" => vec![
            format!("frequency: {}", p.hz("frequencyHz")?),
            format!("intensity: {}mA", p.n("intensityMilliamps")?),
        ],
        "vibrotactile_40hz" => vec![
            format!("intensity_g: {}", p.n("intensityG")?),
            format!("sync_to_audio: {}", p.b("syncToAudio")?),
            format!("sync_to_visual: {}", p.b("syncToVisual")?),
        ],
        other => return Err(format!("Unknown modality type: {other}")),
    })
}

fn modality(m: &Value, level: usize) -> R<String> {
    let pad = INDENT.repeat(level);
    let kind = m["type"].as_str().ok_or("a modality has no type")?;
    let mut lines = vec![format!("{pad}{kind} {{")];
    for f in modality_fields(kind, &m["params"])? {
        lines.push(format!("{pad}{INDENT}{f}"));
    }
    let iv = &m["interval"];
    let on = iv["intervalOnSeconds"].as_f64().unwrap_or(0.0);
    let off = iv["intervalOffSeconds"].as_f64().unwrap_or(0.0);
    let start = iv["startOffsetSeconds"].as_f64().unwrap_or(0.0);
    if start > 0.0 {
        lines.push(format!("{pad}{INDENT}start: {}", format_time(start)));
    }
    if !(on == 0.0 && off == 0.0) {
        lines.push(format!("{pad}{INDENT}interval_on: {}", format_time(on)));
        lines.push(format!("{pad}{INDENT}interval_off: {}", format_time(off)));
        match iv["repeatCount"].as_f64() {
            Some(n) => lines.push(format!("{pad}{INDENT}repeat: {}", num_string(n))),
            None => lines.push(format!("{pad}{INDENT}repeat: until_end")),
        }
    }
    if m["enabled"] == false {
        lines.push(format!("{pad}{INDENT}enabled: false"));
    }
    lines.push(format!("{pad}}}"));
    Ok(lines.join("\n"))
}

/// `pulseWidthUs` to `pulse_width_us`: the file's spelling of a model key.
fn snake(camel: &str) -> String {
    let mut out = String::new();
    for c in camel.chars() {
        if c.is_ascii_uppercase() {
            out.push('_');
            out.push(c.to_ascii_lowercase());
        } else {
            out.push(c);
        }
    }
    out
}

fn text<'a>(v: &'a Value, k: &str) -> &'a str {
    v[k].as_str().unwrap_or("")
}

fn references(refs: &[Value]) -> String {
    if refs.is_empty() {
        return "[]".into();
    }
    let one = |r: &Value| match r {
        Value::String(s) => q(s),
        o => format!("[{}, {}]", q(text(o, "label")), q(text(o, "url"))),
    };
    format!("[{}]", refs.iter().map(one).collect::<Vec<_>>().join(", "))
}

/// The header fields `protocol` and `composite` share, in the order the web serializer wrote them.
fn header(def: &Value, lines: &mut Vec<String>) -> R<()> {
    lines.push(format!("{INDENT}id: {}", q(text(def, "id"))));
    lines.push(format!("{INDENT}description: {}", q(text(def, "description"))));
    lines.push(format!("{INDENT}author: {}", q(text(def, "author"))));
    lines.push(format!("{INDENT}version: {}", q(text(def, "version"))));
    if def["isReadOnly"] == true {
        lines.push(format!("{INDENT}readonly: true"));
    }
    lines.push(format!("{INDENT}tags: {}", str_arr(&string_list(&def["tags"], "tags")?)));
    if let Some(c) = def["conditions"].as_array().filter(|c| !c.is_empty()) {
        lines.push(format!("{INDENT}conditions: {}", str_arr(&string_list(&Value::Array(c.clone()), "conditions")?)));
    }
    if let Some(r) = def["references"].as_array().filter(|r| !r.is_empty()) {
        lines.push(format!("{INDENT}references: {}", references(r)));
    }
    Ok(())
}

fn single(p: &Value) -> R<String> {
    let mut lines = vec![format!("protocol {} {{", q(text(p, "name")))];
    header(p, &mut lines)?;
    let tm = &p["timingMode"];
    if tm["type"] == "duration" {
        lines.push(format!("{INDENT}duration: {}", format_time(tm["seconds"].as_f64().ok_or("timingMode.seconds must be a number")?)));
    } else {
        lines.push(format!("{INDENT}interval_count: {}", num_string(tm["count"].as_f64().ok_or("timingMode.count must be a number")?)));
    }
    for (json_key, key) in [("sessionsPerWeek", "sessions_per_week"), ("courseWeeks", "course_weeks")] {
        if let Some(n) = p[json_key].as_f64() {
            lines.push(format!("{INDENT}{key}: {}", num_string(n)));
        }
    }
    for m in p["modalities"].as_array().ok_or("modalities must be a list")? {
        lines.push(String::new());
        lines.push(modality(m, 1)?);
    }
    lines.push("}".into());
    Ok(lines.join("\n"))
}

fn composite(c: &Value) -> R<String> {
    let mut lines = vec![format!("composite {} {{", q(text(c, "name")))];
    header(c, &mut lines)?;
    lines.push(format!("{INDENT}conflict_resolution: {}", text(c, "conflictResolution")));
    for l in c["layers"].as_array().ok_or("layers must be a list")? {
        lines.push(String::new());
        lines.push(format!("{INDENT}layer {} {{", q(text(l, "protocolName"))));
        lines.push(format!("{INDENT}{INDENT}start: {}", format_time(l["startOffsetSeconds"].as_f64().unwrap_or(0.0))));
        if let Some(d) = l["durationSeconds"].as_f64() {
            lines.push(format!("{INDENT}{INDENT}duration: {}", format_time(d)));
        }
        let scale = l["intensityScale"].as_f64().unwrap_or(1.0);
        if (scale - 1.0).abs() > 0.001 {
            lines.push(format!("{INDENT}{INDENT}intensity_scale: {}", num_string(scale)));
        }
        lines.push(format!("{INDENT}}}"));
    }
    lines.push("}".into());
    Ok(lines.join("\n"))
}

/// A zone's socket list is canonicalised on the way out, so the writer holds the contract the reader does: a
/// zone carrying `[0, 4, 4, 999]` is refused here rather than written into a file its own parser rejects.
fn zone(z: &Value) -> R<String> {
    let raw = z["sockets"].as_array().cloned().unwrap_or_default();
    let (sockets, invalid) = to_socket_set(&raw);
    if !invalid.is_empty() {
        return Err(format!(
            "cannot serialize zone \"{}\": {} {} on this helmet — ids are whole numbers {NP_SOCKET_ID_MIN}–{NP_SOCKET_ID_MAX}",
            text(z, "name"),
            invalid.iter().map(|v| describe_invalid(v)).collect::<Vec<_>>().join(", "),
            if invalid.len() == 1 { "is not a socket" } else { "are not sockets" },
        ));
    }
    let mut lines = vec![format!("zone {} {{", q(text(z, "name")))];
    if !text(z, "id").is_empty() {
        lines.push(format!("{INDENT}id: {}", q(text(z, "id"))));
    }
    if !text(z, "description").is_empty() {
        lines.push(format!("{INDENT}description: {}", q(text(z, "description"))));
    }
    lines.push(format!(
        "{INDENT}sockets: [{}]",
        sockets.iter().map(i64::to_string).collect::<Vec<_>>().join(", ")
    ));
    if z["types"].is_array() {
        lines.push(format!("{INDENT}types: {}", str_arr(&string_list(&z["types"], "types")?)));
    }
    if truthy(&z["excludeTypes"]) {
        lines.push(format!("{INDENT}exclude_types: true"));
    }
    lines.push("}".into());
    Ok(lines.join("\n"))
}

fn condition(c: &Value) -> R<String> {
    let mut lines = vec![format!("condition {} {{", q(text(c, "name")))];
    if !text(c, "id").is_empty() {
        lines.push(format!("{INDENT}id: {}", q(text(c, "id"))));
    }
    lines.push(format!("{INDENT}link: {}", q(text(c, "link"))));
    if !text(c, "code").is_empty() {
        lines.push(format!("{INDENT}code: {}", q(text(c, "code"))));
    }
    if !text(c, "description").is_empty() {
        lines.push(format!("{INDENT}description: {}", q(text(c, "description"))));
    }
    lines.push("}".into());
    Ok(lines.join("\n"))
}

fn wavelength_rules(w: &Value) -> R<String> {
    let mut lines = vec![format!("wavelength_rules {} {{", q(text(w, "name")))];
    lines.push(format!("{INDENT}level: {}", text(w, "level")));
    if !text(w, "description").is_empty() {
        lines.push(format!("{INDENT}description: {}", q(text(w, "description"))));
    }
    for c in w["channels"].as_array().ok_or("channels must be a list")? {
        lines.push(String::new());
        lines.push(format!("{INDENT}channel {} {{", q(text(c, "element"))));
        for (k, j) in [("nominal_nm", "nominalNm"), ("min_nm", "minNm"), ("max_nm", "maxNm")] {
            let v = c[j].as_f64().ok_or_else(|| format!("wavelengthRules {j} must be a number"))?;
            lines.push(format!("{INDENT}{INDENT}{k}: {}", num_string(v)));
        }
        lines.push(format!("{INDENT}}}"));
    }
    lines.push("}".into());
    Ok(lines.join("\n"))
}

fn limits(l: &Value) -> R<String> {
    let mut lines = vec![format!("limits {} {{", q(text(l, "name")))];
    lines.push(format!("{INDENT}level: {}", text(l, "level")));
    for (k, j) in [("helmet_id", "helmetId"), ("individual_id", "individualId"), ("description", "description")] {
        if !text(l, j).is_empty() {
            lines.push(format!("{INDENT}{k}: {}", q(text(l, j))));
        }
    }
    for kind in MODALITY_LIMITS_KEYS {
        let block = &l[camel_modality(kind)];
        let Some(obj) = block.as_object() else { continue };
        let mut body = Vec::new();
        let mut written: Vec<&str> = Vec::new();
        for (key, out, conv) in limits_fields(kind) {
            // Several spellings can read into one property; the first is the one written.
            if matches!(conv, Conv::Retired(_)) || written.contains(out) {
                continue;
            }
            let Some(v) = obj.get(*out).filter(|v| !v.is_null()) else { continue };
            written.push(out);
            body.push(match conv {
                Conv::Num => format!("{key}: {}", num_string(v.as_f64().ok_or_else(|| format!("{kind} limits: {out} must be a number"))?)),
                Conv::List => format!("{key}: {}", str_arr(&string_list(v, &format!("{kind} limits: {out}"))?)),
                Conv::Flag => format!("{key}: {}", v.as_bool().ok_or_else(|| format!("{kind} limits: {out} must be true or false"))?),
                Conv::Retired(_) => unreachable!(),
            });
        }
        if body.is_empty() {
            continue;
        }
        lines.push(format!("{INDENT}{kind} {{"));
        lines.extend(body.into_iter().map(|b| format!("{INDENT}{INDENT}{b}")));
        lines.push(format!("{INDENT}}}"));
    }
    lines.push("}".into());
    Ok(lines.join("\n"))
}

/// One item of a serialize request: `{"kind":"single","protocol":…}`, `{"kind":"composite","composite":…}`,
/// `{"kind":"zone","zone":…}`, `{"kind":"condition","condition":…}`,
/// `{"kind":"wavelengthRules","wavelengthRules":…}` or `{"kind":"limits","limits":…}`.
pub fn serialize_item(item: &Value) -> R<String> {
    let kind = item["kind"].as_str().ok_or("a serialize item needs a kind")?;
    let body = |k: &str| match &item[k] {
        Value::Null => Err(format!("a '{kind}' item needs a '{k}'")),
        v => Ok(v),
    };
    match kind {
        "single" => single(body("protocol")?),
        "composite" => composite(body("composite")?),
        "zone" => zone(body("zone")?),
        "condition" => condition(body("condition")?),
        "wavelengthRules" => wavelength_rules(body("wavelengthRules")?),
        "limits" => limits(body("limits")?),
        other => Err(format!("unknown serialize item kind '{other}'")),
    }
}
