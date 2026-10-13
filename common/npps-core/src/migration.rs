//! Device-replacement migration and UHDR backup, the rules once for every app (`SWR-MG-01…10`, `SWR-MG-12`; issue #569
//! package 6, `CLAUDE.md` §20).
//!
//! What each platform keeps for itself is the encryption (the user's key, `CLAUDE.md` §5.1), the storage routes and the
//! screens. What it may and may not do with a bundle is decided here: which items a kind carries and omits, whether a
//! bundle can be read, how a withdrawal survives a restore, who owns a claim, when the old phone may clear, what it clears,
//! and whether a storage account can carry a migration. Every operation is a pure function of the JSON it is given: none
//! reads a store, touches a key or produces text for a person (a result is a reason code, and the screen words it from the
//! locale files, `CLAUDE.md` §17). Nothing here sees a bundle's plaintext, because nothing here is given one: the items are
//! named, never carried.
//!
//! # Items
//! The state a migration may move is named in `SWR-MG-01`. **Moved by a migration:** protocols and their used flags
//! (`protocols`), session history (`sessionHistory`), consent state and grants (`consentGrants`), the withdrawal state
//! (`withdrawalState`), characterisation enrolment (`characterisationEnrolment`) and limits profiles (`limitsProfiles`).
//! **Never moved, because it belongs to the install:** the research device token, the warranty consent flag, the last paired
//! helmet id and the launch count. **Not yet sorted, and neither moved nor dropped by this row:** the consumable counters,
//! the cervical fault acknowledgement ledger and the onboarding flags.
//!
//! The UHDR-only kind is the nightly backup (`SWR-MG-03`). It carries session history, characterisation enrolment and the
//! withdrawal state; it omits protocols, limits profiles and the consent grants. The withdrawal state is in both kinds so a
//! restore never resets it (`SWR-MG-04`). Which items the UHDR-only kind carries is not stated by the requirements beyond
//! that, so the list is `UC-085` in `docs/status/unjustified-choices.md`.

use serde_json::{json, Map, Value};
use std::collections::BTreeSet;

type R<T> = Result<T, String>;

/// The bundle format version this build reads and writes (`SWR-MG-05`). The scheme beyond "one number, a newer one is
/// refused whole" is open (`OI-UI-KMP-11` open question 5).
pub const FORMAT_VERSION: u64 = 1;

const MIGRATE_ITEMS: [&str; 6] = [
    "protocols",
    "sessionHistory",
    "consentGrants",
    "withdrawalState",
    "characterisationEnrolment",
    "limitsProfiles",
];
const UHDR_ITEMS: [&str; 3] = ["sessionHistory", "characterisationEnrolment", "withdrawalState"];
const PER_INSTALL: [&str; 4] = ["researchDeviceToken", "warrantyConsentFlag", "lastPairedHelmetId", "launchCount"];
const UNSORTED: [&str; 3] = ["consumableCounters", "cervicalFaultLedger", "onboardingFlags"];

fn kind_items(kind: &str) -> R<&'static [&'static str]> {
    match kind {
        "migrate" => Ok(&MIGRATE_ITEMS),
        "uhdr" => Ok(&UHDR_ITEMS),
        other => Err(format!("unknown bundle kind '{other}'")),
    }
}

fn strs(v: &Value, key: &str) -> Vec<String> {
    v[key].as_array().into_iter().flatten().filter_map(Value::as_str).map(String::from).collect()
}

/// `SWR-MG-01`, `SWR-MG-03`: what a kind carries, and what it leaves out, so the screen can say so before the backup.
fn inventory(req: &Value) -> R<Value> {
    let kind = req["kind"].as_str().ok_or("kind is required")?;
    let carried: BTreeSet<&str> = kind_items(kind)?.iter().copied().collect();
    let omitted: Vec<&str> = MIGRATE_ITEMS.iter().copied().filter(|i| !carried.contains(i)).collect();
    Ok(json!({
        "kind": kind,
        "carried": kind_items(kind)?,
        "omitted": omitted,
        "perInstall": PER_INSTALL,
        "unsorted": UNSORTED,
    }))
}

