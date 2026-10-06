//! The blocks around a `protocol`: `composite`, `zone`, `condition`, `wavelength_rules` and `limits`.
//! Ports of the matching methods of `Parser` in common/lib/nppsParser.ts, messages verbatim.
//! Ids and timestamps the reference generates per parse (`crypto.randomUUID()`, `new Date()`) are
//! not produced here: the caller that builds a model supplies them, as it does for a protocol.

use super::{Parser, R};
use crate::error::ParseError;
use crate::js::{num_string, number, value_string};
use crate::lexer::Kind;
use serde_json::{json, Map, Value};

// The helmet's socket ids come from hardware/np_socket_map.json (build.rs). The lattice is contiguous (the generated map
// has no holes), so a valid id is one in that range.
pub(crate) use crate::constants::socket_lattice::{NP_SOCKET_ID_MAX, NP_SOCKET_ID_MIN, NP_SOCKET_NUMBERING_BASE};
const PBM_CHANNEL_ELEMENTS: [&str; 3] = ["led_660", "led_808", "led_1064"];

/// The `limits` keys that introduce a per-modality sub-block rather than a scalar.
pub(crate) const MODALITY_LIMITS_KEYS: [&str; 14] = [
    "pbm_transcranial", "pbm_intranasal", "eeg_neurofeedback", "bes_tacs", "tdcs", "vns_hrv",
    "audio_entrainment", "visual_stimulation", "tms", "pbm_deep_1170nm", "clinical_tacs", "hd_tdcs",
    "cervical_vns", "vibrotactile_40hz",
];

