//! JavaScript-compatible renderings the reference parser's messages and JSON rely on.

use serde_json::{Number, Value};

/// `String(n)` for a finite number: integers without a decimal point.
pub fn num_string(n: f64) -> String {
    if n.is_nan() {
        return "NaN".into();
    }
    if n.fract() == 0.0 && n.abs() < 1e21 {
        return format!("{}", n as i64);
    }
    format!("{n}")
}

/// A JSON number the way `JSON.stringify` writes it: 20, not 20.0.
pub fn number(n: f64) -> Value {
    if n.fract() == 0.0 && n.abs() < 9.0e15 {
        Value::Number(Number::from(n as i64))
    } else {
        Number::from_f64(n).map(Value::Number).unwrap_or(Value::Null)
    }
}

/// `String(v)` for a parsed value.
pub fn value_string(v: &Value) -> String {
    match v {
        Value::String(s) => s.clone(),
        Value::Number(n) => num_string(n.as_f64().unwrap_or(f64::NAN)),
        Value::Bool(b) => b.to_string(),
        Value::Null => "null".into(),
        Value::Array(a) => a.iter().map(value_string).collect::<Vec<_>>().join(","),
        Value::Object(_) => "[object Object]".into(),
    }
}

/// `Math.round`: halves round toward +infinity.
pub fn js_round(x: f64) -> f64 {
    (x + 0.5).floor()
}

/// `DataView.setUint8`'s ToUint8: truncate, then wrap modulo 256.
pub fn to_u8(x: f64) -> u8 {
    if !x.is_finite() { 0 } else { (x.trunc() as i64).rem_euclid(256) as u8 }
}

pub fn to_u16(x: f64) -> u16 {
    if !x.is_finite() { 0 } else { (x.trunc() as i64).rem_euclid(65536) as u16 }
}

pub fn to_u32(x: f64) -> u32 {
    if !x.is_finite() { 0 } else { (x.trunc() as i64).rem_euclid(1i64 << 32) as u32 }
}