/// `SWR-MG-05`: a bundle carries its kind and a format version in a header readable before the bundle is restored. A
/// bundle of a newer version is refused whole, never partly applied; a kind this build does not know is refused.
fn read_header(req: &Value) -> R<Value> {
    let h = &req["header"];
    let (Some(kind), Some(version)) = (h["kind"].as_str(), h["formatVersion"].as_u64()) else {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    };
    if kind_items(kind).is_err() {
        return Ok(json!({ "ok": false, "reason": "unknown_kind" }));
    }
    if version == 0 {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    }
    if version > FORMAT_VERSION {
        return Ok(json!({ "ok": false, "reason": "newer_version", "bundleVersion": version, "supportedVersion": FORMAT_VERSION }));
    }
    Ok(json!({ "ok": true, "kind": kind, "formatVersion": version }))
}

/// `SWR-MG-03`: after a restore, the screen says what was restored and what was not. `present` names the items found in
/// the bundle. An item the kind carries and the bundle lacks is `missing`; an item the kind does not carry is `omitted`.
fn restore_report(req: &Value) -> R<Value> {
    let kind = req["kind"].as_str().ok_or("kind is required")?;
    let carried = kind_items(kind)?;
    let present: BTreeSet<String> = strs(req, "present").into_iter().collect();
    let restored: Vec<&str> = carried.iter().copied().filter(|i| present.contains(*i)).collect();
    let missing: Vec<&str> = carried.iter().copied().filter(|i| !present.contains(*i)).collect();
    let omitted: Vec<&str> = MIGRATE_ITEMS.iter().copied().filter(|i| !carried.contains(i)).collect();
    Ok(json!({ "restored": restored, "missing": missing, "omitted": omitted }))
}

fn later(a: Option<&str>, b: Option<&str>) -> Option<String> {
    // ISO-8601 timestamps of one format order as text; the later one is the withdrawal that stands.
    match (a, b) {
        (Some(x), Some(y)) => Some(if x >= y { x } else { y }.to_string()),
        (Some(x), None) | (None, Some(x)) => Some(x.to_string()),
        (None, None) => None,
    }
}

/// `SWR-MG-04`: the withdrawal state is the union of what the install holds and what the bundle brings. A restore never
/// resets a withdrawal: the blanket withdrawal time is kept (the later), and every study and category withdrawal is kept.
/// `blanketConsentWithdrawnAt` is null only when it was never set, so "never asked" and "said stop" stay apart.
fn merge_withdrawal(req: &Value) -> R<Value> {
    let (local, incoming) = (&req["local"], &req["incoming"]);
    let blanket = later(local["blanketConsentWithdrawnAt"].as_str(), incoming["blanketConsentWithdrawnAt"].as_str());
    let union = |key: &str| -> Vec<String> {
        strs(local, key).into_iter().chain(strs(incoming, key)).collect::<BTreeSet<_>>().into_iter().collect()
    };
    Ok(json!({
        "blanketConsentWithdrawnAt": blanket,
        "studyWithdrawals": union("studyWithdrawals"),
        "categoryWithdrawals": union("categoryWithdrawals"),
    }))
}

/// `SWR-MG-04`: a bundle with no consent record makes the app ask and not assume, and the app says the consent choices
/// were not restored. Only the migrate kind carries the grants, so a UHDR-only restore always asks.
fn consent_after_restore(req: &Value) -> R<Value> {
    let kind = req["kind"].as_str().ok_or("kind is required")?;
    kind_items(kind)?;
    let has_record = req["bundleHasConsentRecord"].as_bool().unwrap_or(false) && kind == "migrate";
    Ok(json!({ "ask": !has_record, "tellChoicesNotRestored": !has_record }))
}

/// `SWR-MG-10`: a route that cannot give an atomic claim is not offered for the migrate kind, and may still be offered for
/// the UHDR-only backup. A direct link is offered only with an authenticated pairing.
fn offer_routes(req: &Value) -> R<Value> {
    let kind = req["kind"].as_str().ok_or("kind is required")?;
    kind_items(kind)?;
    let routes = req["routes"].as_array().ok_or("routes must be an array")?;
    let mut offered = vec![];
    let mut withheld = vec![];
    for r in routes {
        let id = r["id"].as_str().unwrap_or("");
        let atomic = r["atomicClaim"].as_bool().unwrap_or(false);
        let paired = r["authenticatedPairing"].as_bool().unwrap_or(true);
        if kind == "migrate" && !atomic {
            withheld.push(json!({ "id": id, "reason": "no_atomic_claim" }));
        } else if r["direct"].as_bool().unwrap_or(false) && !paired {
            withheld.push(json!({ "id": id, "reason": "no_authenticated_pairing" }));
        } else {
            offered.push(id);
        }
    }
    Ok(json!({ "offered": offered, "withheld": withheld }))
}

