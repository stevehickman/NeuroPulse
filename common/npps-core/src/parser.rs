//! NPPS parser. A port of the `protocol` path of `Parser` in common/lib/nppsParser.ts,
//! with the per-modality knowledge read from the field table instead of written in code.
//! Messages are copied verbatim, because a refusal must read the same on every runtime.

use crate::error::ParseError;
use crate::js::{num_string, number, value_string};
use crate::lexer::{tokenize, Kind, Token};
use crate::table::table;
use serde_json::{json, Map, Value};

type R<T> = Result<T, ParseError>;

mod blocks;

/// An entry of the file's protocol list, as the normalised JSON the web parser's output reduces to.
#[derive(Debug, Clone, PartialEq)]
pub enum Entry {
    /// A `protocol`.
    Single(Value),
    /// A `composite`.
    Composite(Value),
}

/// Everything one `.npps` file declares, in the order `parseNPPSFile` and `parseNPPSLimits` give it:
/// the protocol entries, plus the zones, conditions, wavelength rules and limits sets the file holds.
/// Ids and timestamps the reference generates per parse are left to the caller.
#[derive(Debug, Clone, PartialEq, Default)]
pub struct ParsedFile {
    pub entries: Vec<Entry>,
    pub zones: Vec<Value>,
    pub conditions: Vec<Value>,
    pub wavelength_rules: Vec<Value>,
    pub limits: Vec<Value>,
}

/// The protocol entries of a file. A block that is not a protocol or composite is still parsed, and
/// refuses on the message the reference gives; use `parse_file` to keep what it declares.
pub fn parse_npps(text: &str) -> R<Vec<Entry>> {
    Ok(parse_file(text)?.entries)
}

pub fn parse_file(text: &str) -> R<ParsedFile> {
    let mut p = Parser { toks: tokenize(text)?, pos: 0, out: ParsedFile::default() };
    p.parse()?;
    Ok(p.out)
}

struct Parser {
    toks: Vec<Token>,
    pos: usize,
    out: ParsedFile,
}

impl Parser {
    fn cur(&self) -> &Token {
        &self.toks[self.pos]
    }
    fn ct(&self) -> Kind {
        self.toks[self.pos].kind
    }
    fn advance(&mut self) -> Token {
        let t = self.toks[self.pos].clone();
        if t.kind != Kind::Eof {
            self.pos += 1;
        }
        t
    }
    fn skip_newlines(&mut self) {
        while self.ct() == Kind::Newline {
            self.advance();
        }
    }
    fn expect(&mut self, kind: Kind) -> R<Token> {
        self.skip_newlines();
        let t = self.cur().clone();
        if t.kind != kind {
            return Err(ParseError::at(
                format!("Expected {}, got {} ({})", kind.name(), t.kind.name(), t.value_string()),
                t.line,
            ));
        }
        Ok(self.advance())
    }
    fn is_word(&self) -> bool {
        matches!(self.ct(), Kind::Keyword | Kind::Ident)
    }
    fn try_keyword(&mut self, name: &str) -> bool {
        self.skip_newlines();
        if self.is_word() && self.cur().text == name {
            self.advance();
            return true;
        }
        false
    }
    fn try_brace(&mut self) -> bool {
        self.skip_newlines();
        if self.ct() == Kind::RBrace {
            self.advance();
            return true;
        }
        false
    }

