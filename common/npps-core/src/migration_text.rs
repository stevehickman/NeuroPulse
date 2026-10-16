//! The names of the texts a migration screen shows, once for every app (`SWR-MG-03`; issue #569 package 6n, `CLAUDE.md` §17).
//!
//! The core produces no text for a person: a result is a reason code, an item is a name, and a screen words them from the
//! locale files. What is shared here is the mapping from a code to the locale key that words it, so that no app spells a
//! key its own way, or words a reason the others leave unworded. A reason the core can end a flow with, an item a kind
//! carries or leaves out, a rejected secret and a storage check's result each have exactly one key, and a test holds the
//! table to `locales/en.json` in both directions: a code with no text fails, and a migration text no code names fails.
//!
//! | group | code | key |
//! |-------|------|-----|
//! | `reasons` | a flow's `done.reason` | `MIGRATION_REASON_<CODE>` |
//! | `secret` | a rejected recovery code or passphrase | `MIGRATION_SECRET_<CODE>` |
//! | `probe` | a storage check's result | `MIGRATION_PROBE_<CODE>` |
//! | `items` | an item a kind carries, leaves out or never moves | `MIGRATION_ITEM_<NAME>` |
//!
//! The keys are written out in full, so that a search for one finds the table that names it and the locale gate sees it used.

use serde_json::{json, Map, Value};

type R<T> = Result<T, String>;

/// Every reason a flow, a header check, a clear decision or a link can end with `ok: false`. `closed` (a serve that ended
/// because the other phone hung up) and `confirmed` (a clear that may go ahead) are not failures and have no text.
const REASONS: [(&str, &str); 18] = [
    ("authentication_failed", "MIGRATION_REASON_AUTHENTICATION_FAILED"),
    ("malformed", "MIGRATION_REASON_MALFORMED"),
    ("newer_version", "MIGRATION_REASON_NEWER_VERSION"),
    ("unknown_kind", "MIGRATION_REASON_UNKNOWN_KIND"),
    ("no_atomic_claim", "MIGRATION_REASON_NO_ATOMIC_CLAIM"),
    ("no_authenticated_pairing", "MIGRATION_REASON_NO_AUTHENTICATED_PAIRING"),
    ("claim_not_owned", "MIGRATION_REASON_CLAIM_NOT_OWNED"),
    ("claimed", "MIGRATION_REASON_CLAIMED"),
    ("no_bundle", "MIGRATION_REASON_NO_BUNDLE"),
    ("route_not_offered", "MIGRATION_REASON_ROUTE_NOT_OFFERED"),
    ("step_failed", "MIGRATION_REASON_STEP_FAILED"),
    ("verify_failed", "MIGRATION_REASON_VERIFY_FAILED"),
    ("route_unavailable", "MIGRATION_REASON_ROUTE_UNAVAILABLE"),
    ("no_confirmation", "MIGRATION_REASON_NO_CONFIRMATION"),
    ("other_bundle", "MIGRATION_REASON_OTHER_BUNDLE"),
    ("not_authenticated", "MIGRATION_REASON_NOT_AUTHENTICATED"),
    ("link_failed", "MIGRATION_REASON_LINK_FAILED"),
    ("malformed_frame", "MIGRATION_REASON_MALFORMED_FRAME"),
];

/// Why a recovery code or passphrase was refused before any bundle was touched (`bundle_key`).
const SECRET: [(&str, &str); 3] = [
    ("malformed", "MIGRATION_SECRET_MALFORMED"),
    ("bad_checksum", "MIGRATION_SECRET_BAD_CHECKSUM"),
    ("too_short", "MIGRATION_SECRET_TOO_SHORT"),
];

/// What the storage check says about an account (`probeResult`).
const PROBE: [(&str, &str); 3] = [
    ("sufficient", "MIGRATION_PROBE_SUFFICIENT"),
    ("backup_only", "MIGRATION_PROBE_BACKUP_ONLY"),
    ("insufficient", "MIGRATION_PROBE_INSUFFICIENT"),
];

/// Every item a screen may name: the six a migration carries, the four it never moves and the three not yet sorted.
const ITEMS: [(&str, &str); 13] = [
    ("protocols", "MIGRATION_ITEM_PROTOCOLS"),
    ("sessionHistory", "MIGRATION_ITEM_SESSION_HISTORY"),
    ("consentGrants", "MIGRATION_ITEM_CONSENT_GRANTS"),
    ("withdrawalState", "MIGRATION_ITEM_WITHDRAWAL_STATE"),
    ("characterisationEnrolment", "MIGRATION_ITEM_CHARACTERISATION_ENROLMENT"),
    ("limitsProfiles", "MIGRATION_ITEM_LIMITS_PROFILES"),
    ("researchDeviceToken", "MIGRATION_ITEM_RESEARCH_DEVICE_TOKEN"),
    ("warrantyConsentFlag", "MIGRATION_ITEM_WARRANTY_CONSENT_FLAG"),
    ("lastPairedHelmetId", "MIGRATION_ITEM_LAST_PAIRED_HELMET_ID"),
    ("launchCount", "MIGRATION_ITEM_LAUNCH_COUNT"),
    ("consumableCounters", "MIGRATION_ITEM_CONSUMABLE_COUNTERS"),
    ("cervicalFaultLedger", "MIGRATION_ITEM_CERVICAL_FAULT_LEDGER"),
    ("onboardingFlags", "MIGRATION_ITEM_ONBOARDING_FLAGS"),
];

