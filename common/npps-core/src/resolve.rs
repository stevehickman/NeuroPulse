//! Three-tier limit resolution (OI-NPPS-CORE-01): `individual ?? helmet ?? global`, field by field, with the tier each
//! value came from.
//!
//! Web, iOS and Android each wrote this, the iOS one as fourteen hand-written per-modality merges and a parallel
//! source-map type. The rule is the same for every field, so it is written once and generically: for each modality block,
//! every field any tier states takes the value of the most specific tier that states it. A block is absent only when all
//! three tiers omit it, which is how "no limit configured" stays distinct from "a limit of zero".

use crate::parser::blocks::{camel_modality, MODALITY_LIMITS_KEYS};
use serde_json::{json, Map, Value};

/// A tier's block for `modality`, when it states one.
fn block<'a>(tier: &'a Value, modality: &str) -> Option<&'a Map<String, Value>> {
    tier.get(modality).and_then(Value::as_object)
}

/// Resolve three limit sets (each an object, or null for "no such tier"). Returns the resolved modality blocks, with
/// `level: "global"`, and the tier (`"individual"`, `"helmet"` or `"global"`) that supplied each field.
pub fn resolve_limits(global: &Value, helmet: &Value, individual: &Value) -> (Value, Value) {
    let mut limits = Map::new();
    limits.insert("level".into(), json!("global"));
    let mut sources = Map::new();
    for kind in MODALITY_LIMITS_KEYS {
        let modality = camel_modality(kind);
        // Most specific first, so the first tier to state a field wins it.
        let tiers = [("individual", block(individual, modality)), ("helmet", block(helmet, modality)), ("global", block(global, modality))];
        if tiers.iter().all(|(_, b)| b.is_none()) {
            continue;
        }
        let mut fields = Map::new();
        let mut from = Map::new();
        for (tier, b) in &tiers {
            for (k, v) in b.iter().flat_map(|b| b.iter()) {
                if !v.is_null() && !fields.contains_key(k) {
                    fields.insert(k.clone(), v.clone());
                    from.insert(k.clone(), json!(tier));
                }
            }
        }
        limits.insert(modality.into(), Value::Object(fields));
        sources.insert(modality.into(), Value::Object(from));
    }
    (Value::Object(limits), Value::Object(sources))
}