    fn read_string(&mut self) -> R<String> {
        self.skip_newlines();
        let t = self.cur().clone();
        if matches!(t.kind, Kind::Str | Kind::Ident | Kind::Keyword) {
            self.advance();
            return Ok(t.text);
        }
        Err(ParseError::at(format!("Expected string, got {}", t.kind.name()), t.line))
    }
    fn read_number(&mut self) -> R<f64> {
        self.skip_newlines();
        let t = self.cur().clone();
        if t.kind != Kind::Number {
            return Err(ParseError::at(
                format!("Expected number, got {} ({})", t.kind.name(), t.value_string()),
                t.line,
            ));
        }
        self.advance();
        Ok(t.num)
    }
    fn read_bool(&mut self) -> R<bool> {
        self.skip_newlines();
        let t = self.cur().clone();
        if t.kind == Kind::Bool {
            self.advance();
            return Ok(t.boolean);
        }
        if matches!(t.kind, Kind::Ident | Kind::Keyword) {
            if t.text == "true" {
                self.advance();
                return Ok(true);
            }
            if t.text == "false" {
                self.advance();
                return Ok(false);
            }
        }
        Err(ParseError::at(format!("Expected bool, got {} ({})", t.kind.name(), t.value_string()), t.line))
    }

    fn read_tag_array(&mut self) -> R<Vec<String>> {
        self.skip_newlines();
        self.expect(Kind::LBracket)?;
        let mut arr = Vec::new();
        self.skip_newlines();
        while self.ct() != Kind::RBracket && self.ct() != Kind::Eof {
            let t = self.cur().clone();
            match t.kind {
                Kind::Str | Kind::Ident | Kind::Keyword => {
                    arr.push(t.text.clone());
                    self.advance();
                }
                Kind::Number => {
                    arr.push(format!("{}{}", num_string(t.num), t.unit.unwrap_or("")));
                    self.advance();
                }
                _ => {
                    return Err(ParseError::at(format!("Expected tag value, got {}", t.kind.name()), t.line));
                }
            }
            self.skip_newlines();
            if self.ct() == Kind::Comma {
                self.advance();
                self.skip_newlines();
            }
        }
        self.expect(Kind::RBracket)?;
        Ok(arr)
    }

    fn read_references(&mut self) -> R<Vec<Value>> {
        let arr = self.read_generic_array()?;
        let mut out = Vec::new();
        for el in arr {
            match &el {
                Value::String(_) => out.push(el),
                Value::Array(a) if a.len() == 2 => {
                    out.push(json!({ "label": value_string(&a[0]), "url": value_string(&a[1]) }))
                }
                Value::Array(a) if a.len() == 1 => out.push(Value::String(value_string(&a[0]))),
                _ => {}
            }
        }
        Ok(out)
    }

    fn read_duration_seconds(&mut self) -> R<f64> {
        self.skip_newlines();
        let t = self.cur().clone();
        if t.kind != Kind::Number {
            return Err(ParseError::at(format!("Expected duration, got {}", t.kind.name()), t.line));
        }
        self.advance();
        Ok(if t.unit == Some("m") { t.num * 60.0 } else { t.num })
    }

    fn skip_value(&mut self) -> R<()> {
        self.skip_newlines();
        match self.ct() {
            Kind::LBracket => {
                self.read_generic_array()?;
            }
            Kind::LBrace => {
                self.read_generic_object()?;
            }
            Kind::Str | Kind::Number | Kind::Bool | Kind::Ident | Kind::Keyword => {
                self.advance();
            }
            _ => {}
        }
        Ok(())
    }

    fn read_any_value(&mut self) -> R<Value> {
        self.skip_newlines();
        let t = self.cur().clone();
        match t.kind {
            Kind::Str => {
                self.advance();
                Ok(Value::String(t.text))
            }
            Kind::Number => {
                self.advance();
                Ok(number(t.num))
            }
            Kind::Bool => {
                self.advance();
                Ok(Value::Bool(t.boolean))
            }
            Kind::Ident | Kind::Keyword => {
                self.advance();
                Ok(Value::String(t.text))
            }
            Kind::LBracket => Ok(Value::Array(self.read_generic_array()?)),
            Kind::LBrace => Ok(Value::Object(self.read_generic_object()?)),
            _ => Err(ParseError::at(format!("Unexpected token {} in value position", t.kind.name()), t.line)),
        }
    }