fn table(rows: &[(&str, &str)]) -> Value {
    Value::Object(rows.iter().map(|(code, key)| ((*code).to_string(), json!(key))).collect::<Map<String, Value>>())
}

/// `messageKeys`: the whole table, each group a map from a code to its locale key.
pub(crate) fn op(name: &str, _req: &Value) -> Option<R<Value>> {
    (name == "messageKeys").then(|| {
        Ok(json!({
            "reasons": table(&REASONS),
            "secret": table(&SECRET),
            "probe": table(&PROBE),
            "items": table(&ITEMS),
        }))
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeSet;

    fn keys() -> Vec<String> {
        let t = op("messageKeys", &json!({})).unwrap().unwrap();
        t.as_object().unwrap().values().flat_map(|g| g.as_object().unwrap().values().map(|k| k.as_str().unwrap().to_string())).collect()
    }

    fn english() -> Map<String, Value> {
        let path = concat!(env!("CARGO_MANIFEST_DIR"), "/../../locales/en.json");
        serde_json::from_str(&std::fs::read_to_string(path).expect("locales/en.json")).unwrap()
    }

    // @verifies SWR-MG-03
    #[test]
    fn every_code_has_a_text_and_every_migration_text_belongs_to_a_code() {
        let en = english();
        let named: BTreeSet<String> = keys().into_iter().collect();
        assert_eq!(named.len(), keys().len(), "two codes share one key");
        for k in &named {
            assert!(en.get(k).and_then(Value::as_str).is_some_and(|t| !t.is_empty()), "{k} has no English text in locales/en.json");
        }
        let held: BTreeSet<String> = en
            .keys()
            .filter(|k| ["MIGRATION_REASON_", "MIGRATION_SECRET_", "MIGRATION_PROBE_", "MIGRATION_ITEM_"].iter().any(|p| k.starts_with(p)))
            .cloned()
            .collect();
        assert_eq!(held, named, "a migration text no code names, or a code with no text");
    }

    // @verifies SWR-MG-03
    #[test]
    fn every_reason_the_core_writes_has_a_key() {
        let sources = [
            include_str!("migration.rs"),
            include_str!("migration_flow.rs"),
            include_str!("object_route.rs"),
            include_str!("link.rs"),
        ];
        let known: BTreeSet<&str> = REASONS.iter().map(|(code, _)| *code).chain(["closed", "confirmed"]).collect();
        let mut seen = BTreeSet::new();
        for src in sources {
            let code = src.split("#[cfg(test)]").next().unwrap();
            for marker in ["\"reason\": \"", "refused(\""] {
                for part in code.split(marker).skip(1) {
                    let reason: String = part.chars().take_while(|c| c.is_ascii_lowercase() || *c == '_').collect();
                    if !reason.is_empty() {
                        seen.insert(reason);
                    }
                }
            }
        }
        for r in &seen {
            assert!(known.contains(r.as_str()), "the core ends with reason '{r}', which has no text key");
        }
        assert!(seen.len() >= 12, "the scan found too few reasons ({seen:?}); the source no longer reads the way it assumes");
    }

    // @verifies SWR-MG-03
    #[test]
    fn the_table_is_served_by_group_and_each_key_follows_its_group_prefix() {
        let m = op("messageKeys", &json!({})).unwrap().unwrap();
        assert_eq!(m["items"]["lastPairedHelmetId"], "MIGRATION_ITEM_LAST_PAIRED_HELMET_ID");
        assert_eq!(m["reasons"]["claimed"], "MIGRATION_REASON_CLAIMED");
        for (group, prefix) in [("reasons", "MIGRATION_REASON_"), ("secret", "MIGRATION_SECRET_"), ("probe", "MIGRATION_PROBE_"), ("items", "MIGRATION_ITEM_")] {
            for (code, key) in m[group].as_object().unwrap() {
                assert!(key.as_str().unwrap().starts_with(prefix), "{code} in {group} has key {key}");
            }
        }
        assert!(op("something_else", &json!({})).is_none());
    }

    // @verifies SWR-MG-03
    #[test]
    fn the_items_the_core_names_are_all_in_the_table() {
        let inv = crate::migration::migration_json(&json!({ "op": "inventory", "kind": "migrate" }).to_string()).unwrap();
        let inv: Value = serde_json::from_str(&inv).unwrap();
        for group in ["carried", "omitted", "perInstall", "unsorted"] {
            for item in inv[group].as_array().unwrap() {
                assert!(ITEMS.iter().any(|(name, _)| *name == item.as_str().unwrap()), "{item} has no text key");
            }
        }
    }
}