/// How a limits field's raw value becomes its output.
#[derive(Clone, Copy)]
pub(crate) enum Conv {
    /// `Number(v)`.
    Num,
    /// `Array.isArray(v) ? v : [String(v)]`.
    List,
    /// `Boolean(v)`.
    Flag,
    /// A percentage ceiling, refused by name: the replacement field to write instead.
    Retired(&'static str),
}

use Conv::{Flag, List, Num, Retired};

pub(crate) type Fields = &'static [(&'static str, &'static str, Conv)];

/// Per modality: `(npps key, output key, conversion)`. Unknown keys are read and dropped.
pub(crate) fn limits_fields(modality: &str) -> Fields {
    match modality {
        "pbm_transcranial" => &[
            ("max_irradiance_mw_cm2", "maxIrradianceMWcm2", Num),
            ("max_intensity", "", Retired("max_irradiance_mw_cm2")),
            ("max_frequency", "maxFrequencyHz", Num),
            ("max_duty_cycle", "maxDutyCyclePercent", Num),
            ("max_session_dose", "maxSessionDoseJCm2", Num),
            ("max_daily_dose", "maxDailyDoseJCm2", Num),
        ],
        "pbm_intranasal" => &[
            ("max_irradiance_mw_cm2", "maxIrradianceMWcm2", Num),
            ("max_intensity", "", Retired("max_irradiance_mw_cm2")),
            ("max_session_dose", "maxSessionDoseJCm2", Num),
            ("max_session_duration", "maxSessionDurationSeconds", Num),
        ],
        "eeg_neurofeedback" => &[
            ("allowed_bands", "allowedBands", List),
            ("require_closed_loop", "requireClosedLoop", Flag),
        ],
        "bes_tacs" => &[
            ("max_intensity", "maxIntensityMilliamps", Num),
            ("max_frequency", "maxFrequencyHz", Num),
            ("min_frequency", "minFrequencyHz", Num),
            ("max_session_duration", "maxSessionDurationSeconds", Num),
            ("max_sessions_per_day", "maxSessionsPerDay", Num),
        ],
        "tdcs" => &[
            ("max_intensity", "maxIntensityMilliamps", Num),
            ("max_session_duration", "maxSessionDurationSeconds", Num),
            ("max_sessions_per_day", "maxSessionsPerDay", Num),
        ],
        "vns_hrv" => &[
            ("max_intensity", "maxIntensityMilliamps", Num),
            ("max_frequency", "maxFrequencyHz", Num),
            ("max_session_duration", "maxSessionDurationSeconds", Num),
            ("allowed_protocols", "allowedProtocols", List),
        ],
        "audio_entrainment" => &[
            ("max_volume_db", "maxVolumeDb", Num),
            ("max_intensity", "", Retired("max_volume_db")),
            ("max_frequency", "maxBinauralBeatsHz", Num),
            ("max_binaural_beats", "maxBinauralBeatsHz", Num),
            ("max_isochronic_tones", "maxIsochronicTonesHz", Num),
        ],
        "visual_stimulation" => &[
            ("max_frequency", "maxFrequencyHz", Num),
            ("min_frequency", "minFrequencyHz", Num),
            ("allowed_modes", "allowedModes", List),
            ("block_high_risk_range", "blockHighRiskRange", Flag),
        ],
        "tms" => &[
            ("max_intensity_pct_mt", "maxIntensityPercentMT", Num),
            ("max_pulses_per_session", "maxPulsesPerSession", Num),
            ("max_pulses_per_day", "maxPulsesPerDay", Num),
            ("max_sessions_per_week", "maxSessionsPerWeek", Num),
            ("allowed_protocols", "allowedProtocols", List),
            ("allowed_targets", "allowedTargets", List),
        ],
        "pbm_deep_1170nm" => &[
            ("max_intensity", "maxIntensityMWcm2", Num),
            ("max_session_duration", "maxSessionDurationSeconds", Num),
        ],
        "clinical_tacs" | "cervical_vns" => &[
            ("max_intensity", "maxIntensityMilliamps", Num),
            ("max_session_duration", "maxSessionDurationSeconds", Num),
        ],
        "hd_tdcs" => &[
            ("max_intensity", "maxIntensityMilliamps", Num),
            ("max_session_duration", "maxSessionDurationSeconds", Num),
            ("allowed_montages", "allowedMontages", List),
        ],
        "vibrotactile_40hz" => &[
            ("max_intensity", "maxIntensityG", Num),
            ("max_session_duration", "maxSessionDurationSeconds", Num),
        ],
        _ => &[],
    }
}

/// JavaScript `Number(v)` for a parsed value; NaN where JavaScript gives NaN.
fn js_number(v: &Value) -> f64 {
    match v {
        Value::Number(n) => n.as_f64().unwrap_or(f64::NAN),
        Value::Bool(b) => f64::from(u8::from(*b)),
        Value::Null => 0.0,
        Value::String(s) => js_number_of_str(s),
        Value::Array(a) => match a.len() {
            0 => 0.0,
            1 => js_number(&a[0]),
            _ => f64::NAN,
        },
        Value::Object(_) => f64::NAN,
    }
}

fn js_number_of_str(s: &str) -> f64 {
    let t = s.trim();
    if t.is_empty() {
        return 0.0;
    }
    match t {
        "Infinity" | "+Infinity" => return f64::INFINITY,
        "-Infinity" => return f64::NEG_INFINITY,
        _ => {}
    }
    // Rust also accepts "inf", "nan" and "infinity"; JavaScript does not.
    if t.chars().any(|c| !(c.is_ascii_digit() || matches!(c, '.' | 'e' | 'E' | '+' | '-'))) {
        if let Some(h) = t.strip_prefix("0x").or_else(|| t.strip_prefix("0X")) {
            return i64::from_str_radix(h, 16).map(|n| n as f64).unwrap_or(f64::NAN);
        }
        return f64::NAN;
    }
    t.parse::<f64>().unwrap_or(f64::NAN)
}

/// JavaScript truthiness, for `Boolean(v)`.
fn js_truthy(v: &Value) -> bool {
    match v {
        Value::Bool(b) => *b,
        Value::Number(n) => n.as_f64().map(|x| x != 0.0 && !x.is_nan()).unwrap_or(false),
        Value::String(s) => !s.is_empty(),
        Value::Null => false,
        Value::Array(_) | Value::Object(_) => true,
    }
}

/// A number as `JSON.stringify` writes it: NaN and infinities are `null`.
fn json_num(x: f64) -> Value {
    if x.is_finite() { number(x) } else { Value::Null }
}

/// `describeInvalid`: a rejected socket id as it was written.
pub(crate) fn describe_invalid(raw: &Value) -> String {
    match raw {
        Value::String(s) => serde_json::to_string(s).unwrap_or_default(),
        other => value_string(other),
    }
}

/// `coerce` in socketSet.ts: a number, or a string that is a plain decimal integer. NaN otherwise.
fn coerce_socket(v: &Value) -> f64 {
    match v {
        Value::Number(n) => n.as_f64().unwrap_or(f64::NAN),
        Value::String(s) => {
            let t = s.trim();
            let digits = t.strip_prefix(['+', '-']).unwrap_or(t);
            if !digits.is_empty() && digits.chars().all(|c| c.is_ascii_digit()) {
                t.parse::<f64>().unwrap_or(f64::NAN)
            } else {
                f64::NAN
            }
        }
        _ => f64::NAN,
    }
}

/// `toSocketSet`: valid ids deduplicated and ascending, and the invalid inputs as written.
pub(crate) fn to_socket_set(raw: &[Value]) -> (Vec<i64>, Vec<&Value>) {
    let mut sockets: Vec<i64> = Vec::new();
    let mut invalid: Vec<&Value> = Vec::new();
    let mut invalid_seen: Vec<String> = Vec::new();
    for v in raw {
        let n = coerce_socket(v);
        let ok = n.fract() == 0.0 && n.is_finite() && (NP_SOCKET_ID_MIN as f64..=NP_SOCKET_ID_MAX as f64).contains(&n);
        if !ok {
            let kind = match v {
                Value::String(_) => "string",
                Value::Number(_) => "number",
                Value::Bool(_) => "boolean",
                _ => "object",
            };
            let key = format!("{kind}:{}", value_string(v));
            if !invalid_seen.contains(&key) {
                invalid_seen.push(key);
                invalid.push(v);
            }
            continue;
        }
        let n = n as i64;
        if !sockets.contains(&n) {
            sockets.push(n);
        }
    }
    sockets.sort_unstable();
    (sockets, invalid)
}

/// `validateWavelengthRules`.
fn validate_wavelength_rules(channels: &[(String, f64, f64, f64)]) -> Vec<String> {
    let mut errors = Vec::new();
    let mut seen: Vec<&str> = Vec::new();
    for (element, nominal, min, max) in channels {
        if !PBM_CHANNEL_ELEMENTS.contains(&element.as_str()) {
            errors.push(format!("unknown channel '{element}'"));
            continue;
        }
        if seen.contains(&element.as_str()) {
            errors.push(format!("channel '{element}' has more than one rule"));
        }
        seen.push(element);
        for (k, v) in [("nominal_nm", nominal), ("min_nm", min), ("max_nm", max)] {
            if !v.is_finite() || *v <= 0.0 {
                errors.push(format!("channel '{element}': {k} must be a positive number"));
            }
        }
        if min > max {
            errors.push(format!(
                "channel '{element}': min_nm {} is above max_nm {}",
                num_string(*min),
                num_string(*max)
            ));
        }
    }
    errors
}

impl Parser {
    /// The quoted name that opens `zone`, `condition`, `wavelength_rules` and `composite` blocks.
    fn block_name(&mut self, what: &str) -> R<String> {
        self.skip_newlines();
        if self.ct() != Kind::Str {
            return Err(ParseError::at(
                format!("Expected {what} name string, got {}", self.ct().name()),
                self.cur().line,
            ));
        }
        let name = self.cur().text.clone();
        self.advance();
        Ok(name)
    }