    fn read_generic_array(&mut self) -> R<Vec<Value>> {
        self.expect(Kind::LBracket)?;
        let mut arr = Vec::new();
        self.skip_newlines();
        while self.ct() != Kind::RBracket && self.ct() != Kind::Eof {
            arr.push(self.read_any_value()?);
            self.skip_newlines();
            if self.ct() == Kind::Comma {
                self.advance();
                self.skip_newlines();
            }
        }
        self.expect(Kind::RBracket)?;
        Ok(arr)
    }

    fn read_key_value(&mut self) -> R<String> {
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
        self.expect(Kind::Colon)?;
        Ok(t.text)
    }

    fn read_generic_object(&mut self) -> R<Map<String, Value>> {
        self.expect(Kind::LBrace)?;
        self.skip_newlines();
        let mut obj = Map::new();
        while self.ct() != Kind::RBrace && self.ct() != Kind::Eof {
            let key = self.read_key_value()?;
            let v = self.read_any_value()?;
            obj.insert(key, v);
            self.skip_newlines();
            if self.ct() == Kind::Comma {
                self.advance();
                self.skip_newlines();
            }
        }
        self.expect(Kind::RBrace)?;
        Ok(obj)
    }

    // ── top level ────────────────────────────────────────────────────────────

    fn parse(&mut self) -> R<()> {
        self.skip_newlines();
        while self.ct() != Kind::Eof {
            self.skip_newlines();
            if self.ct() == Kind::Eof {
                break;
            }
            if self.try_keyword("protocol") {
                let p = self.parse_protocol()?;
                self.out.entries.push(Entry::Single(p));
            } else if self.try_keyword("composite") {
                let c = self.parse_composite()?;
                self.out.entries.push(Entry::Composite(c));
            } else if self.try_keyword("limits") {
                let l = self.parse_limits_block()?;
                self.out.limits.push(l);
            } else if self.try_keyword("zone") {
                let z = self.parse_zone_block()?;
                self.out.zones.push(z);
            } else if self.try_keyword("condition") {
                let c = self.parse_condition_block()?;
                self.out.conditions.push(c);
            } else if self.try_keyword("wavelength_rules") {
                let w = self.parse_wavelength_rules_block()?;
                self.out.wavelength_rules.push(w);
            } else {
                let t = self.cur();
                return Err(ParseError::at(
                    format!(
                        "Expected 'protocol', 'composite', 'limits', 'zone', 'condition', or 'wavelength_rules', got '{}'",
                        t.value_string()
                    ),
                    t.line,
                ));
            }
            self.skip_newlines();
        }
        Ok(())
    }

