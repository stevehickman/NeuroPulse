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

// The wrapping conversions that used to live here (`to_u8`, `to_u16`, `to_u32`: truncate, then reduce modulo 2^8, 2^16
// or 2^32, as `DataView.setUint8` does) are removed. They mirrored the TypeScript compiler this crate replaced, and they
// turned an out-of-range authored value into a different one without a word: 300 Hz became frequency code 44. A wire
// field is now written through `compiler::uint`, which refuses what does not fit (NP-NPPS-REF-001 REQ-NPPS-33 (b)).
// Do not add a wrapping conversion back.
