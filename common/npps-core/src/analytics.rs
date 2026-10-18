//! Which analytics events may leave the phone (`REQ-SW03-07`; NP-APP-TELEMETRY-001 §3 and §7).
//!
//! An analytics vendor must learn nothing about the person (`CLAUDE.md` §5.1: when in doubt it is UHDR). The telemetry policy
//! says so as an allowlist, nine permitted properties each with the form of its value. The apps used to enforce a list of
//! twelve keys to forbid instead, which lets through any key nobody thought of. The rule is here, once, so that iOS and Android
//! cannot disagree about it, and it reads the one data file `common/npps/analytics-properties.json`.
//!
//! An event is permitted only if (1) its name is lowercase snake_case of at most the stated length and contains none of the
//! forbidden terms of the policy's §7, and (2) every property is named in the file and its value is of one of that property's
//! forms. One property or value outside that drops the whole event: a clamped event is a different event from the one written.
//! The answer names the first offending property by its key and never repeats a value, so a refusal logged by a caller carries
//! nothing the vendor was not to have.
//!
//! | operation | arguments | result |
//! |-----------|-----------|--------|
//! | `checkAnalyticsEvent` | `event` (text), `properties` (object of text) | `{ok: true}`, or `{ok: false, reason, key?}` with `reason` one of `malformed` (not text), `event_name`, `property_not_permitted` (`key`), `value_not_permitted` (`key`) |

use serde_json::{json, Map, Value};

type R<T> = Result<T, String>;

/// The permitted events, as data (`common/npps/analytics-properties.json`).
const POLICY: &str = include_str!("../../npps/analytics-properties.json");

fn policy() -> Value {
    serde_json::from_str(POLICY).expect("common/npps/analytics-properties.json is valid JSON")
}

pub(crate) fn op(op: &str, req: &Value) -> Option<R<Value>> {
    match op {
        "checkAnalyticsEvent" => Some(Ok(check(req))),
        _ => None,
    }
}

fn refusal(reason: &str, key: Option<&str>) -> Value {
    match key {
        Some(k) => json!({ "ok": false, "reason": reason, "key": k }),
        None => json!({ "ok": false, "reason": reason }),
    }
}

fn check(req: &Value) -> Value {
    let (Some(event), Some(props)) = (req["event"].as_str(), req["properties"].as_object()) else {
        return refusal("malformed", None);
    };
    let p = policy();
    if !event_name_ok(event, &p["eventName"]) {
        return refusal("event_name", None);
    }
    for (key, value) in props {
        let Some(forms) = p["properties"][key.as_str()].as_array() else {
            return refusal("property_not_permitted", Some(key));
        };
        let Some(text) = value.as_str() else {
            return refusal("malformed", None);
        };
        if !forms.iter().any(|f| form_matches(f, text)) {
            return refusal("value_not_permitted", Some(key));
        }
    }
    json!({ "ok": true })
}

/// Lowercase snake_case, starting with a letter, within the length, and free of every forbidden term.
fn event_name_ok(name: &str, rule: &Value) -> bool {
    let max = rule["snake"]["max"].as_u64().unwrap_or(0) as usize;
    let mut chars = name.chars();
    let shaped = !name.is_empty()
        && name.len() <= max
        && chars.next().is_some_and(|c| c.is_ascii_lowercase())
        && name.chars().all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == '_');
    shaped
        && !rule["forbiddenTerms"]
            .as_array()
            .into_iter()
            .flatten()
            .filter_map(Value::as_str)
            .any(|term| name.contains(&term.to_ascii_lowercase()))
}

/// 1 to `max` ASCII digits, and nothing else.
fn digits_ok(s: &str, max: usize) -> bool {
    !s.is_empty() && s.len() <= max && s.bytes().all(|b| b.is_ascii_digit())
}