    fn parse_protocol(&mut self) -> R<Value> {
        self.skip_newlines();
        if self.ct() != Kind::Str {
            return Err(ParseError::at(
                format!("Expected protocol name string, got {}", self.ct().name()),
                self.cur().line,
            ));
        }
        let name = self.cur().text.clone();
        self.advance();
        self.skip_newlines();
        self.expect(Kind::LBrace)?;

        let mut description = String::new();
        let mut author = "NeurOne".to_string();
        let mut version = "1.0".to_string();
        let mut tags: Vec<String> = Vec::new();
        let mut timing = json!({ "type": "duration", "seconds": 1200 });
        let mut modalities: Vec<Value> = Vec::new();
        let mut parsed_id: Option<String> = None;
        let mut read_only: Option<bool> = None;
        let mut conditions: Option<Vec<String>> = None;
        let mut references: Option<Vec<Value>> = None;

        self.skip_newlines();
        while !self.try_brace() {
            self.skip_newlines();
            if self.ct() == Kind::RBrace {
                break;
            }
            let key_tok = self.cur().clone();
            if !matches!(key_tok.kind, Kind::Keyword | Kind::Ident) {
                return Err(ParseError::at(
                    format!("Expected key, got {} ({})", key_tok.kind.name(), key_tok.value_string()),
                    key_tok.line,
                ));
            }
            let key = key_tok.text.clone();
            self.advance();
            self.skip_newlines();

            if self.ct() == Kind::LBrace {
                modalities.push(self.parse_typed_modality_block(&key)?);
                self.skip_newlines();
                continue;
            }

            self.expect(Kind::Colon)?;
            match key.as_str() {
                "id" => parsed_id = Some(self.read_string()?),
                "description" => description = self.read_string()?,
                "author" => author = self.read_string()?,
                "version" => version = self.read_string()?,
                "readonly" => read_only = Some(self.read_bool()?),
                "tags" => tags = self.read_tag_array()?,
                "duration" => timing = json!({ "type": "duration", "seconds": number(self.read_duration_seconds()?) }),
                "interval_count" => timing = json!({ "type": "interval_count", "count": number(self.read_number()?) }),
                "conditions" => conditions = Some(self.read_tag_array()?),
                "references" => references = Some(self.read_references()?),
                _ => self.skip_value()?,
            }
            self.skip_newlines();
        }

        let mut proto = Map::new();
        if let Some(id) = &parsed_id {
            proto.insert("id".into(), json!(id));
        }
        proto.insert("name".into(), json!(name));
        proto.insert("description".into(), json!(description));
        proto.insert("author".into(), json!(author));
        proto.insert("version".into(), json!(version));
        proto.insert("tags".into(), json!(tags));
        proto.insert("isPredefined".into(), json!(parsed_id.is_some()));
        proto.insert("timingMode".into(), timing);
        proto.insert("modalities".into(), Value::Array(modalities));
        if let Some(r) = read_only {
            proto.insert("isReadOnly".into(), json!(r));
        }
        if let Some(c) = conditions {
            proto.insert("conditions".into(), json!(c));
        }
        if let Some(r) = references {
            proto.insert("references".into(), Value::Array(r));
        }
        Ok(Value::Object(proto))
    }

    // ── modality blocks ──────────────────────────────────────────────────────

    /// An absolute quantity (`irradiance: 300mW_cm2`, `volume: 72dB`), or `None` when `key`
    /// is not one for this modality. A percentage that used to carry one is refused.
    fn read_absolute_quantity(&mut self, type_name: &str, key: &str, line: u32) -> R<Option<(String, f64)>> {
        let tbl = table();
        let pbm = type_name == "pbm_transcranial" || type_name == "pbm_intranasal";
        if tbl.percent_refused(key, type_name) {
            let fix = if type_name == "audio_entrainment" {
                "state the level in dB SPL, e.g. volume: 72dB"
            } else if type_name == "visual_stimulation" {
                "visual_stimulation has no percentage intensity, and no absolute one is defined yet — remove the field"
            } else {
                "state irradiance in mW/cm², e.g. irradiance: 300mW_cm2"
            };
            let _ = pbm;
            return Err(ParseError::at(
                format!(
                    "{type_name}: '{key}' is a percentage of a baseline the hardware owns and can change, so it does not state the stimulus. {fix} (NP-NPPS-REF-001 §4.1b)."
                ),
                line,
            ));
        }
        let Some(spec) = tbl.quantity_for(type_name, key) else { return Ok(None) };
        let unit = spec["unit"].as_str().unwrap_or("");
        let canonical = spec["key"].as_str().unwrap_or("").to_string();
        let via_suffix = spec["short"] == key;
        self.skip_newlines();
        let t = self.cur().clone();
        if t.kind != Kind::Number {
            let suffix = if via_suffix { format!(" with unit {unit}") } else { String::new() };
            return Err(ParseError::at(format!("{type_name}: {key} must be a number{suffix}"), line));
        }
        self.advance();
        match t.unit {
            Some("%") => {
                return Err(ParseError::at(format!("{type_name}: {key} in % is refused — state it in {unit}"), line));
            }
            Some(u) if u != unit => {
                return Err(ParseError::at(format!("{type_name}: {key} takes {unit}, not {u}"), line));
            }
            None if via_suffix => {
                return Err(ParseError::at(
                    format!(
                        "{type_name}: {key} needs its unit written ({key}: {}{unit}); a bare number does not say what it measures",
                        num_string(t.num)
                    ),
                    line,
                ));
            }
            _ => {}
        }
        Ok(Some((canonical, t.num)))
    }

