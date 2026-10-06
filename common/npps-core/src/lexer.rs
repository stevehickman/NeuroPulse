//! NPPS lexer. A line-for-line port of `tokenize()` in common/lib/nppsParser.ts: the
//! two must agree on every input, which `tests/differential.rs` checks.

use crate::error::ParseError;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Kind {
    Keyword,
    Ident,
    Str,
    Number,
    Bool,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Colon,
    Comma,
    Newline,
    Eof,
}

impl Kind {
    /// The token type name the reference parser prints in its messages.
    pub fn name(self) -> &'static str {
        match self {
            Kind::Keyword => "KEYWORD",
            Kind::Ident => "IDENT",
            Kind::Str => "STRING",
            Kind::Number => "NUMBER",
            Kind::Bool => "BOOL",
            Kind::LBrace => "LBRACE",
            Kind::RBrace => "RBRACE",
            Kind::LBracket => "LBRACKET",
            Kind::RBracket => "RBRACKET",
            Kind::Colon => "COLON",
            Kind::Comma => "COMMA",
            Kind::Newline => "NEWLINE",
            Kind::Eof => "EOF",
        }
    }
}

#[derive(Debug, Clone)]
pub struct Token {
    pub kind: Kind,
    /// Text of a string, identifier, keyword or punctuation token.
    pub text: String,
    pub num: f64,
    pub boolean: bool,
    pub line: u32,
    /// `s`, `m`, `Hz`, `%`, `mA`, `mW_cm2` or `dB`, kept from the source.
    pub unit: Option<&'static str>,
}

impl Token {
    fn new(kind: Kind, text: &str, line: u32) -> Token {
        Token { kind, text: text.to_string(), num: 0.0, boolean: false, line, unit: None }
    }

    /// `String(token.value)` in the reference parser.
    pub fn value_string(&self) -> String {
        match self.kind {
            Kind::Number => crate::js::num_string(self.num),
            Kind::Bool => self.boolean.to_string(),
            _ => self.text.clone(),
        }
    }
}

const KEYWORDS: &[&str] = &[
    "protocol", "composite", "limits", "zone", "condition", "layer",
    "wavelength_rules", "channel",
    "pbm_transcranial", "pbm_intranasal", "pbm_deep_1170nm", "eeg_neurofeedback",
    "bes_tacs", "tdcs", "vns_hrv", "audio_entrainment", "visual_stimulation",
    "qeeg_21ch", "tms", "clinical_tacs", "hd_tdcs", "cervical_vns",
    "vibrotactile_40hz",
    "id", "description", "author", "version", "readonly", "tags",
    "duration", "interval_count", "conditions", "references",
    "interval_on", "interval_off", "repeat", "enabled",
    "start", "end", "intensity_scale", "conflict_resolution",
    "level", "global", "helmet", "individual", "helmet_id", "individual_id",
    "nominal_nm", "min_nm", "max_nm",
    "sockets", "types", "exclude_types", "link", "code",
    "max_intensity", "max_frequency", "min_frequency", "max_duty_cycle",
    "max_session_dose", "max_daily_dose", "max_session_duration",
    "max_sessions_per_day", "max_sessions_per_week",
    "max_intensity_pct_mt", "max_pulses_per_session", "max_pulses_per_day",
    "max_binaural_beats", "max_isochronic_tones",
    "allowed_modes", "allowed_protocols", "allowed_targets", "allowed_montages",
    "allowed_bands", "require_closed_loop", "block_high_risk_range",
    "true", "false",
];

fn is_alnum_(c: char) -> bool {
    c.is_ascii_alphanumeric() || c == '_'
}

/// `parseFloat` on a run of digits and dots: the longest valid decimal prefix.
fn parse_float_prefix(s: &str) -> f64 {
    let mut end = 0;
    let mut seen_dot = false;
    for (i, c) in s.char_indices() {
        if c == '-' && i == 0 {
            end = i + 1;
        } else if c.is_ascii_digit() {
            end = i + 1;
        } else if c == '.' && !seen_dot {
            seen_dot = true;
            end = i + 1;
        } else {
            break;
        }
    }
    s[..end].trim_end_matches('.').parse::<f64>().unwrap_or(f64::NAN)
}