fn form_matches(form: &Value, text: &str) -> bool {
    let Some(f) = form.as_object() else { return false };
    if let Some(lits) = f.get("literal").and_then(Value::as_array) {
        return lits.iter().filter_map(Value::as_str).any(|l| l == text);
    }
    if let Some(d) = f.get("digits") {
        return digits_ok(text, d["max"].as_u64().unwrap_or(0) as usize);
    }
    if let Some(d) = f.get("prefixDigits") {
        let prefix = d["prefix"].as_str().unwrap_or("\u{0}");
        return text.strip_prefix(prefix).is_some_and(|rest| digits_ok(rest, d["max"].as_u64().unwrap_or(0) as usize));
    }
    if let Some(d) = f.get("dotted") {
        return dotted_ok(text, d);
    }
    false
}

/// `prefixes` (one of them and a space, or none listed), then `parts` numbers joined by dots, each at most `max` digits.
fn dotted_ok(text: &str, d: &Value) -> bool {
    let prefixes: Vec<&str> = d["prefixes"].as_array().into_iter().flatten().filter_map(Value::as_str).collect();
    let numbers = if prefixes.is_empty() {
        Some(text)
    } else {
        prefixes.iter().find_map(|p| text.strip_prefix(*p).and_then(|rest| rest.strip_prefix(' ')))
    };
    let (parts, max) = (d["parts"].as_u64().unwrap_or(0) as usize, d["max"].as_u64().unwrap_or(0) as usize);
    numbers.is_some_and(|n| {
        let segs: Vec<&str> = n.split('.').collect();
        segs.len() == parts && segs.iter().all(|s| digits_ok(s, max))
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ask(event: &str, props: Value) -> Value {
        check(&json!({ "event": event, "properties": props }))
    }

    fn ok(event: &str, props: Value) -> bool {
        ask(event, props)["ok"] == json!(true)
    }

    // @verifies REQ-SW03-07
    #[test]
    fn every_property_the_telemetry_policy_permits_is_accepted_with_a_value_of_its_form() {
        assert!(ok(
            "app_opened",
            json!({
                "app_version": "1.0.3", "os_version": "iOS 18.2", "device_class": "iPhone_flagship",
                "screen_name": "protocol_detail", "engagement_tier": "established", "app_start_type": "cold_start",
                "onboarding_step": "consent_flow_step_1", "error_code": "42", "connectivity_type": "wifi",
            })
        ));
        assert!(ok("app_foregrounded", json!({})), "an event with no properties carries nothing and is permitted");
        assert!(ok("e1", json!({ "onboarding_step": "consent_flow_complete" })));
        assert!(ok("e2", json!({ "app_version": "12.34.567", "os_version": "Android 15.0" })));
    }

    // @verifies REQ-SW03-07
    #[test]
    fn a_property_the_policy_does_not_name_drops_the_event_and_the_answer_names_its_key_only() {
        // The twelve keys the old denylist knew, and ones it never thought of.
        for key in [
            "eeg", "hrv", "rmssd", "coherence", "session_id", "protocol_id", "session_count", "session_sequence", "imp",
            "impedance", "impedance_flags", "pass_flags", "heart_rate", "alpha_power", "electrode_site", "condition", "email",
            "duration_s", "timestamp", "idfa",
        ] {
            let r = ask("evt", json!({ "engagement_tier": "new", key: "1" }));
            assert_eq!(r["ok"], json!(false), "{key}");
            assert_eq!(r["reason"], json!("property_not_permitted"));
            assert_eq!(r["key"], json!(key));
            assert!(!r.to_string().contains("\"1\""), "a refusal never repeats a value");
        }
    }

    // @verifies REQ-SW03-07
    #[test]
    fn a_value_not_of_its_propertys_form_drops_the_event() {
        for (key, bad) in [
            ("engagement_tier", "42"),                 // the raw count the tier replaced
            ("engagement_tier", "New"),                // not the exact text
            ("screen_name", "home"),                   // not one of the nine coarsened screens
            ("screen_name", "tdcs_depression_setup"),  // a protocol name
            ("device_class", "iPhone15,2"),            // a specific model
            ("app_version", "1.0.3-beta+sha.5114f85"), // semver pre-release and build metadata
            ("app_version", "1.0"),
            ("os_version", "iOS 18.2.1"),              // a patch version
            ("os_version", "18.2"),
            ("os_version", "iOS  18.2"),
            ("app_start_type", "restart"),
            ("onboarding_step", "consent_flow_step_"),
            ("onboarding_step", "consent_flow_step_123"),
            ("onboarding_step", "depression_screening"),
            ("error_code", "1746000000000"),           // a timestamp in the digits of an error code
            ("error_code", "-1"),
            ("error_code", "4x"),
            ("error_code", ""),
            ("connectivity_type", "5g"),
        ] {
            let r = ask("evt", json!({ key: bad }));
            assert_eq!(r["ok"], json!(false), "{key}={bad}");
            assert_eq!(r["reason"], json!("value_not_permitted"), "{key}={bad}");
            assert_eq!(r["key"], json!(key));
        }
        assert_eq!(ask("evt", json!({ "error_code": 7 }))["reason"], json!("malformed"), "a value that is not text");
    }

    // @verifies REQ-SW03-07
    #[test]
    fn one_bad_property_drops_the_whole_event_it_is_never_trimmed_to_fit() {
        let r = ask("evt", json!({ "engagement_tier": "new", "app_version": "1.0.3", "hrv": "50" }));
        assert_eq!(r["ok"], json!(false));
    }

    // @verifies REQ-SW03-07
    #[test]
    fn an_event_name_must_be_short_lowercase_snake_case_with_no_health_or_identity_term() {
        assert!(ok("session_start", json!({})) && ok("setup", json!({})) && ok("app_open", json!({})));
        for bad in [
            "", "AppOpen", "app open", "app-open", "1app", "_app", "app.open", "app_open!", "é",
            &"a".repeat(41),
            "tdcs_started", "eeg_ready", "depression_check", "focus_mode", "user_email_sent", "device_token", "alpha_wave", "brain_state",
            "serial_read",
        ] {
            assert_eq!(ask(bad, json!({}))["reason"], json!("event_name"), "{bad:?}");
        }
        assert!(ok(&"a".repeat(40), json!({})), "the length bound is inclusive");
    }

    // @verifies REQ-SW03-07
    #[test]
    fn a_request_that_is_not_an_event_is_malformed_and_never_permitted() {
        for req in [json!({}), json!({ "event": "x" }), json!({ "properties": {} }), json!({ "event": 1, "properties": {} }), json!({ "event": "x", "properties": [] })] {
            assert_eq!(check(&req)["reason"], json!("malformed"), "{req}");
        }
    }

    // @verifies REQ-SW03-07
    #[test]
    fn the_policy_file_is_what_the_telemetry_spec_lists_and_every_property_has_a_form() {
        let p = policy();
        let mut names: Vec<&str> = p["properties"].as_object().unwrap().keys().map(String::as_str).collect();
        names.sort_unstable();
        assert_eq!(
            names,
            ["app_start_type", "app_version", "connectivity_type", "device_class", "engagement_tier", "error_code", "onboarding_step", "os_version", "screen_name"],
            "the nine properties of NP-APP-TELEMETRY-001 §3, no more"
        );
        for (k, forms) in p["properties"].as_object().unwrap() {
            assert!(forms.as_array().is_some_and(|f| !f.is_empty() && f.iter().all(|x| x.as_object().is_some_and(|o| o.len() == 1))), "{k}");
        }
        assert!(p["eventName"]["forbiddenTerms"].as_array().unwrap().iter().all(|t| t.as_str().unwrap() == t.as_str().unwrap().to_ascii_lowercase()));
        let _: &Map<String, Value> = p.as_object().unwrap();
    }
}