    fn open_block(&mut self) -> R<()> {
        self.skip_newlines();
        self.expect(Kind::LBrace)?;
        self.skip_newlines();
        Ok(())
    }

    pub(super) fn parse_composite(&mut self) -> R<Value> {
        let name = self.block_name("composite")?;
        self.open_block()?;

        let mut description = String::new();
        let mut author = "NeurOne".to_string();
        let mut version = "1.0".to_string();
        let mut tags: Vec<String> = Vec::new();
        let mut layers: Vec<Value> = Vec::new();
        let mut conflict_resolution = Value::String("merge".into());
        let mut parsed_id: Option<String> = None;
        let mut read_only: Option<bool> = None;
        let mut conditions: Option<Vec<String>> = None;
        let mut references: Option<Vec<Value>> = None;

        while !self.try_brace() {
            self.skip_newlines();
            if self.ct() == Kind::RBrace {
                break;
            }
            if self.is_word() && self.cur().text == "layer" {
                self.advance();
                layers.push(self.parse_layer_block()?);
                self.skip_newlines();
                continue;
            }
            let key = self.read_key_value()?;
            match key.as_str() {
                "id" => parsed_id = Some(self.read_string()?),
                "description" => description = self.read_string()?,
                "author" => author = self.read_string()?,
                "version" => version = self.read_string()?,
                "readonly" => read_only = Some(self.read_bool()?),
                "tags" => tags = self.read_tag_array()?,
                "conflict_resolution" => conflict_resolution = Value::String(self.read_string()?),
                "conditions" => conditions = Some(self.read_tag_array()?),
                "references" => references = Some(self.read_references()?),
                _ => self.skip_value()?,
            }
            self.skip_newlines();
        }

        let mut c = Map::new();
        if let Some(id) = &parsed_id {
            c.insert("id".into(), json!(id));
        }
        c.insert("name".into(), json!(name));
        c.insert("description".into(), json!(description));
        c.insert("author".into(), json!(author));
        c.insert("version".into(), json!(version));
        c.insert("tags".into(), json!(tags));
        c.insert("isPredefined".into(), json!(parsed_id.is_some()));
        c.insert("layers".into(), Value::Array(layers));
        c.insert("conflictResolution".into(), conflict_resolution);
        if let Some(r) = read_only {
            c.insert("isReadOnly".into(), json!(r));
        }
        if let Some(v) = conditions {
            c.insert("conditions".into(), json!(v));
        }
        if let Some(v) = references {
            c.insert("references".into(), Value::Array(v));
        }
        Ok(Value::Object(c))
    }