    fn parse_typed_modality_block(&mut self, type_name: &str) -> R<Value> {
        self.skip_newlines();
        self.expect(Kind::LBrace)?;
        self.skip_newlines();

        let mut enabled = true;
        let mut on = 0.0f64;
        let mut off = 0.0f64;
        let mut repeat: Option<f64> = None;
        let mut start: Option<f64> = None;
        let mut raw: Map<String, Value> = Map::new();
        let tbl = table();
        // `__intensity` is held outside `raw` so it cannot collide with a real key.
        let mut intensity: Option<Value> = None;

        while !self.try_brace() {
            self.skip_newlines();
            if self.ct() == Kind::RBrace {
                break;
            }
            let key_tok = self.cur().clone();
            if !matches!(key_tok.kind, Kind::Keyword | Kind::Ident) {
                return Err(ParseError::at(
                    format!("Expected field name, got {} ({})", key_tok.kind.name(), key_tok.value_string()),
                    key_tok.line,
                ));
            }
            let key = key_tok.text.clone();
            self.advance();
            self.skip_newlines();
            self.expect(Kind::Colon)?;

            match key.as_str() {
                "interval_on" => {
                    on = self.read_duration_seconds()?;
                    self.skip_newlines();
                    continue;
                }
                "interval_off" => {
                    off = self.read_duration_seconds()?;
                    self.skip_newlines();
                    continue;
                }
                "start" => {
                    let s = self.read_duration_seconds()?;
                    if !s.is_finite() || s < 0.0 {
                        return Err(ParseError::at(
                            format!("{type_name}: start must be a duration of zero or more"),
                            key_tok.line,
                        ));
                    }
                    start = Some(s);
                    self.skip_newlines();
                    continue;
                }
                "repeat" => {
                    let t = self.cur().clone();
                    if matches!(t.kind, Kind::Ident | Kind::Keyword) && t.text == "until_end" {
                        self.advance();
                        repeat = None;
                    } else {
                        repeat = Some(self.read_number()?);
                    }
                    self.skip_newlines();
                    continue;
                }
                "enabled" => {
                    enabled = self.read_bool()?;
                    self.skip_newlines();
                    continue;
                }
                _ => {}
            }

            if let Some((canonical, value)) = self.read_absolute_quantity(type_name, &key, key_tok.line)? {
                raw.insert(canonical, number(value));
                self.skip_newlines();
                continue;
            }

            if key == "intensity" {
                intensity = Some(self.read_any_value()?);
                self.skip_newlines();
                continue;
            }

            let canonical = tbl.alias(&key).unwrap_or(&key).to_string();
            let val = self.read_any_value()?;

            if canonical == "noise_type" && value_string(&val) == "none" {
                self.skip_newlines();
                continue;
            }
            if !(canonical != key && raw.contains_key(&canonical)) {
                raw.insert(canonical, val);
            }
            self.skip_newlines();
        }

        if let Some(v) = intensity {
            match tbl.intensity_target(type_name) {
                Some(target) => {
                    raw.insert(target.to_string(), v);
                }
                None => {
                    return Err(ParseError::at(
                        format!("{type_name}: 'intensity' is not a field of this modality"),
                        self.cur().line,
                    ));
                }
            }
        }

        if tbl.modality(type_name).is_none() {
            return Err(ParseError::at(format!("Unknown modality block type: '{type_name}'"), self.cur().line));
        }

        let params = build_params(type_name, &raw, self.cur().line)?;
        let mut interval = Map::new();
        interval.insert("intervalOnSeconds".into(), number(on));
        interval.insert("intervalOffSeconds".into(), number(off));
        if let Some(r) = repeat {
            interval.insert("repeatCount".into(), number(r));
        }
        if let Some(s) = start {
            if s > 0.0 {
                interval.insert("startOffsetSeconds".into(), number(s));
            }
        }
        Ok(json!({
            "type": type_name,
            "enabled": enabled,
            "params": Value::Object(params),
            "interval": Value::Object(interval),
        }))
    }
}