/// `SWR-MG-07`: what a restore does on meeting the claim marker of the backup location. The claim is an exclusive
/// create-if-absent write, so the store is the arbiter; this decides what to do with what the store says. A UHDR-only bundle
/// is not claimed, and a route without an atomic claim is not used for migrate.
fn claim_decision(req: &Value) -> R<Value> {
    let kind = req["kind"].as_str().ok_or("kind is required")?;
    kind_items(kind)?;
    if kind == "uhdr" {
        return Ok(json!({ "action": "no_claim_needed" }));
    }
    if !req["route"]["atomicClaim"].as_bool().unwrap_or(false) {
        return Ok(json!({ "action": "route_not_offered" }));
    }
    let (token, hash) = (req["token"].as_str().unwrap_or(""), req["bundleHash"].as_str().unwrap_or(""));
    if token.is_empty() || hash.is_empty() {
        return Err("a claim carries a token and a bundle hash".into());
    }
    let claim = &req["existingClaim"];
    if claim.is_null() {
        return Ok(json!({ "action": "write_claim" }));
    }
    // A claim that is ours (written, then the restore was resumed) stands; anyone else's refuses.
    if claim["token"].as_str() == Some(token) && claim["bundleHash"].as_str() == Some(hash) {
        return Ok(json!({ "action": "already_ours" }));
    }
    Ok(json!({ "action": "refuse_claimed" }))
}

/// `SWR-MG-07`: a claim written is owned only when it is read back and is ours. A claim written but not read back is not
/// owned; the restore stops and the person is told, and a claim with no confirmation is released only by the person's
/// explicit act, never by a timer.
fn claim_readback(req: &Value) -> R<Value> {
    let (token, hash) = (req["token"].as_str().unwrap_or(""), req["bundleHash"].as_str().unwrap_or(""));
    let back = &req["readBack"];
    let owned = !back.is_null() && back["token"].as_str() == Some(token) && back["bundleHash"].as_str() == Some(hash);
    Ok(json!({ "owned": owned, "reason": if back.is_null() { "not_read_back" } else if owned { "ours" } else { "someone_elses" } }))
}

/// `SWR-MG-08`: the old phone clears only after it reads a confirmation that matches this bundle, and never on a timer.
fn clear_decision(req: &Value) -> R<Value> {
    let hash = req["bundleHash"].as_str().unwrap_or("");
    let c = &req["confirmation"];
    let matches = !hash.is_empty() && !c.is_null() && c["bundleHash"].as_str() == Some(hash);
    Ok(json!({ "clear": matches, "reason": if c.is_null() { "no_confirmation" } else if matches { "confirmed" } else { "other_bundle" } }))
}

/// `SWR-MG-06`, `SWR-MG-08`, `SWR-MG-09`: what the old phone does when it clears, in order. The key goes first: with the
/// key destroyed, whatever flash still holds is unreadable. `done` names the steps already finished, so a clear interrupted
/// by a kill resumes at the next launch; a steps list that is not empty means a clear is in progress.
fn clear_plan(req: &Value) -> R<Value> {
    const STEPS: [&str; 6] = [
        "destroyEncryptionKey",
        "overwriteAndDeleteFiles",
        "forgetPairingRecord",
        "forgetLastPairedHelmetId",
        "clearAppHeldState",
        "writeClearedMarker",
    ];
    let done: BTreeSet<String> = strs(req, "done").into_iter().collect();
    if let Some(bad) = done.iter().find(|d| !STEPS.contains(&d.as_str())) {
        return Err(format!("unknown clear step '{bad}'"));
    }
    let remaining: Vec<&str> = STEPS.iter().copied().filter(|s| !done.contains(*s)).collect();
    Ok(json!({
        "remaining": remaining,
        "complete": remaining.is_empty(),
        // The operating system's own Bluetooth bond is outside the app. Where it cannot remove it, it says so (`SWR-MG-09`).
        "osBondInstruction": !req["canRemoveOsBond"].as_bool().unwrap_or(false),
    }))
}