    fn parse_layer_block(&mut self) -> R<Value> {
        let protocol_name = self.block_name("layer protocol")?;
        self.open_block()?;
        let mut start = 0.0;
        let mut duration: Option<f64> = None;
        let mut scale = 1.0;
        while !self.try_brace() {
            let key = self.read_key_value()?;
            match key.as_str() {
                "start" => start = self.read_duration_seconds()?,
                "end" => duration = Some(self.read_duration_seconds()? - start),
                "duration" => duration = Some(self.read_duration_seconds()?),
                "intensity_scale" => scale = self.read_number()?,
                _ => self.skip_value()?,
            }
            self.skip_newlines();
        }
        let mut layer = Map::new();
        layer.insert("protocolName".into(), json!(protocol_name));
        layer.insert("startOffsetSeconds".into(), json_num(start));
        layer.insert("intensityScale".into(), json_num(scale));
        if let Some(d) = duration {
            layer.insert("durationSeconds".into(), json_num(d));
        }
        Ok(Value::Object(layer))
    }

    pub(super) fn parse_zone_block(&mut self) -> R<Value> {
        let name = self.block_name("zone")?;
        self.open_block()?;
        let mut zone = Map::new();
        zone.insert("name".into(), json!(name));
        zone.insert("sockets".into(), json!([]));
        zone.insert("isPredefined".into(), json!(false));
        while !self.try_brace() {
            let key = self.read_key_value()?;
            match key.as_str() {
                "id" => {
                    zone.insert("id".into(), json!(self.read_string()?));
                    zone.insert("isPredefined".into(), json!(true));
                }
                "description" => {
                    zone.insert("description".into(), json!(self.read_string()?));
                }
                "sockets" => {
                    let line = self.cur().line;
                    let arr = self.read_generic_array()?;
                    let (sockets, invalid) = to_socket_set(&arr);
                    if !invalid.is_empty() {
                        let listed = invalid.iter().map(|v| describe_invalid(v)).collect::<Vec<_>>().join(", ");
                        return Err(ParseError::at(
                            format!(
                                "zone \"{name}\": {listed} {} on this helmet — ids are whole numbers {NP_SOCKET_ID_MIN}–{NP_SOCKET_ID_MAX} \
                                 ({} sockets, numbered from {NP_SOCKET_NUMBERING_BASE})",
                                if invalid.len() == 1 { "is not a socket" } else { "are not sockets" },
                                NP_SOCKET_ID_MAX - NP_SOCKET_ID_MIN + 1,
                            ),
                            line,
                        ));
                    }
                    zone.insert("sockets".into(), json!(sockets));
                }
                "types" => {
                    let arr = self.read_generic_array()?;
                    zone.insert("types".into(), Value::Array(arr.iter().map(|v| json!(value_string(v))).collect()));
                }
                "exclude_types" => {
                    zone.insert("excludeTypes".into(), json!(self.read_bool()?));
                }
                _ => self.skip_value()?,
            }
            self.skip_newlines();
        }
        Ok(Value::Object(zone))
    }