// ── building a modality's params from the field table ───────────────────────

/// A default from the field table, written the way `JSON.stringify` would (1, not 1.0).
fn norm(v: Value) -> Value {
    match v {
        Value::Number(n) => number(n.as_f64().unwrap_or(f64::NAN)),
        Value::Array(a) => Value::Array(a.into_iter().map(norm).collect()),
        other => other,
    }
}

fn num_of(raw: &Map<String, Value>, key: &str) -> Option<f64> {
    raw.get(key).and_then(Value::as_f64)
}

fn build_params(type_name: &str, raw: &Map<String, Value>, line: u32) -> R<Map<String, Value>> {
    let spec = table().modality(type_name).expect("checked by the caller");
    let fields = spec["fields"].as_array().expect("fields is an array");
    let mut out = Map::new();

    let find_kind = |kind: &str| fields.iter().find(|f| f["kind"] == kind);

    for f in fields {
        let key = f["key"].as_str().unwrap_or("");
        let json_name = f["json"].as_str().unwrap_or("");
        let default_owned = f.get("default").cloned().map(norm);
        let default = default_owned.as_ref();
        match f["kind"].as_str().unwrap_or("") {
            "number" => {
                let v = num_of(raw, key).map(number).or_else(|| default.cloned()).unwrap_or(Value::Null);
                out.insert(json_name.into(), v);
            }
            "string" => {
                let v = match raw.get(key) {
                    Some(Value::String(s)) => Value::String(s.clone()),
                    _ => default.cloned().unwrap_or(Value::Null),
                };
                out.insert(json_name.into(), v);
            }
            "bool" => {
                let v = match raw.get(key) {
                    Some(Value::Bool(b)) => Value::Bool(*b),
                    _ => default.cloned().unwrap_or(Value::Null),
                };
                out.insert(json_name.into(), v);
            }
            "optional_number" => {
                if let Some(n) = num_of(raw, key) {
                    out.insert(json_name.into(), number(n));
                }
            }
            "optional_string" => {
                if let Some(Value::String(s)) = raw.get(key) {
                    if !s.is_empty() {
                        out.insert(json_name.into(), Value::String(s.clone()));
                    }
                }
            }
            "string_array_optional" => {
                if let Some(Value::Array(a)) = raw.get(key) {
                    out.insert(json_name.into(), Value::Array(a.clone()));
                }
            }
            "pairs" => {
                let v = match raw.get(key) {
                    Some(Value::Array(a)) => Value::Array(
                        a.iter()
                            .map(|p| match p {
                                Value::Array(pa) if pa.len() >= 2 => {
                                    json!([value_string(&pa[0]), value_string(&pa[1])])
                                }
                                _ => json!(["Fp1", "Fp2"]),
                            })
                            .collect(),
                    ),
                    _ => default.cloned().unwrap_or(Value::Null),
                };
                out.insert(json_name.into(), v);
            }
            "quantity" => {
                let what = match f.get("what").and_then(Value::as_str) {
                    Some(w) => w.to_string(),
                    None => format!(
                        "{} (e.g. {})",
                        f["short"].as_str().unwrap_or(key),
                        f["example"].as_str().unwrap_or("")
                    ),
                };
                match num_of(raw, key) {
                    Some(v) if v.is_finite() && v > 0.0 => {
                        out.insert(json_name.into(), number(v));
                    }
                    _ => {
                        return Err(ParseError::at(
                            format!("{type_name}: {what} is required and must be positive"),
                            line,
                        ));
                    }
                }
            }
            "wavelength" => {
                let Some(Value::String(w)) = raw.get("wavelength") else {
                    return Err(ParseError::at(
                        format!("{type_name}: wavelength is required, one value per block, e.g. wavelength: \"810nm\""),
                        line,
                    ));
                };
                if let Some(rep) = table().retired_wavelength(w) {
                    let blocks = rep.iter().map(|x| format!("\"{x}\"")).collect::<Vec<_>>().join(" and ");
                    return Err(ParseError::at(
                        format!(
                            "{type_name}: wavelength \"{w}\" is retired: it welded independent emitters into one block. Write one block per wavelength ({blocks}), each with its own irradiance_mw_cm2."
                        ),
                        line,
                    ));
                }
                out.insert(json_name.into(), Value::String(w.clone()));
            }
            "zones" => {
                let (zones, refs) = build_zones(raw.get("zones"), default, line)?;
                out.insert(json_name.into(), Value::String(zones));
                if let Some(r) = refs {
                    out.insert("zoneRefs".into(), Value::Array(r));
                }
            }
            "pulse_frequency" => {
                let duty_field = find_kind("pulse_duty").expect("a pulse_frequency has a pulse_duty");
                let def_f = default.and_then(Value::as_f64).unwrap_or(0.0);
                let def_d = duty_field["default"].as_f64().unwrap_or(0.0);
                let freq = num_of(raw, "frequency_hz").unwrap_or(def_f);
                let mut duty = num_of(raw, "duty_cycle_percent").unwrap_or(def_d);
                if freq == 0.0 {
                    if let Some(d) = num_of(raw, "duty_cycle_percent") {
                        if d != 100.0 {
                            return Err(ParseError::at(
                                format!(
                                    "{type_name}: frequency: 0 selects continuous wave, which has no duty cycle, but duty_cycle is {}%. Remove duty_cycle for CW, or give a pulse frequency above 0 for a pulsed train",
                                    num_string(d)
                                ),
                                line,
                            ));
                        }
                    }
                    duty = 100.0;
                }
                out.insert(json_name.into(), number(freq));
                out.insert(duty_field["json"].as_str().unwrap_or("dutyCyclePercent").into(), number(duty));
            }
            "pulse_duty" => {}
            other => panic!("unknown field kind {other} in common/npps/fields.json"),
        }
    }
    Ok(out)
}