/// `SWR-MG-12`: the capability check of a storage account, from the results of the probe on a throwaway object. Each
/// operation is `true` (it worked), `false` (it failed) or absent (not run). Migration needs all of them, including the
/// second create-if-absent being refused; the UHDR-only backup needs write, read, list, delete and space.
fn probe_result(req: &Value) -> R<Value> {
    let ops = &req["ops"];
    let ok = |k: &str| ops[k].as_bool() == Some(true);
    let first_failed = |names: &[&'static str]| names.iter().copied().find(|n| !ok(n));
    const BACKUP: [&str; 5] = ["write", "read", "list", "delete", "space"];
    const MIGRATE_EXTRA: [&str; 3] = ["createIfAbsentFirst", "createIfAbsentSecondRefused", "release"];
    if let Some(f) = first_failed(&BACKUP) {
        return Ok(json!({ "result": "insufficient", "failedOperation": f }));
    }
    if let Some(f) = first_failed(&MIGRATE_EXTRA) {
        return Ok(json!({ "result": "backup_only", "failedOperation": f }));
    }
    Ok(json!({ "result": "sufficient" }))
}

pub fn migration_json(request: &str) -> Result<String, String> {
    let req: Value = serde_json::from_str(request).map_err(|e| format!("bad migration request: {e}"))?;
    let op = req["op"].as_str().ok_or("op is required")?;
    let out = match op {
        "inventory" => inventory(&req)?,
        "readHeader" => read_header(&req)?,
        "restoreReport" => restore_report(&req)?,
        "mergeWithdrawal" => merge_withdrawal(&req)?,
        "consentAfterRestore" => consent_after_restore(&req)?,
        "offerRoutes" => offer_routes(&req)?,
        "claimDecision" => claim_decision(&req)?,
        "claimReadback" => claim_readback(&req)?,
        "clearDecision" => clear_decision(&req)?,
        "clearPlan" => clear_plan(&req)?,
        "probeResult" => probe_result(&req)?,
        other => return Err(format!("unknown migration operation '{other}'")),
    };
    let _: &Map<String, Value> = out.as_object().ok_or("a migration answer is an object")?;
    Ok(out.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;
    use std::sync::{Arc, Mutex};
    use std::thread;

    fn ask(v: Value) -> Value {
        serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
    }

    // A backup location with a conditional write: the only arbiter of a claim, as `SWR-MG-07` requires.
    #[derive(Default)]
    struct Store(Mutex<HashMap<String, Value>>);
    impl Store {
        fn create_if_absent(&self, key: &str, v: Value) -> bool {
            let mut m = self.0.lock().unwrap();
            if m.contains_key(key) {
                return false;
            }
            m.insert(key.into(), v);
            true
        }
        fn get(&self, key: &str) -> Option<Value> {
            self.0.lock().unwrap().get(key).cloned()
        }
    }

    // One restore, as the app would run it: decide, write conditionally, read back, decide again.
    fn try_claim(store: &Store, token: &str, hash: &str) -> bool {
        let existing = store.get("claim").unwrap_or(Value::Null);
        let d = ask(json!({"op":"claimDecision","kind":"migrate","route":{"atomicClaim":true},"token":token,"bundleHash":hash,"existingClaim":existing}));
        if d["action"] != "write_claim" {
            return d["action"] == "already_ours";
        }
        store.create_if_absent("claim", json!({"token":token,"bundleHash":hash}));
        ask(json!({"op":"claimReadback","token":token,"bundleHash":hash,"readBack":store.get("claim").unwrap_or(Value::Null)}))["owned"] == true
    }

    // @verifies SWR-MG-07
    #[test]
    fn of_many_simultaneous_claims_exactly_one_is_owned() {
        for _ in 0..50 {
            let store = Arc::new(Store::default());
            let wins: Vec<bool> = (0..8)
                .map(|i| {
                    let s = store.clone();
                    thread::spawn(move || try_claim(&s, &format!("tok{i}"), "h"))
                })
                .collect::<Vec<_>>()
                .into_iter()
                .map(|h| h.join().unwrap())
                .collect();
            assert_eq!(wins.iter().filter(|w| **w).count(), 1, "{wins:?}");
        }
    }

    // @verifies SWR-MG-07
    #[test]
    fn a_claim_is_released_only_by_an_explicit_act_and_a_second_restore_is_then_possible() {
        let store = Store::default();
        assert!(try_claim(&store, "a", "h"));
        assert!(!try_claim(&store, "b", "h"), "a second restore meets the first's claim");
        assert!(try_claim(&store, "a", "h"), "the owner resuming after a crash keeps its claim");
        store.0.lock().unwrap().remove("claim"); // the person's explicit release
        assert!(try_claim(&store, "b", "h"));
    }

    // @verifies SWR-MG-07
    #[test]
    fn a_uhdr_bundle_is_not_claimed_and_a_route_without_an_atomic_claim_is_not_used_to_migrate() {
        let r = ask(json!({"op":"claimDecision","kind":"uhdr","route":{"atomicClaim":false},"token":"t","bundleHash":"h","existingClaim":null}));
        assert_eq!(r["action"], "no_claim_needed");
        let r = ask(json!({"op":"claimDecision","kind":"migrate","route":{"atomicClaim":false},"token":"t","bundleHash":"h","existingClaim":null}));
        assert_eq!(r["action"], "route_not_offered");
    }

    // @verifies SWR-MG-07
    #[test]
    fn a_claim_written_but_not_read_back_is_not_owned() {
        let r = ask(json!({"op":"claimReadback","token":"t","bundleHash":"h","readBack":null}));
        assert_eq!(r["owned"], false);
        assert_eq!(r["reason"], "not_read_back");
        let r = ask(json!({"op":"claimReadback","token":"t","bundleHash":"h","readBack":{"token":"u","bundleHash":"h"}}));
        assert_eq!(r["owned"], false);
    }

    // @verifies SWR-MG-08
    #[test]
    fn the_old_phone_clears_only_on_a_matching_confirmation() {
        let none = ask(json!({"op":"clearDecision","bundleHash":"h","confirmation":null}));
        assert_eq!(none["clear"], false);
        let other = ask(json!({"op":"clearDecision","bundleHash":"h","confirmation":{"bundleHash":"x"}}));
        assert_eq!(other["clear"], false);
        let ok = ask(json!({"op":"clearDecision","bundleHash":"h","confirmation":{"bundleHash":"h"}}));
        assert_eq!(ok["clear"], true);
        let empty = ask(json!({"op":"clearDecision","bundleHash":"","confirmation":{"bundleHash":""}}));
        assert_eq!(empty["clear"], false, "no hash is no match");
    }

    // @verifies SWR-MG-08
    #[test]
    fn an_interrupted_clear_resumes_where_it_stopped() {
        let fresh = ask(json!({"op":"clearPlan","done":[]}));
        assert_eq!(fresh["remaining"][0], "destroyEncryptionKey", "the key goes first");
        let part = ask(json!({"op":"clearPlan","done":["destroyEncryptionKey","overwriteAndDeleteFiles"]}));
        assert_eq!(part["remaining"][0], "forgetPairingRecord");
        assert_eq!(part["complete"], false);
        let all = ask(json!({"op":"clearPlan","done":["destroyEncryptionKey","overwriteAndDeleteFiles","forgetPairingRecord","forgetLastPairedHelmetId","clearAppHeldState","writeClearedMarker"]}));
        assert_eq!(all["complete"], true);
        assert!(migration_json(r#"{"op":"clearPlan","done":["bogus"]}"#).is_err());
    }

    // @verifies SWR-MG-06
    #[test]
    fn the_key_is_destroyed_before_the_files_are_overwritten() {
        let p = ask(json!({"op":"clearPlan","done":[]}));
        let steps: Vec<&str> = p["remaining"].as_array().unwrap().iter().map(|s| s.as_str().unwrap()).collect();
        let key = steps.iter().position(|s| *s == "destroyEncryptionKey").unwrap();
        let files = steps.iter().position(|s| *s == "overwriteAndDeleteFiles").unwrap();
        assert!(key < files);
    }

    // @verifies SWR-MG-09
    #[test]
    fn the_clear_forgets_the_pairing_and_says_so_where_it_cannot_remove_the_os_bond() {
        let p = ask(json!({"op":"clearPlan","done":[],"canRemoveOsBond":false}));
        let steps = p["remaining"].to_string();
        assert!(steps.contains("forgetPairingRecord") && steps.contains("forgetLastPairedHelmetId"));
        assert_eq!(p["osBondInstruction"], true);
        assert_eq!(ask(json!({"op":"clearPlan","done":[],"canRemoveOsBond":true}))["osBondInstruction"], false);
    }

    // @verifies SWR-MG-01
    #[test]
    fn a_migration_carries_the_users_state_and_none_of_the_per_install_state() {
        let inv = ask(json!({"op":"inventory","kind":"migrate"}));
        let carried: Vec<&str> = inv["carried"].as_array().unwrap().iter().map(|s| s.as_str().unwrap()).collect();
        for want in ["protocols", "sessionHistory", "consentGrants", "withdrawalState", "characterisationEnrolment", "limitsProfiles"] {
            assert!(carried.contains(&want), "{want}");
        }
        for never in ["researchDeviceToken", "warrantyConsentFlag", "lastPairedHelmetId", "launchCount"] {
            assert!(!carried.contains(&never), "{never}");
            assert!(inv["perInstall"].to_string().contains(never));
        }
        assert!(inv["unsorted"].to_string().contains("consumableCounters"), "unsorted items are named, not moved or dropped");
    }

    // @verifies SWR-MG-03
    #[test]
    fn each_kind_says_what_it_leaves_out_before_the_backup() {
        let u = ask(json!({"op":"inventory","kind":"uhdr"}));
        let omitted = u["omitted"].to_string();
        assert!(omitted.contains("protocols") && omitted.contains("limitsProfiles"));
        assert_eq!(ask(json!({"op":"inventory","kind":"migrate"}))["omitted"].as_array().unwrap().len(), 0);
        assert!(migration_json(r#"{"op":"inventory","kind":"nightly"}"#).is_err());
    }

    // @verifies SWR-MG-03
    #[test]
    fn a_restore_lists_what_was_restored_and_what_was_not() {
        let r = ask(json!({"op":"restoreReport","kind":"migrate","present":["protocols","sessionHistory"]}));
        assert_eq!(r["restored"].as_array().unwrap().len(), 2);
        assert!(r["missing"].to_string().contains("limitsProfiles"));
        let u = ask(json!({"op":"restoreReport","kind":"uhdr","present":["sessionHistory","withdrawalState","characterisationEnrolment"]}));
        assert_eq!(u["missing"].as_array().unwrap().len(), 0);
        assert!(u["omitted"].to_string().contains("protocols"));
    }

    // @verifies SWR-MG-04
    #[test]
    fn a_restore_never_resets_a_withdrawal() {
        let local = json!({"blanketConsentWithdrawnAt":null,"studyWithdrawals":["s1"],"categoryWithdrawals":[]});
        let incoming = json!({"blanketConsentWithdrawnAt":"2026-03-01T00:00:00Z","studyWithdrawals":["s2"],"categoryWithdrawals":["c1"]});
        let m = ask(json!({"op":"mergeWithdrawal","local":local,"incoming":incoming}));
        assert_eq!(m["blanketConsentWithdrawnAt"], "2026-03-01T00:00:00Z");
        assert_eq!(m["studyWithdrawals"], json!(["s1", "s2"]));
        assert_eq!(m["categoryWithdrawals"], json!(["c1"]));
        // The other way round: a fresh install brings none, the bundle's stands; an older one never replaces a later one.
        let m = ask(json!({"op":"mergeWithdrawal","local":{"blanketConsentWithdrawnAt":"2026-05-01T00:00:00Z"},"incoming":{"blanketConsentWithdrawnAt":"2026-03-01T00:00:00Z"}}));
        assert_eq!(m["blanketConsentWithdrawnAt"], "2026-05-01T00:00:00Z");
    }

    // @verifies SWR-MG-04
    #[test]
    fn never_asked_and_said_stop_stay_apart() {
        let never = ask(json!({"op":"mergeWithdrawal","local":{},"incoming":{}}));
        assert!(never["blanketConsentWithdrawnAt"].is_null(), "never asked stays null");
        let stopped = ask(json!({"op":"mergeWithdrawal","local":{},"incoming":{"blanketConsentWithdrawnAt":"2026-03-01T00:00:00Z"}}));
        assert!(!stopped["blanketConsentWithdrawnAt"].is_null());
    }

    // @verifies SWR-MG-04
    #[test]
    fn a_bundle_with_no_consent_record_makes_the_app_ask() {
        let r = ask(json!({"op":"consentAfterRestore","kind":"uhdr","bundleHasConsentRecord":true}));
        assert_eq!(r["ask"], true, "the UHDR-only kind never carries the grants");
        assert_eq!(r["tellChoicesNotRestored"], true);
        let r = ask(json!({"op":"consentAfterRestore","kind":"migrate","bundleHasConsentRecord":false}));
        assert_eq!(r["ask"], true);
        let r = ask(json!({"op":"consentAfterRestore","kind":"migrate","bundleHasConsentRecord":true}));
        assert_eq!(r["ask"], false);
    }

    // @verifies SWR-MG-05
    #[test]
    fn a_header_names_the_kind_and_a_newer_version_is_refused_whole() {
        assert_eq!(ask(json!({"op":"readHeader","header":{"kind":"uhdr","formatVersion":1}}))["ok"], true);
        let newer = ask(json!({"op":"readHeader","header":{"kind":"migrate","formatVersion":FORMAT_VERSION + 1}}));
        assert_eq!(newer["ok"], false);
        assert_eq!(newer["reason"], "newer_version");
        assert_eq!(ask(json!({"op":"readHeader","header":{"kind":"x","formatVersion":1}}))["reason"], "unknown_kind");
        assert_eq!(ask(json!({"op":"readHeader","header":{"kind":"uhdr"}}))["reason"], "malformed");
        assert_eq!(ask(json!({"op":"readHeader","header":{"kind":"uhdr","formatVersion":0}}))["reason"], "malformed");
    }

    // @verifies SWR-MG-10
    #[test]
    fn a_route_without_an_atomic_claim_is_withheld_from_migration_but_offered_for_backup() {
        let routes = json!([{"id":"cloudA","atomicClaim":true},{"id":"folder","atomicClaim":false},{"id":"direct","atomicClaim":true,"direct":true,"authenticatedPairing":false}]);
        let m = ask(json!({"op":"offerRoutes","kind":"migrate","routes":routes}));
        assert_eq!(m["offered"], json!(["cloudA"]));
        assert_eq!(m["withheld"].as_array().unwrap().len(), 2);
        let b = ask(json!({"op":"offerRoutes","kind":"uhdr","routes":routes}));
        assert_eq!(b["offered"], json!(["cloudA", "folder"]), "no pairing still withholds a direct link");
    }

    // @verifies SWR-MG-12
    #[test]
    fn the_probe_yields_one_of_three_results_and_names_the_failed_operation() {
        let all = json!({"write":true,"read":true,"list":true,"delete":true,"space":true,"createIfAbsentFirst":true,"createIfAbsentSecondRefused":true,"release":true});
        assert_eq!(ask(json!({"op":"probeResult","ops":all}))["result"], "sufficient");
        let mut no_cond = all.clone();
        no_cond["createIfAbsentSecondRefused"] = json!(false);
        let r = ask(json!({"op":"probeResult","ops":no_cond}));
        assert_eq!(r["result"], "backup_only");
        assert_eq!(r["failedOperation"], "createIfAbsentSecondRefused");
        let mut no_write = all.clone();
        no_write["write"] = json!(false);
        let r = ask(json!({"op":"probeResult","ops":no_write}));
        assert_eq!(r["result"], "insufficient");
        assert_eq!(r["failedOperation"], "write");
        let mut not_run = all;
        not_run.as_object_mut().unwrap().remove("space");
        assert_eq!(ask(json!({"op":"probeResult","ops":not_run}))["result"], "insufficient", "an operation not run is not a pass");
    }
}