    pub(super) fn parse_condition_block(&mut self) -> R<Value> {
        let name = self.block_name("condition")?;
        self.open_block()?;
        let mut link = String::new();
        let mut id: Option<String> = None;
        let mut description: Option<String> = None;
        let mut code: Option<String> = None;
        while !self.try_brace() {
            let key = self.read_key_value()?;
            match key.as_str() {
                "link" => link = self.read_string()?,
                "id" => id = Some(self.read_string()?),
                "description" => description = Some(self.read_string()?),
                "code" => code = Some(self.read_string()?),
                _ => self.skip_value()?,
            }
            self.skip_newlines();
        }
        let mut c = Map::new();
        c.insert("name".into(), json!(name));
        c.insert("link".into(), json!(link));
        if let Some(v) = id {
            c.insert("id".into(), json!(v));
        }
        if let Some(v) = description {
            c.insert("description".into(), json!(v));
        }
        if let Some(v) = code {
            c.insert("code".into(), json!(v));
        }
        Ok(Value::Object(c))
    }

    pub(super) fn parse_wavelength_rules_block(&mut self) -> R<Value> {
        let start_line = self.cur().line;
        let name = self.block_name("wavelength_rules")?;
        self.open_block()?;
        let mut level = "global".to_string();
        let mut description: Option<String> = None;
        let mut channels: Vec<(String, f64, f64, f64)> = Vec::new();
        while !self.try_brace() {
            self.skip_newlines();
            if self.ct() == Kind::RBrace {
                break;
            }
            if self.is_word() && self.cur().text == "channel" {
                self.advance();
                channels.push(self.parse_wavelength_channel_rule()?);
                self.skip_newlines();
                continue;
            }
            let key = self.read_key_value()?;
            match key.as_str() {
                "level" => {
                    let line = self.cur().line;
                    let v = value_string(&self.read_any_value()?);
                    if v != "global" && v != "user" {
                        return Err(ParseError::at(
                            format!("wavelength_rules \"{name}\": level must be global or user, got '{v}'"),
                            line,
                        ));
                    }
                    level = v;
                }
                "description" => description = Some(self.read_string()?),
                _ => self.skip_value()?,
            }
            self.skip_newlines();
        }
        let errors = validate_wavelength_rules(&channels);
        if !errors.is_empty() {
            return Err(ParseError::at(
                format!("wavelength_rules \"{name}\": {}", errors.join("; ")),
                start_line,
            ));
        }
        let mut rules = Map::new();
        rules.insert("name".into(), json!(name));
        rules.insert("level".into(), json!(level));
        rules.insert(
            "channels".into(),
            Value::Array(
                channels
                    .iter()
                    .map(|(e, nominal, min, max)| {
                        json!({ "element": e, "nominalNm": number(*nominal), "minNm": number(*min), "maxNm": number(*max) })
                    })
                    .collect(),
            ),
        );
        if let Some(d) = description {
            rules.insert("description".into(), json!(d));
        }
        Ok(Value::Object(rules))
    }

    fn parse_wavelength_channel_rule(&mut self) -> R<(String, f64, f64, f64)> {
        self.skip_newlines();
        let line = self.cur().line;
        if self.ct() != Kind::Str {
            return Err(ParseError::at(
                format!("Expected channel name string (e.g. \"led_808\"), got {}", self.ct().name()),
                line,
            ));
        }
        let element = self.cur().text.clone();
        self.advance();
        if !PBM_CHANNEL_ELEMENTS.contains(&element.as_str()) {
            return Err(ParseError::at(
                format!("unknown channel '{element}' — one of {}", PBM_CHANNEL_ELEMENTS.join(", ")),
                line,
            ));
        }
        self.open_block()?;
        let (mut nominal, mut min, mut max): (Option<f64>, Option<f64>, Option<f64>) = (None, None, None);
        while !self.try_brace() {
            let key = self.read_key_value()?;
            match key.as_str() {
                "nominal_nm" => nominal = Some(self.read_number()?),
                "min_nm" => min = Some(self.read_number()?),
                "max_nm" => max = Some(self.read_number()?),
                _ => self.skip_value()?,
            }
            self.skip_newlines();
        }
        match (nominal, min, max) {
            (Some(n), Some(lo), Some(hi)) => Ok((element, n, lo, hi)),
            _ => Err(ParseError::at(
                format!("channel \"{element}\": nominal_nm, min_nm and max_nm are all required"),
                line,
            )),
        }
    }

    /// A key inside a `limits` block. Two shapes are legal and only one has a colon: scalar fields
    /// (`level: global`) and per-modality sub-blocks (`pbm_transcranial { … }`).
    fn read_limits_key(&mut self) -> R<(String, bool)> {
        self.skip_newlines();
        let t = self.cur().clone();
        if !matches!(t.kind, Kind::Keyword | Kind::Ident) {
            return Err(ParseError::at(
                format!("Expected key, got {} ({})", t.kind.name(), t.value_string()),
                t.line,
            ));
        }
        self.advance();
        self.skip_newlines();
        if self.ct() == Kind::LBrace {
            return Ok((t.text, true));
        }
        self.expect(Kind::Colon)?;
        Ok((t.text, false))
    }