fn build_zones(raw: Option<&Value>, default: Option<&Value>, line: u32) -> R<(String, Option<Vec<Value>>)> {
    let d = default.cloned().unwrap_or(Value::Null);
    let mut zones = d["zones"].as_str().unwrap_or("named").to_string();
    let mut refs: Option<Vec<Value>> =
        if zones == "named" { d["zoneRefs"].as_array().cloned() } else { None };
    match raw {
        Some(Value::Array(els)) => {
            if !els.iter().all(Value::is_string) {
                return Err(ParseError::at(
                    "pbm_transcranial: zones must be a list of quoted zone names, e.g. zones: [\"Frontal Left\", \"Frontal Right\"]",
                    line,
                ));
            }
            if els.is_empty() {
                return Err(ParseError::at(
                    "pbm_transcranial: zones: [] names no zone — a session cannot target nothing",
                    line,
                ));
            }
            zones = "named".into();
            refs = Some(els.clone());
        }
        Some(other) => {
            if other.as_str() != Some("clinician_selected") {
                return Err(ParseError::at(
                    format!(
                        "pbm_transcranial: unknown zone selector '{}'. zones must be a list of quoted zone names (e.g. zones: [\"Frontal\"]) or clinician_selected",
                        value_string(other)
                    ),
                    line,
                ));
            }
            // Faithful to the reference parser: it sets the discriminant and leaves the
            // default `zoneRefs` in place. Recorded as a finding under OI-NPPS-CORE-01.
            zones = "clinician_selected".into();
        }
        None => {}
    }
    Ok((zones, refs))
}