pub fn tokenize(text: &str) -> Result<Vec<Token>, ParseError> {
    let c: Vec<char> = text.chars().collect();
    let mut tokens = Vec::new();
    let mut pos = 0usize;
    let mut line = 1u32;
    let at = |i: usize| -> Option<char> { c.get(i).copied() };

    while pos < c.len() {
        let ch = c[pos];
        if ch == ' ' || ch == '\t' || ch == '\r' {
            pos += 1;
            continue;
        }
        if ch == '#' {
            while pos < c.len() && c[pos] != '\n' {
                pos += 1;
            }
            continue;
        }
        if ch == '\n' {
            tokens.push(Token::new(Kind::Newline, "\n", line));
            line += 1;
            pos += 1;
            continue;
        }
        let punct = match ch {
            '{' => Some(Kind::LBrace),
            '}' => Some(Kind::RBrace),
            '[' => Some(Kind::LBracket),
            ']' => Some(Kind::RBracket),
            ':' => Some(Kind::Colon),
            ',' => Some(Kind::Comma),
            _ => None,
        };
        if let Some(k) = punct {
            tokens.push(Token::new(k, &ch.to_string(), line));
            pos += 1;
            continue;
        }

        if ch == '"' {
            pos += 1;
            let mut s = String::new();
            while pos < c.len() && c[pos] != '"' {
                if c[pos] == '\\' && pos + 1 < c.len() {
                    match c[pos + 1] {
                        '"' => s.push('"'),
                        '\\' => s.push('\\'),
                        'n' => s.push('\n'),
                        't' => s.push('\t'),
                        other => s.push(other),
                    }
                    pos += 2;
                } else {
                    if c[pos] == '\n' {
                        line += 1;
                    }
                    s.push(c[pos]);
                    pos += 1;
                }
            }
            if pos >= c.len() {
                return Err(ParseError::at("Unterminated string", line));
            }
            pos += 1;
            tokens.push(Token::new(Kind::Str, &s, line));
            continue;
        }

        let digit = |x: Option<char>| x.map_or(false, |d| d.is_ascii_digit());
        if (ch == '-' && digit(at(pos + 1))) || ch.is_ascii_digit() {
            let mut num_str = String::new();
            if ch == '-' {
                num_str.push('-');
                pos += 1;
            }
            while pos < c.len() && (c[pos].is_ascii_digit() || c[pos] == '.') {
                num_str.push(c[pos]);
                pos += 1;
            }
            if num_str == "-" {
                return Err(ParseError::at("Stray minus sign", line));
            }
            let mut unit: Option<&'static str> = None;
            if pos < c.len() {
                let rest = |n: usize| -> String { c[pos..(pos + n).min(c.len())].iter().collect() };
                let next_alnum = |off: usize| at(pos + off).map_or(false, is_alnum_);
                if c[pos] == '%' {
                    unit = Some("%");
                    pos += 1;
                } else if c[pos] == 's' && !next_alnum(1) {
                    unit = Some("s");
                    pos += 1;
                } else if c[pos] == 'm' && !next_alnum(1) {
                    unit = Some("m");
                    pos += 1;
                } else if rest(2) == "Hz" {
                    unit = Some("Hz");
                    pos += 2;
                } else if rest(2) == "mA" {
                    unit = Some("mA");
                    pos += 2;
                } else if rest(6) == "mW_cm2" && !next_alnum(6) {
                    unit = Some("mW_cm2");
                    pos += 6;
                } else if rest(2) == "dB" && !next_alnum(2) {
                    unit = Some("dB");
                    pos += 2;
                }
            }
            if unit.is_none() && pos < c.len() && (c[pos].is_ascii_alphabetic() || c[pos] == '_' || c[pos] == '-') {
                let mut rest = num_str.clone();
                while pos < c.len() && (c[pos].is_ascii_alphanumeric() || c[pos] == '_' || c[pos] == '-') {
                    rest.push(c[pos]);
                    pos += 1;
                }
                return Err(ParseError::at(
                    format!(
                        "Unquoted value '{rest}' — values that start with a digit and are not a plain number must be quoted, e.g. \"{rest}\""
                    ),
                    line,
                ));
            }
            let mut t = Token::new(Kind::Number, "", line);
            t.num = parse_float_prefix(&num_str);
            t.unit = unit;
            tokens.push(t);
            continue;
        }

        if ch.is_ascii_alphabetic() || ch == '_' {
            let mut ident = String::new();
            while pos < c.len() && is_alnum_(c[pos]) {
                ident.push(c[pos]);
                pos += 1;
            }
            if ident == "true" || ident == "false" {
                let mut t = Token::new(Kind::Bool, &ident, line);
                t.boolean = ident == "true";
                tokens.push(t);
            } else if KEYWORDS.contains(&ident.as_str()) {
                tokens.push(Token::new(Kind::Keyword, &ident, line));
            } else {
                tokens.push(Token::new(Kind::Ident, &ident, line));
            }
            continue;
        }

        return Err(ParseError::at(format!("Unexpected character: {ch}"), line));
    }
    tokens.push(Token::new(Kind::Eof, "", line));
    Ok(tokens)
}