    pub(super) fn parse_limits_block(&mut self) -> R<Value> {
        let start_line = self.cur().line;
        self.skip_newlines();
        let mut name = "Untitled Limits".to_string();
        if matches!(self.ct(), Kind::Str | Kind::Ident) {
            name = self.cur().text.clone();
            self.advance();
        }
        self.open_block()?;

        let mut level = "global".to_string();
        let mut helmet_id = String::new();
        let mut individual_id = String::new();
        let mut description = String::new();
        let mut modalities: Map<String, Value> = Map::new();

        while !self.try_brace() {
            let (key, is_block) = self.read_limits_key()?;
            if is_block != MODALITY_LIMITS_KEYS.contains(&key.as_str()) {
                return Err(ParseError::at(
                    if is_block {
                        format!("'{key}' is a scalar limits field, not a sub-block")
                    } else {
                        format!("'{key}' is a per-modality limits sub-block and takes no ':'")
                    },
                    start_line,
                ));
            }
            match key.as_str() {
                "level" => {
                    let v = self.read_string()?;
                    if matches!(v.as_str(), "global" | "helmet" | "individual") {
                        level = v;
                    }
                }
                "helmet_id" => helmet_id = self.read_string()?,
                "individual_id" => individual_id = self.read_string()?,
                "description" => description = self.read_string()?,
                k if MODALITY_LIMITS_KEYS.contains(&k) => {
                    let sub = self.parse_limits_sub_block(k)?;
                    modalities.insert(camel_modality(k).into(), sub);
                }
                _ => return Err(ParseError::at(format!("Unknown limits key: '{key}'"), start_line)),
            }
            self.skip_newlines();
        }

        let mut out = Map::new();
        out.insert("name".into(), json!(name));
        out.insert("description".into(), json!(description));
        out.insert("level".into(), json!(level));
        if !helmet_id.is_empty() {
            out.insert("helmetId".into(), json!(helmet_id));
        }
        if !individual_id.is_empty() {
            out.insert("individualId".into(), json!(individual_id));
        }
        out.extend(modalities);
        Ok(Value::Object(out))
    }

    fn parse_limits_sub_block(&mut self, modality: &str) -> R<Value> {
        let fields = limits_fields(modality);
        self.open_block()?;
        let mut result = Map::new();
        while !self.try_brace() {
            let key = self.read_key_value()?;
            let raw = self.read_any_value()?;
            if let Some((_, out, conv)) = fields.iter().find(|(k, _, _)| *k == key) {
                match conv {
                    Num => {
                        result.insert((*out).into(), json_num(js_number(&raw)));
                    }
                    List => {
                        let v = if raw.is_array() { raw } else { Value::Array(vec![json!(value_string(&raw))]) };
                        result.insert((*out).into(), v);
                    }
                    Flag => {
                        result.insert((*out).into(), json!(js_truthy(&raw)));
                    }
                    Retired(replacement) => {
                        return Err(ParseError::at(
                            format!(
                                "{modality} limits: max_intensity is a percentage ceiling and is retired; \
                                 write {replacement} in the quantity's own unit (NP-NPPS-REF-001 §7)."
                            ),
                            self.cur().line,
                        ));
                    }
                }
            }
            self.skip_newlines();
        }
        Ok(Value::Object(result))
    }
}

/// The `NPLimitsSet` property a modality's limits sub-block is stored under.
pub(crate) fn camel_modality(key: &str) -> &'static str {
    match key {
        "pbm_transcranial" => "pbmTranscranial",
        "pbm_intranasal" => "pbmIntranasal",
        "eeg_neurofeedback" => "eegNeurofeedback",
        "bes_tacs" => "besTacs",
        "tdcs" => "tdcs",
        "vns_hrv" => "vnsHrv",
        "audio_entrainment" => "audioEntrainment",
        "visual_stimulation" => "visualStimulation",
        "tms" => "tms",
        "pbm_deep_1170nm" => "pbmDeep1170nm",
        "clinical_tacs" => "clinicalTacs",
        "hd_tdcs" => "hdTdcs",
        "cervical_vns" => "cervicalVns",
        _ => "vibrotactile40hz",
    }
}
