//! Which analytics events may leave the phone (`REQ-SW03-07`; NP-APP-TELEMETRY-001 §3 and §7).
//!
//! An analytics vendor must learn nothing about the person (`CLAUDE.md` §5.1: when in doubt it is UHDR). The telemetry policy
//! says so as an allowlist, nine permitted properties each with the form of its value. The apps used to enforce a list of
//! twelve keys to forbid instead, which lets through any key nobody thought of. The rule is here, once, so that iOS and Android
//! cannot disagree about it, and it reads the one data file `common/npps/analytics-properties.json`.
//!
//! An event is permitted only if (1) its name is, exactly, one of the names the file lists, and (2) every property is named in
//! the file and its value is of one of that property's forms. One property or value outside that drops the whole event: a
//! clamped event is a different event from the one written. The answer names the first offending property by its key and never
//! repeats a value, so a refusal logged by a caller carries nothing the vendor was not to have.
//!
//! **The names are a list too (`OI-PRIV-05`).** A name used to pass if it was short snake_case containing none of the policy's
//! §7 terms, which is a list of names to forbid: `stim_2ma_done` passed it. The §7 terms are now a check on the file, not on
//! the event: a test here fails if a listed name or a literal value contains one. `error_code` was any 1 to 4 digits, which
//! admitted a helmet fault slot (a per-modality interlock slot is a number below 240, and the cardiac interlock is one of
//! them); it is a list of app failures, each above 255, so that no one-byte fault code is a permitted value
//! (`CLAUDE.md` §5.1 rule 2). `screen_name` lists no session screen and no consent screen.
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
    if !event_name_ok(event, &p["events"]) {
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

/// One of the listed names, exactly. No shape and no pattern: a name nobody listed is not sent.
fn event_name_ok(name: &str, events: &Value) -> bool {
    events.as_array().into_iter().flatten().filter_map(Value::as_str).any(|e| e == name)
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
                "onboarding_step": "consent_flow_step_1", "error_code": "1001", "connectivity_type": "wifi",
            })
        ));
        assert!(ok("app_opened", json!({})), "an event with no properties carries nothing and is permitted");
        assert!(ok("onboarding_step_completed", json!({ "onboarding_step": "consent_flow_complete" })));
        assert!(ok("app_opened", json!({ "app_version": "12.34.567", "os_version": "Android 15.0" })));
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
            let r = ask("app_opened", json!({ "engagement_tier": "new", key: "1" }));
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
            ("screen_name", "home"),                   // not one of the listed screens
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
            let r = ask("app_opened", json!({ key: bad }));
            assert_eq!(r["ok"], json!(false), "{key}={bad}");
            assert_eq!(r["reason"], json!("value_not_permitted"), "{key}={bad}");
            assert_eq!(r["key"], json!(key));
        }
        assert_eq!(ask("app_opened", json!({ "error_code": 1001 }))["reason"], json!("malformed"), "a value that is not text");
    }

    // @verifies REQ-SW03-07
    #[test]
    fn one_bad_property_drops_the_whole_event_it_is_never_trimmed_to_fit() {
        let r = ask("app_opened", json!({ "engagement_tier": "new", "app_version": "1.0.3", "hrv": "50" }));
        assert_eq!(r["ok"], json!(false));
    }

    // @verifies REQ-SW03-07
    #[test]
    fn an_event_name_is_sent_only_if_it_is_one_of_the_listed_names_exactly() {
        let p = policy();
        let listed: Vec<&str> = p["events"].as_array().unwrap().iter().map(|e| e.as_str().unwrap()).collect();
        assert_eq!(
            listed,
            ["app_opened", "onboarding_step_completed", "screen_viewed", "feature_used", "error_reported", "app_crashed"],
            "a name is added here and in NP-APP-TELEMETRY-001 §3.3 together, after the Privacy Lead's review"
        );
        for name in &listed {
            assert!(ok(name, json!({})), "{name}");
        }
        for bad in [
            // Well-shaped names with no forbidden term: each passed the old shape-and-terms rule (OI-PRIV-05 (1)).
            "stim_2ma_done", "session_start", "setup", "app_open", "e1", "evt", "low_mood_checkin", "migraine_logged",
            "consent_granted", "consent_revoked", "clinician_linked", "stimulation_halted",
            // Near misses of a listed name.
            "app_opened ", " app_opened", "App_Opened", "app_opened_2", "app_opene", "app_opened\n", "screen_viewed_session",
            // What the old rule refused is still refused.
            "", "AppOpen", "app open", "tdcs_started", "eeg_ready", "depression_check", "user_email_sent", "brain_state",
        ] {
            assert_eq!(ask(bad, json!({}))["reason"], json!("event_name"), "{bad:?}");
        }
    }

    // @verifies REQ-SW03-07
    #[test]
    fn no_screen_name_records_a_session_or_a_consent_relationship() {
        // OI-PRIV-05 (2): under the installation's stable ID each of these was a timestamped record of a session, or of a
        // research or clinician relationship. They are dropped, and no neutral stand-in for them is listed.
        for gone in ["session_setup", "session_active", "session_results", "research_consent", "clinical_consent", "session", "consent"] {
            let r = ask("screen_viewed", json!({ "screen_name": gone }));
            assert_eq!(r["reason"], json!("value_not_permitted"), "{gone}");
        }
        let p = policy();
        let screens: Vec<&str> = p["properties"]["screen_name"][0]["literal"].as_array().unwrap().iter().map(|v| v.as_str().unwrap()).collect();
        assert_eq!(screens, ["protocol_library", "protocol_detail", "account_settings", "device_management"]);
        assert_eq!(p["properties"]["screen_name"].as_array().unwrap().len(), 1, "screen_name is a literal list and nothing else");
        for s in screens {
            assert!(ok("screen_viewed", json!({ "screen_name": s })), "{s}");
            assert!(!s.contains("session") && !s.contains("consent"), "{s}");
        }
    }

    // @verifies REQ-SW03-07
    #[test]
    fn an_error_code_is_one_of_the_listed_app_failures_and_never_a_helmet_fault_code() {
        let p = policy();
        let forms = p["properties"]["error_code"].as_array().unwrap();
        assert_eq!(forms.len(), 1, "error_code is a literal list and nothing else");
        let codes: Vec<&str> = forms[0]["literal"].as_array().unwrap().iter().map(|v| v.as_str().unwrap()).collect();
        let mut meant: Vec<&str> = p["errorCodeMeanings"].as_object().unwrap().keys().map(String::as_str).collect();
        meant.sort_unstable();
        assert_eq!(codes, meant, "every permitted code says what it means, and no meaning is listed for a code not permitted");
        for c in &codes {
            assert!(ok("error_reported", json!({ "error_code": c })), "{c}");
            // A safety MCU fault slot and a module fault code are one byte. A per-modality interlock slot (the cardiac
            // interlock, the photoparoxysmal halt) is below 240, and 248 to 255 are the device slots of np_safety_config.h.
            assert!(c.parse::<u32>().unwrap() > 255, "{c} could be a one-byte helmet fault code");
        }
        // OI-PRIV-05 (3): every one-byte value, and the old form's other digit strings, are refused.
        for n in (0..=255u32).chain([256, 999, 1000, 1004, 1109, 1200, 9999]) {
            let r = ask("error_reported", json!({ "error_code": n.to_string() }));
            assert_eq!(r["reason"], json!("value_not_permitted"), "{n}");
        }
        for bad in ["01001", "1001 ", "+1001", "0x3E9"] {
            assert!(!ok("error_reported", json!({ "error_code": bad })), "{bad:?}");
        }
    }

    // @verifies REQ-SW03-07
    #[test]
    fn no_event_name_or_literal_value_in_the_policy_file_contains_a_forbidden_term() {
        // NP-APP-TELEMETRY-001 §7, applied to the list instead of to each event: with every name and every value but the
        // version and step numbers a literal, checking the file is checking everything that can be sent.
        let p = policy();
        let terms: Vec<&str> = p["forbiddenTerms"].as_array().unwrap().iter().map(|t| t.as_str().unwrap()).collect();
        assert_eq!(terms.len(), 43);
        assert!(terms.iter().all(|t| *t == t.to_ascii_lowercase()));
        let mut texts: Vec<String> = p["events"].as_array().unwrap().iter().map(|e| e.as_str().unwrap().to_string()).collect();
        for forms in p["properties"].as_object().unwrap().values() {
            for f in forms.as_array().unwrap() {
                texts.extend(f["literal"].as_array().into_iter().flatten().map(|v| v.as_str().unwrap().to_string()));
                texts.extend(f["prefixDigits"]["prefix"].as_str().map(str::to_string));
                texts.extend(f["dotted"]["prefixes"].as_array().into_iter().flatten().map(|v| v.as_str().unwrap().to_string()));
            }
        }
        assert!(texts.len() > 40, "the walk found the literals");
        for text in &texts {
            let lower = text.to_ascii_lowercase();
            for term in &terms {
                assert!(!lower.contains(term), "{text:?} contains the forbidden term {term:?}");
            }
        }
        for e in p["events"].as_array().unwrap() {
            let e = e.as_str().unwrap();
            assert!(e.chars().all(|c| c.is_ascii_lowercase() || c == '_'), "{e}: an event name carries no digit, so no value");
        }
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
            assert!(
                forms.as_array().is_some_and(|f| !f.is_empty()
                    && f.iter().all(|x| x.as_object().is_some_and(|o| o.len() == 1 && ["literal", "prefixDigits", "dotted"].contains(&o.keys().next().unwrap().as_str())))),
                "{k}: every form is one this core knows; an open digit form is not one of them"
            );
        }
        let _: &Map<String, Value> = p.as_object().unwrap();
    }
}
