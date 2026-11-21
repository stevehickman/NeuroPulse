//! Device-replacement migration and UHDR backup, the rules once for every app (`SWR-MG-01…10`, `SWR-MG-12`; issue #569
//! package 6, `CLAUDE.md` §20).
//!
//! What each platform keeps for itself is the user's key and its Argon2id computation (`bundle_key`), the storage routes
//! and the screens. What it may and may not do with a bundle is decided here: which items a kind carries and omits, the
//! container that seals them, whether a bundle can be read, how a withdrawal survives a restore, who owns a claim, when the
//! old phone may clear, what it clears, and whether a storage account can carry a migration. Every operation is a pure
//! function of the JSON it is given (the ORDER in which they are asked is `migration_flow`): none reads a store, keeps a key or produces text for a person (a result is a reason
//! code, and the screen words it from the locale files, `CLAUDE.md` §17). A key, a nonce and a bundle's items are handed in
//! for one call and nothing is kept (`sealBundle`, `openBundle`); every other operation is given item names and ids only.
//!
//! # Items
//! The state a migration may move is named in `SWR-MG-01`. **Moved by a migration:** protocols and their used flags
//! (`protocols`), session history (`sessionHistory`), the withdrawal state (`withdrawalState`) and limits profiles
//! (`limitsProfiles`). **Never moved, because it belongs to the install:**
//! the research device token, the last paired helmet id and the launch count. **Never moved with a user's data, because it is
//! the warranty owner's and not a user's** (`REQ-SHDR-10`; the principal, 2026-11-20): the warranty consent
//! (`warrantyConsentFlag`) and characterisation enrolment (`characterisationEnrolment`). A bundle an earlier version sealed
//! may hold `characterisationEnrolment`; it is dropped when the bundle is opened and the rest is restored (`RETIRED_ITEMS`).
//! **Never moved, by
//! ruling:** the consent grants (`consentGrants`). A grant is stated on each device, so an out-of-date one can never be
//! restored; a restore can only reduce what a phone is consented to, never add to it (`SWR-MG-04`). **Not yet sorted, and neither moved nor dropped by this row:** the consumable counters,
//! the cervical fault acknowledgement ledger and the onboarding flags.
//!
//! The UHDR-only kind is the nightly backup (`SWR-MG-03`). It carries session history and the
//! withdrawal state; it omits protocols and limits profiles. The withdrawal state is in both kinds so a
//! restore never resets it (`SWR-MG-04`). Which items the UHDR-only kind carries is not stated by the requirements beyond
//! that, so the list is `UC-085` in `docs/status/unjustified-choices.md`.

use crate::bundle_key;
use aes_gcm::aead::{Aead, KeyInit, Payload};
use aes_gcm::{Aes256Gcm, Nonce};
use serde_json::{json, Map, Value};
use hkdf::Hkdf;
use hmac::{Hmac, Mac};
use sha2::{Digest, Sha256};
use std::collections::BTreeSet;

type R<T> = Result<T, String>;

/// The bundle format version this build reads and writes (`SWR-MG-05`). The scheme beyond "one number, a newer one is
/// refused whole" is open (`OI-UI-KMP-11` open question 5). The epoch joined the sealed body (`SWR-MG-29`, `OI-PRIV-09`)
/// and the number stayed 1, by ruling (the principal, 2026-11-19): no build that sealed a bundle without an epoch was
/// released, so no such bundle should exist and there is no older reader to refuse this one. A body with no epoch, were one
/// to turn up, is `malformed`. The next change to the format after a release bumps the version.
pub const FORMAT_VERSION: u64 = 1;

/// The stored keys, sorted (`common/npps/migration-keys.json`, one file for every app and the gate that reads every app).
const KEY_MAP: &str = include_str!("../../npps/migration-keys.json");

/// The whole key map as JSON.
pub(crate) fn key_map() -> Value {
    serde_json::from_str(KEY_MAP).expect("common/npps/migration-keys.json is valid JSON")
}

/// The stored keys that make up one carried item, or none for an item not stored by key (a consent item).
pub(crate) fn keys_of(item: &str) -> Vec<String> {
    key_map()["carried"][item].as_array().into_iter().flatten().filter_map(Value::as_str).map(String::from).collect()
}

/// Every key in every list: what the old phone forgets when it clears (`SWR-MG-09`).
pub(crate) fn all_keys() -> Vec<String> {
    let m = key_map();
    let mut v: Vec<String> = m["carried"].as_object().into_iter().flat_map(|o| o.values()).chain(["consent", "perInstall", "unsorted"].iter().map(|k| &m[*k]))
        .flat_map(|a| a.as_array().into_iter().flatten().filter_map(Value::as_str).map(String::from).collect::<Vec<_>>())
        .collect();
    v.sort();
    v.dedup();
    v
}

/// `SWR-MG-06`: the keys whose values a host seals at rest, so that destroying the data key makes them unreadable. It is every key
/// the apps store: every key of every item a migration carries, the consent keys, **the per-install keys and the keys not yet
/// sorted**. A key still waiting to be sorted for migration has no ruling that it is not user data, and `CLAUDE.md` §5.1 says what to do then: when in doubt it is UHDR, and a
/// reclassification needs a positive demonstration of no user biology. The unsorted keys also include the cervical fault
/// acknowledgement (a cardiac fact) and the onboarding records (age, biometric-privacy consent), which are about the person. The
/// per-install keys are sealed too (`OI-DESKTOP-01` item 4, `SWR-MG-16`). They name a device, an install or a warranty registration
/// and so are not the person's, but several say something about the person by inference: the launch count is an engagement
/// measure, the backup folder is a path with the person's name in it, the pending analytics erasures are pseudonymous IDs, and the
/// helmet and the research device token tie a person to a device. Nothing shows that any of them is free of that, and `CLAUDE.md`
/// §5.1 says what to do then. They never move in a migration (that is `perInstall`'s meaning); sealing is about what is left on
/// the disk. The list is computed here so every platform seals the same keys, and a key moved out of `unsorted` into a list that
/// is not sealed has to be taken out of this one on purpose.
pub(crate) fn sealed_at_rest() -> Vec<String> {
    all_keys()
}

/// `NP-APP-PRESENCE-001` §4 (`REQ-PRES-05`): the keys that stay readable while the person's presence has not been established, so that
/// reminders, the fleet upload and the hub reconnect work locked. **Every other sealed key is UHDR and is gated.** The list is in the
/// key file next to the sorting, so the apps read one list; it is a subset of the sorted keys, and a key joins it only by a ruling that
/// shows it says nothing about the person (`CLAUDE.md` §5.1). **The three it holds are a proposal awaiting the Privacy Lead
/// (`OI-PRES-01`).**
pub(crate) fn device_tier() -> Vec<String> {
    let mut v: Vec<String> = key_map()["deviceTier"].as_array().into_iter().flatten().filter_map(Value::as_str).map(String::from).collect();
    v.sort();
    v.dedup();
    v
}

const MIGRATE_ITEMS: [&str; 4] = ["protocols", "sessionHistory", "withdrawalState", "limitsProfiles"];
const UHDR_ITEMS: [&str; 2] = ["sessionHistory", "withdrawalState"];
const PER_INSTALL: [&str; 6] =
    ["researchDeviceToken", "warrantyConsentFlag", "characterisationEnrolment", "lastPairedHelmetId", "launchCount", "consentGrants"];
/// `REQ-SHDR-10` (c): items an earlier version carried and no bundle carries now. One met in a bundle that is being opened is
/// dropped, never restored, and the rest of the bundle is restored; sealing one is refused like any item a kind does not carry.
/// Characterisation enrolment is the warranty owner's decision about a helmet and was once carried with a user's data.
const RETIRED_ITEMS: [&str; 1] = ["characterisationEnrolment"];
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
    // A withdrawal mark is a day and that decision's withdrawal count, `yyyy-MM-dd#NNNNNN` (`REQ-CONSENT-24`). Marks order
    // as text, by day and then by count; the later one is the withdrawal that stands. The core never parses one.
    match (a, b) {
        (Some(x), Some(y)) => Some(if x >= y { x } else { y }.to_string()),
        (Some(x), None) | (None, Some(x)) => Some(x.to_string()),
        (None, None) => None,
    }
}

/// The withdrawals that are one time each: the blanket consent (L3), the contact consent (L1), and the two L4 opt-ins.
const WITHDRAWAL_TIMES: [&str; 4] = [
    "blanketConsentWithdrawnAt",
    "contactConsentWithdrawnAt",
    "resultsOptInWithdrawnAt",
    "suggestionPortalOptInWithdrawnAt",
];

/// The withdrawals that are one time per id: each research category (L2, named by its wire name, e.g. `PTSD`) and each study.
const WITHDRAWAL_BY_ID: [&str; 2] = ["categoryWithdrawals", "studyWithdrawals"];

/// `SWR-MG-04`: the withdrawal state is the union of what the install holds and what the bundle brings, and a withdrawal
/// is sticky: it is never undone. A restore keeps the later time of every withdrawal and every id withdrawn on either side, and
/// a time is null only when that decision was never withdrawn, so "never asked" and "said stop" stay apart. A time is a
/// withdrawal mark, a day and that decision's count in one fixed form (`2026-03-01#000001`), which orders as text. The app switches off whatever the merged state
/// withdraws that the phone had not: a restore can only reduce what a phone is consented to.
fn merge_withdrawal(req: &Value) -> R<Value> {
    let (local, incoming) = (&req["local"], &req["incoming"]);
    let mut out = Map::new();
    for key in WITHDRAWAL_TIMES {
        out.insert(key.into(), json!(later(local[key].as_str(), incoming[key].as_str())));
    }
    for key in WITHDRAWAL_BY_ID {
        let mut merged: std::collections::BTreeMap<String, String> = Default::default();
        for side in [local, incoming] {
            for (id, at) in side[key].as_object().into_iter().flatten() {
                if let Some(at) = at.as_str() {
                    let entry = merged.entry(id.clone()).or_insert_with(|| at.to_string());
                    if at > entry.as_str() {
                        *entry = at.to_string();
                    }
                }
            }
        }
        out.insert(key.into(), json!(merged));
    }
    Ok(Value::Object(out))
}

/// `SWR-MG-04`: what a restore changes on a phone that already holds withdrawals. Every platform asks the core instead of
/// comparing times itself. `applied` names each decision whose incoming withdrawal is later than the held one (or the phone
/// held none), and the app switches exactly those grants off and records the time: a grant made here after the withdrawal
/// stands, and nothing here turns a grant on. `merged` is the withdrawal state to hold afterwards (`mergeWithdrawal`).
fn apply_withdrawal(req: &Value) -> R<Value> {
    let (held, incoming) = (&req["held"], &req["incoming"]);
    let later_than_held = |held: &Value, incoming: &Value| match (incoming.as_str(), held.as_str()) {
        (Some(i), Some(h)) => i > h,
        (Some(_), None) => true,
        _ => false,
    };
    let mut applied = Map::new();
    let mut times = vec![];
    for key in WITHDRAWAL_TIMES {
        if later_than_held(&held[key], &incoming[key]) {
            times.push(key);
        }
    }
    applied.insert("times".into(), json!(times));
    for key in WITHDRAWAL_BY_ID {
        let ids: Vec<&String> = incoming[key]
            .as_object()
            .into_iter()
            .flatten()
            .filter(|(id, at)| later_than_held(&held[key][id.as_str()], at))
            .map(|(id, _)| id)
            .collect();
        applied.insert(key.into(), json!(ids));
    }
    Ok(json!({ "merged": merge_withdrawal(&json!({ "local": held, "incoming": incoming }))?, "applied": applied }))
}

/// `SWR-MG-04`: a restore makes the app ask and not assume, and the app says the consent choices were not restored. Grants
/// are never carried (they are stated on each device), so this is the answer whatever the bundle holds; a bundle's
/// `bundleHasConsentRecord` is honoured only for a future format that carries them.
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
    let tag = req["claimTag"].as_str().filter(|t| !t.is_empty()).ok_or("a claim carries a tag")?;
    let claim = &req["existingClaim"];
    if claim.is_null() {
        return Ok(json!({ "action": "write_claim" }));
    }
    // OI-PRIV-02 (option A): a marker is a claim only when it is for this bundle and carries the tag a holder of the bundle
    // key makes. Any other marker was planted, is refused with its own reason and is never touched.
    let genuine = claim["bundleHash"].as_str() == Some(hash) && same_tag(claim["tag"].as_str().unwrap_or(""), tag);
    if !genuine {
        return Ok(json!({ "action": "refuse_unauthentic" }));
    }
    // A claim that is ours (written, then the restore was resumed) stands; another phone's refuses.
    if claim["token"].as_str() == Some(token) {
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
    let tag = req["claimTag"].as_str().unwrap_or("");
    let owned = !back.is_null()
        && back["token"].as_str() == Some(token)
        && back["bundleHash"].as_str() == Some(hash)
        && same_tag(back["tag"].as_str().unwrap_or(""), tag);
    Ok(json!({ "owned": owned, "reason": if back.is_null() { "not_read_back" } else if owned { "ours" } else { "someone_elses" } }))
}

/// `SWR-MG-08`: the label under which the confirmation key is derived from the bundle key, so the bundle key itself is
/// used only to seal and open the bundle.
const CONFIRMATION_LABEL: &[u8] = b"np-migration-confirmation-v1";

/// `SWR-MG-08`: the tag a confirmation carries: HMAC-SHA-256 over the bundle hash, under a key derived (HKDF-SHA-256) from
/// the bundle key. Only a phone that holds the bundle key can make it: the old phone makes it when it seals the bundle and
/// keeps it (not the key), and the new phone makes it when it has opened the bundle. The bundle hash is computed from
/// ciphertext the backup location already holds, so without the tag anyone who can write there could confirm.
fn confirmation_tag(req: &Value) -> R<Value> {
    let key = crate::secret_io::secret(req, "key", &["bundle"])?;
    if key.len() != KEY_LEN {
        return Err(format!("the key is {KEY_LEN} bytes"));
    }
    let hash = req["bundleHash"].as_str().filter(|h| !h.is_empty()).ok_or("bundleHash is required")?;
    let mut mac_key = zeroize::Zeroizing::new([0u8; 32]);
    Hkdf::<Sha256>::new(None, &key).expand(CONFIRMATION_LABEL, &mut *mac_key).map_err(|_| "key derivation failed".to_string())?;
    let mut m = <Hmac<Sha256> as Mac>::new_from_slice(&*mac_key).map_err(|_| "bad key".to_string())?;
    m.update(hash.as_bytes());
    Ok(json!({ "tag": hex(&m.finalize().into_bytes()) }))
}

/// `SWR-MG-07`: the label under which the claim key is derived from the bundle key (`OI-PRIV-02`).
const CLAIM_LABEL: &[u8] = b"np-migration-claim-v1";

/// `SWR-MG-07`: the tag a claim carries: HMAC-SHA-256 over the bundle hash under an HKDF-SHA-256 key from the bundle key with
/// its own label. Only a phone that holds the bundle key can make it, so a marker planted by anyone else has none. It is
/// published at the location and is not a secret; it is per bundle, not per token, so the restoring phone can check a
/// marker without keeping the key past the step that opened the bundle.
fn claim_tag(req: &Value) -> R<Value> {
    let key = crate::secret_io::secret(req, "key", &["bundle"])?;
    if key.len() != KEY_LEN {
        return Err(format!("the key is {KEY_LEN} bytes"));
    }
    let hash = req["bundleHash"].as_str().filter(|h| !h.is_empty()).ok_or("bundleHash is required")?;
    let mut mac_key = zeroize::Zeroizing::new([0u8; 32]);
    Hkdf::<Sha256>::new(None, &key).expand(CLAIM_LABEL, &mut *mac_key).map_err(|_| "key derivation failed".to_string())?;
    let mut m = <Hmac<Sha256> as Mac>::new_from_slice(&*mac_key).map_err(|_| "bad key".to_string())?;
    m.update(hash.as_bytes());
    Ok(json!({ "tag": hex(&m.finalize().into_bytes()) }))
}

/// Whether two tags are the same, in time that does not depend on where they differ. An empty tag matches nothing.
pub(crate) fn same_tag(a: &str, b: &str) -> bool {
    !a.is_empty() && a.len() == b.len() && a.bytes().zip(b.bytes()).fold(0u8, |d, (x, y)| d | (x ^ y)) == 0
}

/// `SWR-MG-08`: the old phone clears only after it reads a confirmation that matches this bundle and carries the tag this
/// phone recorded when it sealed the bundle (`expected`), and never on a timer. A confirmation with no tag or another tag was
/// not written by a phone that opened the bundle, and is no confirmation. A phone with no record of the bundle (`expected`
/// absent, or of another bundle) never clears against it: the bundle at the backup location may not be the one it wrote.
fn clear_decision(req: &Value) -> R<Value> {
    let hash = req["bundleHash"].as_str().unwrap_or("");
    let (c, e) = (&req["confirmation"], &req["expected"]);
    let reason = if hash.is_empty() || c.is_null() {
        "no_confirmation"
    } else if c["bundleHash"].as_str() != Some(hash) || (!e.is_null() && e["bundleHash"].as_str() != Some(hash)) {
        "other_bundle"
    } else if same_tag(e["tag"].as_str().unwrap_or(""), c["tag"].as_str().unwrap_or("")) {
        "confirmed"
    } else {
        "no_confirmation"
    };
    Ok(json!({ "clear": reason == "confirmed", "reason": reason }))
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

// ─── The encrypted bundle container (`SWR-MG-02`, `SWR-MG-05`) ───────────────────────────────────────────────────────────
//
// Layout, in bytes:  `"NPBK"` (4) · header length, big-endian u32 (4) · header JSON · ciphertext with its 16-byte tag.
// The header is `{"kind","formatVersion","nonce","kdf"}`, in the clear so a restore can identify the kind and refuse a newer
// version before it asks for a key (`SWR-MG-05`). The body is AES-256-GCM (`CLAUDE.md` §5.1) of `{"items":{name: value}}`,
// with the magic, the length and the header as associated data, so an edited header or kind fails authentication and a
// bundle cannot be re-labelled. The key is the user's: the platform derives it and hands it in for this one call, and the
// core keeps nothing (`SWR-MG-02`: NeurOne holds no key and sees no plaintext). The nonce is also handed in, from the
// platform's CSPRNG, because the core is pure and has no source of randomness; a nonce is used once per key.
//
// The epoch (`SWR-MG-29` (b) to (d), `OI-PRIV-09`). The place that keeps a bundle is not trusted, and without more it can
// hand back any bundle it was ever given: an older one is as authentic as the newest. So the sealed body is
// `{"epoch": n, "items": {…}}`, where the epoch is a count that only rises: a phone seals each bundle
// with one more than the highest epoch it has sealed or opened. It is inside the ciphertext, so it is authenticated and the
// backup location cannot read it or count backups by it. `openBundle` is told the highest epoch the phone has seen
// (`seenEpoch`, required, 0 for none) and refuses a bundle below it whole (`older_bundle`), with no items in the answer.
// An equal epoch opens: it is the bundle the phone last sealed or opened. The phone keeps the number, not the core
// (`migration_flow`).
//
// **What this does not do.** A phone that has seen nothing (a new phone, a fresh install, a phone after an erase) holds 0
// and opens any authentic bundle, an old one included: nothing on it says a newer one ever existed. So the epoch protects
// a phone that still holds its state from being taken back, and does not protect the first restore on a new phone, which
// is the device-replacement case. There, the old phone refuses to clear against any bundle but the one it sealed
// (`clearDecision`), so the person keeps the newer state on the old phone; the new phone is not told.

const MAGIC: &[u8; 4] = b"NPBK";
const NONCE_LEN: usize = 12;
const KEY_LEN: usize = 32;

pub(crate) fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

pub(crate) fn unhex(s: &str, what: &str) -> R<Vec<u8>> {
    if s.len() % 2 != 0 || !s.is_ascii() {
        return Err(format!("{what} is not hex"));
    }
    // Decoded into one buffer of its final size (`SWR-MG-28` (c)). Collecting through a `Result` starts small and grows,
    // and each growth releases the shorter copy as it is: for a key, its first bytes, whatever wraps the result afterwards.
    let mut out = Vec::with_capacity(s.len() / 2);
    for i in (0..s.len()).step_by(2) {
        match u8::from_str_radix(&s[i..i + 2], 16) {
            Ok(b) => out.push(b),
            Err(_) => {
                zeroize::Zeroize::zeroize(&mut out);
                return Err(format!("{what} is not hex"));
            }
        }
    }
    Ok(out)
}

/// `SWR-MG-07`, `SWR-MG-08`: the bundle hash a claim and a confirmation carry: SHA-256 of the whole bundle, as 64 hex digits.
fn bundle_hash(bundle: &[u8]) -> String {
    hex(&Sha256::digest(bundle))
}

/// Split a bundle into its header bytes, its parsed header and its ciphertext. A bundle that is not ours, is truncated, or
/// has a header that is not JSON is `malformed`.
fn split_bundle(bundle: &[u8]) -> Result<(&[u8], Value, &[u8]), &'static str> {
    if bundle.len() < 8 || &bundle[..4] != MAGIC {
        return Err("malformed");
    }
    let hlen = u32::from_be_bytes([bundle[4], bundle[5], bundle[6], bundle[7]]) as usize;
    let end = 8usize.checked_add(hlen).ok_or("malformed")?;
    if hlen == 0 || end > bundle.len() {
        return Err("malformed");
    }
    let header = serde_json::from_slice(&bundle[8..end]).map_err(|_| "malformed")?;
    Ok((&bundle[..end], header, &bundle[end..]))
}

/// `SWR-MG-05`: identify the kind and the format version of a bundle before it is restored, with no key.
fn read_bundle_header(req: &Value) -> R<Value> {
    let bundle = unhex(req["bundle"].as_str().ok_or("bundle is required")?, "bundle")?;
    let Ok((_, header, _)) = split_bundle(&bundle) else {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    };
    let mut out = read_header(&json!({ "header": header }))?;
    if out["ok"] == true {
        // The restore asks for the right secret before it has a key. A header without a usable `kdf` is not a bundle.
        if let Err(reason) = bundle_key::check_kdf(&header["kdf"]) {
            return Ok(json!({ "ok": false, "reason": reason }));
        }
        out["kdf"] = header["kdf"].clone();
        out["method"] = header["kdf"]["method"].clone();
    }
    out["bundleHash"] = json!(bundle_hash(&bundle));
    Ok(out)
}

/// The cipher for the request's key. The decoded key is wiped when this returns, and the cipher wipes its own copy when it
/// is dropped (the `zeroize` feature of `aes-gcm`), `SWR-MG-28` (c).
fn cipher(req: &Value) -> R<Aes256Gcm> {
    let key = crate::secret_io::secret(req, "key", &["bundle"])?;
    if key.len() != KEY_LEN {
        return Err(format!("the key is {KEY_LEN} bytes"));
    }
    Aes256Gcm::new_from_slice(&key).map_err(|_| "bad key".to_string())
}

/// `SWR-MG-02`, `SWR-MG-03`: seal a bundle. Items the kind does not carry are refused whole, so a UHDR-only bundle can never
/// hold protocols, limits profiles or consent grants, and a per-install item can never be sealed into either kind.
fn seal_bundle(req: &Value) -> R<Value> {
    let kind = req["kind"].as_str().ok_or("kind is required")?;
    let carried = kind_items(kind)?;
    let items = req["items"].as_object().ok_or("items must be an object")?;
    if let Some(bad) = items.keys().find(|k| !carried.contains(&k.as_str())) {
        return Err(format!("'{bad}' is not carried by a {kind} bundle"));
    }
    let nonce = unhex(req["nonce"].as_str().ok_or("nonce is required")?, "nonce")?;
    if nonce.len() != NONCE_LEN {
        return Err(format!("the nonce is {NONCE_LEN} bytes"));
    }
    // The header says how the key was made, so a restore on a new phone knows what to ask for (`SWR-MG-02`, `SWR-MG-05`).
    let kdf = &req["kdf"];
    bundle_key::check_kdf(kdf).map_err(|e| format!("kdf: {e}"))?;
    // `SWR-MG-29` (b): the epoch is named by the caller, which holds the count, and is at least 1 (0 means "seen nothing").
    let epoch = req["epoch"].as_u64().filter(|e| *e >= 1).ok_or("epoch is required and is at least 1")?;
    let header = json!({ "kind": kind, "formatVersion": FORMAT_VERSION, "nonce": hex(&nonce), "kdf": kdf }).to_string().into_bytes();
    let mut aad = Vec::with_capacity(8 + header.len());
    aad.extend_from_slice(MAGIC);
    aad.extend_from_slice(&(header.len() as u32).to_be_bytes());
    aad.extend_from_slice(&header);
    let plain = json!({ "epoch": epoch, "items": items }).to_string().into_bytes();
    let ct = cipher(req)?
        .encrypt(Nonce::from_slice(&nonce), Payload { msg: &plain, aad: &aad })
        .map_err(|_| "encryption failed".to_string())?;
    aad.extend_from_slice(&ct);
    Ok(json!({ "bundle": hex(&aad), "bundleHash": bundle_hash(&aad), "kind": kind, "formatVersion": FORMAT_VERSION, "epoch": epoch }))
}

/// `SWR-MG-02`, `SWR-MG-05`: open a bundle with the user's key. A newer version or an unknown kind is refused before any
/// decryption; a wrong key and an edited bundle are indistinguishable by design (`authentication_failed`), and nothing is
/// returned from either. The items it returns are checked against the kind's list, so a forged body cannot bring an item
/// the kind does not carry.
///
/// `SWR-MG-29` (c): the caller says the highest epoch this phone has sealed or opened (`seenEpoch`; 0 when it has seen
/// none). It is required, so no caller opens a bundle without the check by leaving it out. A bundle whose epoch is lower is
/// refused whole (`older_bundle`) and its items are not returned. A body with no epoch does not open (`malformed`).
fn open_bundle(req: &Value) -> R<Value> {
    let bundle = unhex(req["bundle"].as_str().ok_or("bundle is required")?, "bundle")?;
    let seen = req["seenEpoch"].as_u64().ok_or("seenEpoch is required (0 for a phone that has seen no bundle)")?;
    let Ok((aad, header, ct)) = split_bundle(&bundle) else {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    };
    let checked = read_header(&json!({ "header": header }))?;
    if checked["ok"] != true {
        return Ok(checked);
    }
    let kind = checked["kind"].as_str().unwrap_or_default();
    if let Err(reason) = bundle_key::check_kdf(&header["kdf"]) {
        return Ok(json!({ "ok": false, "reason": reason }));
    }
    let Some(nonce) = header["nonce"].as_str().and_then(|n| unhex(n, "nonce").ok()).filter(|n| n.len() == NONCE_LEN) else {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    };
    let Ok(plain) = cipher(req)?.decrypt(Nonce::from_slice(&nonce), Payload { msg: ct, aad }) else {
        return Ok(json!({ "ok": false, "reason": "authentication_failed" }));
    };
    let Ok(body) = serde_json::from_slice::<Value>(&plain) else {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    };
    // The key holder sealed this body, so a missing or zero epoch is a bundle this build did not make: it gets no place in
    // the order, and is never read as "the oldest".
    let Some(epoch) = body["epoch"].as_u64().filter(|e| *e >= 1) else {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    };
    if epoch < seen {
        return Ok(json!({ "ok": false, "reason": "older_bundle", "epoch": epoch, "seenEpoch": seen }));
    }
    let Some(mut items) = body["items"].as_object().cloned() else {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    };
    items.retain(|k, _| !RETIRED_ITEMS.contains(&k.as_str()));
    let carried = kind_items(kind)?;
    if items.keys().any(|k| !carried.contains(&k.as_str())) {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    }
    Ok(json!({ "ok": true, "kind": kind, "formatVersion": checked["formatVersion"], "epoch": epoch, "items": items, "bundleHash": bundle_hash(&bundle) }))
}

pub fn migration_json(request: &str) -> Result<String, String> {
    let req: Value = serde_json::from_str(request).map_err(|e| format!("bad migration request: {e}"))?;
    let op = req["op"].as_str().ok_or("op is required")?;
    let out = if op == "flow" { crate::migration_flow::flow(&req)? } else { dispatch(op, &req)? };
    let _: &Map<String, Value> = out.as_object().ok_or("a migration answer is an object")?;
    Ok(out.to_string())
}

/// One operation by name, for `migration_json` and for the flows (`migration_flow`), which ask the same rules in order.
pub(crate) fn dispatch(op: &str, req: &Value) -> R<Value> {
    if let Some(answer) = bundle_key::op(op, req) {
        return answer;
    }
    migration_op(op, req)
}

fn migration_op(op: &str, req: &Value) -> R<Value> {
    Ok(match op {
        "inventory" => inventory(req)?,
        "itemKeys" => {
            let mut m = key_map();
            m["sealedAtRest"] = json!(sealed_at_rest());
            m["deviceTier"] = json!(device_tier());
            m
        }
        "readHeader" => read_header(req)?,
        "restoreReport" => restore_report(req)?,
        "mergeWithdrawal" => merge_withdrawal(req)?,
        "applyWithdrawal" => apply_withdrawal(req)?,
        "consentAfterRestore" => consent_after_restore(req)?,
        "offerRoutes" => offer_routes(req)?,
        "claimDecision" => claim_decision(req)?,
        "claimReadback" => claim_readback(req)?,
        "clearDecision" => clear_decision(req)?,
        "confirmationTag" => confirmation_tag(req)?,
        "claimTag" => claim_tag(req)?,
        "clearPlan" => clear_plan(req)?,
        "probeResult" => probe_result(req)?,
        "objectNames" => crate::object_route::names(),
        "sealBundle" => seal_bundle(req)?,
        "readBundleHeader" => read_bundle_header(req)?,
        "openBundle" => open_bundle(req)?,
        other => {
            return crate::secret_io::op(other, req)
                .or_else(|| crate::link::op(other, req))
                .or_else(|| crate::migration_text::op(other, req))
                .or_else(|| crate::at_rest::op(other, req))
                .or_else(|| crate::archive::op(other, req))
                .or_else(|| crate::analytics::op(other, req))
                .or_else(|| crate::ohttp::op(other, req))
                .or_else(|| crate::sigv4::op(other, req))
                .or_else(|| crate::primitives::op(other, req))
                .unwrap_or_else(|| Err(format!("unknown migration operation '{other}'")))
        }
    })
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
    const TAG: &str = "aa";

    fn try_claim(store: &Store, token: &str, hash: &str) -> bool {
        let existing = store.get("claim").unwrap_or(Value::Null);
        let d = ask(json!({"op":"claimDecision","kind":"migrate","route":{"atomicClaim":true},"token":token,"bundleHash":hash,"claimTag":TAG,"existingClaim":existing}));
        if d["action"] != "write_claim" {
            return d["action"] == "already_ours";
        }
        store.create_if_absent("claim", json!({"token":token,"bundleHash":hash,"tag":TAG}));
        ask(json!({"op":"claimReadback","token":token,"bundleHash":hash,"claimTag":TAG,"readBack":store.get("claim").unwrap_or(Value::Null)}))["owned"] == true
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
        let r = ask(json!({"op":"claimDecision","kind":"uhdr","route":{"atomicClaim":false},"token":"t","bundleHash":"h","claimTag":TAG,"existingClaim":null}));
        assert_eq!(r["action"], "no_claim_needed");
        let r = ask(json!({"op":"claimDecision","kind":"migrate","route":{"atomicClaim":false},"token":"t","bundleHash":"h","claimTag":TAG,"existingClaim":null}));
        assert_eq!(r["action"], "route_not_offered");
    }

    // @verifies SWR-MG-07
    #[test]
    fn a_claim_written_but_not_read_back_is_not_owned() {
        let r = ask(json!({"op":"claimReadback","token":"t","bundleHash":"h","claimTag":TAG,"readBack":null}));
        assert_eq!(r["owned"], false);
        assert_eq!(r["reason"], "not_read_back");
        let r = ask(json!({"op":"claimReadback","token":"t","bundleHash":"h","claimTag":TAG,"readBack":{"token":"u","bundleHash":"h","tag":TAG}}));
        assert_eq!(r["owned"], false);
    }

    // @verifies SWR-MG-07
    #[test]
    fn a_marker_with_no_tag_a_wrong_tag_or_another_bundle_is_refused_as_unauthentic_and_a_genuine_other_claim_as_claimed() {
        let d = |claim: Value| ask(json!({"op":"claimDecision","kind":"migrate","route":{"atomicClaim":true},"token":"t","bundleHash":"h","claimTag":TAG,"existingClaim":claim}))["action"].clone();
        assert_eq!(d(json!({"token":"x","bundleHash":"h"})), "refuse_unauthentic", "no tag");
        assert_eq!(d(json!({"token":"x","bundleHash":"h","tag":"bb"})), "refuse_unauthentic", "wrong tag");
        assert_eq!(d(json!({"token":"t","bundleHash":"h","tag":"bb"})), "refuse_unauthentic", "our token is no proof");
        assert_eq!(d(json!({"token":"x","bundleHash":"g","tag":TAG})), "refuse_unauthentic", "another bundle");
        assert_eq!(d(json!({"token":"x","bundleHash":"h","tag":TAG})), "refuse_claimed", "another phone with the key");
        assert_eq!(d(json!({"token":"t","bundleHash":"h","tag":TAG})), "already_ours");
        assert!(migration_json(&json!({"op":"claimDecision","kind":"migrate","route":{"atomicClaim":true},"token":"t","bundleHash":"h","existingClaim":null}).to_string()).is_err(), "no tag, no claim");
    }

    // @verifies SWR-MG-07
    #[test]
    fn the_claim_tag_needs_the_bundle_key_and_is_bound_to_the_bundle_and_apart_from_the_confirmation_tag() {
        let tag = |op: &str, key: &str, hash: &str| ask(json!({"op":op,"key":key,"bundleHash":hash}))["tag"].as_str().unwrap().to_string();
        let (k1, k2) = (hex(&[0x11; 32]), hex(&[0x12; 32]));
        let t = tag("claimTag", &k1, "h");
        assert_eq!(t, tag("claimTag", &k1, "h"));
        assert_ne!(t, tag("claimTag", &k2, "h"));
        assert_ne!(t, tag("claimTag", &k1, "g"));
        assert_ne!(t, tag("confirmationTag", &k1, "h"), "a confirmation tag is no claim tag");
        let mut mk = [0u8; 32];
        hkdf::Hkdf::<sha2::Sha256>::new(None, &[0x11; 32]).expand(b"np-migration-claim-v1", &mut mk).unwrap();
        let mut m = <Hmac<Sha256> as Mac>::new_from_slice(&mk).unwrap();
        m.update(b"h");
        assert_eq!(t, hex(&m.finalize().into_bytes()));
    }

    // @verifies SWR-MG-08
    #[test]
    fn the_old_phone_clears_only_on_a_matching_confirmation() {
        let e = json!({"bundleHash":"h","tag":"aa"});
        let none = ask(json!({"op":"clearDecision","bundleHash":"h","expected":e,"confirmation":null}));
        assert_eq!((none["clear"].clone(), none["reason"].clone()), (json!(false), json!("no_confirmation")));
        let other = ask(json!({"op":"clearDecision","bundleHash":"h","expected":e,"confirmation":{"bundleHash":"x","tag":"aa"}}));
        assert_eq!((other["clear"].clone(), other["reason"].clone()), (json!(false), json!("other_bundle")));
        let ok = ask(json!({"op":"clearDecision","bundleHash":"h","expected":e,"confirmation":{"bundleHash":"h","tag":"aa"}}));
        assert_eq!(ok["clear"], true);
        let empty = ask(json!({"op":"clearDecision","bundleHash":"","expected":{"bundleHash":"","tag":"aa"},"confirmation":{"bundleHash":"","tag":"aa"}}));
        assert_eq!(empty["clear"], false, "no hash is no match");
    }

    // The hash is computed from ciphertext the backup location holds, so the hash alone confirms nothing (privacy review of
    // `common/`, 2026-11-15, Finding 1).
    // @verifies SWR-MG-08
    #[test]
    fn a_confirmation_without_the_tag_this_phone_recorded_does_not_clear() {
        let e = json!({"bundleHash":"h","tag":"aa"});
        for forged in [json!({"bundleHash":"h"}), json!({"bundleHash":"h","tag":""}), json!({"bundleHash":"h","tag":"ab"}), json!({"bundleHash":"h","tag":"aaa"})] {
            let r = ask(json!({"op":"clearDecision","bundleHash":"h","expected":e,"confirmation":forged}));
            assert_eq!((r["clear"].clone(), r["reason"].clone()), (json!(false), json!("no_confirmation")), "{forged}");
        }
        // A phone with no record of the bundle clears against nothing, whatever the confirmation says.
        for expected in [Value::Null, json!({"bundleHash":"h"}), json!({"bundleHash":"h","tag":""})] {
            let r = ask(json!({"op":"clearDecision","bundleHash":"h","expected":expected,"confirmation":{"bundleHash":"h","tag":""}}));
            assert_eq!(r["clear"], false, "{expected}");
        }
        // The record is of another bundle: the one at the backup location is not the one this phone wrote.
        let r = ask(json!({"op":"clearDecision","bundleHash":"h","expected":{"bundleHash":"g","tag":"aa"},"confirmation":{"bundleHash":"h","tag":"aa"}}));
        assert_eq!((r["clear"].clone(), r["reason"].clone()), (json!(false), json!("other_bundle")));
    }

    // @verifies SWR-MG-08
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn the_confirmation_tag_needs_the_bundle_key_and_is_bound_to_the_bundle() {
        let tag = |key: &str, hash: &str| ask(json!({"op":"confirmationTag","key":key,"bundleHash":hash}))["tag"].as_str().unwrap().to_string();
        let (k1, k2) = ("11".repeat(32), "12".repeat(32));
        assert_eq!(tag(&k1, "h").len(), 64);
        assert_eq!(tag(&k1, "h"), tag(&k1, "h"));
        assert_ne!(tag(&k1, "h"), tag(&k2, "h"), "another key gives another tag");
        assert_ne!(tag(&k1, "h"), tag(&k1, "g"), "another bundle gives another tag");
        // HKDF-SHA-256 (no salt, the label as info, 32 bytes) then HMAC-SHA-256 over the hash text, worked independently.
        let mut mk = [0u8; 32];
        hkdf::Hkdf::<sha2::Sha256>::new(None, &[0x11; 32]).expand(b"np-migration-confirmation-v1", &mut mk).unwrap();
        let mut m = <hmac::Hmac<sha2::Sha256> as hmac::Mac>::new_from_slice(&mk).unwrap();
        hmac::Mac::update(&mut m, b"h");
        assert_eq!(tag(&k1, "h"), hex(&hmac::Mac::finalize(m).into_bytes()));
        assert!(migration_json(&json!({"op":"confirmationTag","key":k1,"bundleHash":""}).to_string()).is_err(), "no hash, no tag");
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
        for want in ["protocols", "sessionHistory", "withdrawalState", "limitsProfiles"] {
            assert!(carried.contains(&want), "{want}");
        }
        // The warranty owner's two items are among the never-moved: they are not a user's (`REQ-SHDR-10`).
        for never in ["researchDeviceToken", "warrantyConsentFlag", "characterisationEnrolment", "lastPairedHelmetId", "launchCount", "consentGrants"] {
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
        let u = ask(json!({"op":"restoreReport","kind":"uhdr","present":["sessionHistory","withdrawalState"]}));
        assert_eq!(u["missing"].as_array().unwrap().len(), 0);
        assert!(u["omitted"].to_string().contains("protocols"));
    }

    // @verifies SWR-MG-04
    #[test]
    fn a_restore_never_resets_a_withdrawal() {
        let local = json!({"blanketConsentWithdrawnAt":null,"studyWithdrawals":{"s1":"2026-02-01#000001"},"categoryWithdrawals":{}});
        let incoming = json!({"blanketConsentWithdrawnAt":"2026-03-01#000001","studyWithdrawals":{"s2":"2026-03-02#000001","s1":"2026-01-01#000001"},"categoryWithdrawals":{"c1":"2026-03-03#000001"},"contactConsentWithdrawnAt":"2026-03-04#000001"});
        let m = ask(json!({"op":"mergeWithdrawal","local":local,"incoming":incoming}));
        assert_eq!(m["blanketConsentWithdrawnAt"], "2026-03-01#000001");
        assert_eq!(m["studyWithdrawals"], json!({"s1":"2026-02-01#000001","s2":"2026-03-02#000001"}), "the later time per id stands");
        assert_eq!(m["categoryWithdrawals"], json!({"c1":"2026-03-03#000001"}));
        assert_eq!(m["contactConsentWithdrawnAt"], "2026-03-04#000001");
        // The other way round: a fresh install brings none, the bundle's stands; an older one never replaces a later one.
        let m = ask(json!({"op":"mergeWithdrawal","local":{"blanketConsentWithdrawnAt":"2026-05-01#000001"},"incoming":{"blanketConsentWithdrawnAt":"2026-03-01#000001"}}));
        assert_eq!(m["blanketConsentWithdrawnAt"], "2026-05-01#000001");
    }

    // @verifies SWR-MG-04
    #[test]
    fn a_restore_applies_only_a_later_withdrawal_and_never_a_grant() {
        let held = json!({"blanketConsentWithdrawnAt":"2026-05-01#000001","contactConsentWithdrawnAt":null,
            "studyWithdrawals":{"s1":"2026-02-01#000001"},"categoryWithdrawals":{"c1":"2026-04-01#000001"}});
        let incoming = json!({"blanketConsentWithdrawnAt":"2026-03-01#000001","contactConsentWithdrawnAt":"2026-03-04#000001",
            "studyWithdrawals":{"s1":"2026-02-01#000001","s2":"2026-03-02#000001"},"categoryWithdrawals":{"c1":"2026-06-01#000001"}});
        let r = ask(json!({"op":"applyWithdrawal","held":held,"incoming":incoming}));
        // Older than the held blanket withdrawal and equal for s1: not applied. New, or later: applied.
        assert_eq!(r["applied"]["times"], json!(["contactConsentWithdrawnAt"]));
        assert_eq!(r["applied"]["studyWithdrawals"], json!(["s2"]));
        assert_eq!(r["applied"]["categoryWithdrawals"], json!(["c1"]));
        assert_eq!(r["merged"]["blanketConsentWithdrawnAt"], "2026-05-01#000001");
        assert_eq!(r["merged"]["categoryWithdrawals"]["c1"], "2026-06-01#000001");
        let none = ask(json!({"op":"applyWithdrawal","held":{},"incoming":{}}));
        assert_eq!(none["applied"]["times"], json!([]));
    }

    // @verifies SWR-MG-04
    #[test]
    fn never_asked_and_said_stop_stay_apart() {
        let never = ask(json!({"op":"mergeWithdrawal","local":{},"incoming":{}}));
        assert!(never["blanketConsentWithdrawnAt"].is_null(), "never asked stays null");
        let stopped = ask(json!({"op":"mergeWithdrawal","local":{},"incoming":{"blanketConsentWithdrawnAt":"2026-03-01#000001"}}));
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

    const KEY: &str = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    const NONCE: &str = "a0a1a2a3a4a5a6a7a8a9aaab";

    fn kdf(method: &str) -> Value {
        ask(json!({"op":"bundleKdf","method":method,"salt":"5a".repeat(32)}))["kdf"].clone()
    }

    fn sealed(kind: &str, items: Value) -> Value {
        ask(json!({"op":"sealBundle","epoch":1,"kind":kind,"key":KEY,"nonce":NONCE,"items":items,"kdf":kdf("recoveryCode")}))
    }

    // @verifies SWR-MG-02
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_sealed_bundle_opens_with_the_key_and_returns_what_was_sealed() {
        let items = json!({"sessionHistory":[{"id":1}],"withdrawalState":{"blanketConsentWithdrawnAt":"2026-10-01#000001"}});
        let b = sealed("uhdr", items.clone());
        let opened = ask(json!({"op":"openBundle","seenEpoch":0,"bundle":b["bundle"],"key":KEY}));
        assert_eq!(opened["ok"], true);
        assert_eq!(opened["items"], items);
        assert_eq!(opened["bundleHash"], b["bundleHash"]);
    }

    // @verifies SWR-MG-02
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn the_bundle_is_unreadable_without_the_key_and_its_plaintext_is_not_in_it() {
        let b = sealed("uhdr", json!({"sessionHistory":["a-very-distinctive-session-note"]}));
        let bytes = unhex(b["bundle"].as_str().unwrap(), "bundle").unwrap();
        assert!(!bytes.windows(10).any(|w| w == b"distinctiv"), "plaintext must not appear in the bundle");
        let wrong = "ff".repeat(32);
        let r = ask(json!({"op":"openBundle","seenEpoch":0,"bundle":b["bundle"],"key":wrong}));
        assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("authentication_failed")));
        assert!(r.get("items").is_none());
    }

    // @verifies SWR-MG-02
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn an_edited_bundle_is_refused_and_a_relabelled_kind_does_not_authenticate() {
        let b = sealed("uhdr", json!({"sessionHistory":[]}));
        let mut bytes = unhex(b["bundle"].as_str().unwrap(), "bundle").unwrap();
        let last = bytes.len() - 1;
        bytes[last] ^= 1;
        let r = ask(json!({"op":"openBundle","seenEpoch":0,"bundle":hex(&bytes),"key":KEY}));
        assert_eq!(r["reason"], "authentication_failed");
        // Re-label the cleartext kind as migrate: the header is associated data, so the tag no longer verifies.
        let orig = unhex(b["bundle"].as_str().unwrap(), "bundle").unwrap();
        let h = u32::from_be_bytes([orig[4], orig[5], orig[6], orig[7]]) as usize;
        let header = String::from_utf8(orig[8..8 + h].to_vec()).unwrap().replace("\"uhdr\"", "\"migrate\"").into_bytes();
        let mut relabelled = b"NPBK".to_vec();
        relabelled.extend_from_slice(&(header.len() as u32).to_be_bytes());
        relabelled.extend_from_slice(&header);
        relabelled.extend_from_slice(&orig[8 + h..]);
        let r = ask(json!({"op":"openBundle","seenEpoch":0,"bundle":hex(&relabelled),"key":KEY}));
        assert_eq!(r["reason"], "authentication_failed");
    }

    // @verifies SWR-MG-05
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn the_kind_and_version_are_readable_before_restore_with_no_key() {
        for kind in ["uhdr", "migrate"] {
            let b = sealed(kind, json!({}));
            let h = ask(json!({"op":"readBundleHeader","bundle":b["bundle"]}));
            assert_eq!((h["ok"].clone(), h["kind"].clone(), h["formatVersion"].clone()), (json!(true), json!(kind), json!(FORMAT_VERSION)));
            assert_eq!(h["bundleHash"], b["bundleHash"]);
        }
        assert_eq!(ask(json!({"op":"readBundleHeader","bundle":"00"}))["reason"], "malformed");
        assert_eq!(ask(json!({"op":"readBundleHeader","bundle":hex(b"NPBK\0\0\0\x05{}")}))["reason"], "malformed");
    }

    // @verifies SWR-MG-05
    #[test]
    fn a_bundle_one_version_newer_is_refused_whole_before_any_decryption() {
        let header = json!({"kind":"uhdr","formatVersion":FORMAT_VERSION + 1,"nonce":NONCE,"kdf":kdf("passphrase")}).to_string().into_bytes();
        let mut bytes = b"NPBK".to_vec();
        bytes.extend_from_slice(&(header.len() as u32).to_be_bytes());
        bytes.extend_from_slice(&header);
        bytes.extend_from_slice(&[0u8; 32]);
        let r = ask(json!({"op":"openBundle","seenEpoch":0,"bundle":hex(&bytes),"key":KEY}));
        assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("newer_version")));
        assert!(r.get("items").is_none());
    }

    /// A bundle of any version with any body, sealed under `KEY` the way `sealBundle` does: what an older or a foreign
    /// build that holds the key could have written.
    fn sealed_raw(version: u64, body: Value) -> String {
        let header = json!({"kind":"uhdr","formatVersion":version,"nonce":NONCE,"kdf":kdf("recoveryCode")}).to_string().into_bytes();
        let mut bytes = b"NPBK".to_vec();
        bytes.extend_from_slice(&(header.len() as u32).to_be_bytes());
        bytes.extend_from_slice(&header);
        let ct = Aes256Gcm::new_from_slice(&unhex(KEY, "key").unwrap())
            .unwrap()
            .encrypt(Nonce::from_slice(&unhex(NONCE, "nonce").unwrap()), Payload { msg: body.to_string().as_bytes(), aad: &bytes })
            .unwrap();
        bytes.extend_from_slice(&ct);
        hex(&bytes)
    }

    fn sealed_at(epoch: u64, items: Value) -> Value {
        ask(json!({"op":"sealBundle","epoch":epoch,"kind":"uhdr","key":KEY,"nonce":NONCE,"items":items,"kdf":kdf("recoveryCode")}))
    }

    // @verifies SWR-MG-29#b
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_bundle_carries_its_epoch_inside_the_seal_and_not_in_the_clear() {
        let b = sealed_at(41, json!({"sessionHistory":[]}));
        assert_eq!(b["epoch"], 41);
        let opened = ask(json!({"op":"openBundle","seenEpoch":0,"bundle":b["bundle"],"key":KEY}));
        assert_eq!((opened["ok"].clone(), opened["epoch"].clone()), (json!(true), json!(41)));
        // The backup location reads the header and nothing else: neither it nor the keyless answer names the epoch.
        let bytes = unhex(b["bundle"].as_str().unwrap(), "bundle").unwrap();
        let h = u32::from_be_bytes([bytes[4], bytes[5], bytes[6], bytes[7]]) as usize;
        let header: Value = serde_json::from_slice(&bytes[8..8 + h]).unwrap();
        assert!(header.get("epoch").is_none(), "{header}");
        assert!(ask(json!({"op":"readBundleHeader","bundle":b["bundle"]})).get("epoch").is_none());
        // Two bundles that differ only in epoch are the same length, so the size does not count backups either.
        assert_eq!(sealed_at(7, json!({}))["bundle"].as_str().unwrap().len(), sealed_at(8, json!({}))["bundle"].as_str().unwrap().len());
        // The epoch is the caller's count and is never left out or zero: 0 is what a phone that has seen nothing holds.
        for bad in [json!(null), json!(0), json!(-1), json!("3"), json!(1.5)] {
            let mut req = json!({"op":"sealBundle","kind":"uhdr","key":KEY,"nonce":NONCE,"items":{},"kdf":kdf("recoveryCode")});
            if !bad.is_null() {
                req["epoch"] = bad.clone();
            }
            assert!(migration_json(&req.to_string()).is_err(), "sealed with epoch {bad}");
        }
    }

    // @verifies SWR-MG-29#c
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_phone_that_has_seen_a_later_epoch_refuses_an_older_bundle_whole() {
        let old = sealed_at(3, json!({"sessionHistory":["from-before-the-withdrawal"]}));
        // A phone that sealed or opened epoch 5 is handed the bundle of epoch 3 by the backup location.
        let r = ask(json!({"op":"openBundle","seenEpoch":5,"bundle":old["bundle"],"key":KEY}));
        assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("older_bundle")));
        assert_eq!((r["epoch"].clone(), r["seenEpoch"].clone()), (json!(3), json!(5)));
        assert!(r.get("items").is_none(), "nothing of an older bundle is returned");
        // The bundle it last sealed or opened, and any later one, open.
        for seen in [3, 2, 0] {
            assert_eq!(ask(json!({"op":"openBundle","seenEpoch":seen,"bundle":old["bundle"],"key":KEY}))["ok"], true, "seen {seen}");
        }
        // The check cannot be skipped by leaving the number out, and a wrong key learns nothing about the epoch.
        assert!(migration_json(&json!({"op":"openBundle","bundle":old["bundle"],"key":KEY}).to_string()).is_err());
        let wrong = ask(json!({"op":"openBundle","seenEpoch":5,"bundle":old["bundle"],"key":"ff".repeat(32)}));
        assert_eq!(wrong["reason"], "authentication_failed");
        assert!(wrong.get("epoch").is_none());
    }

    // @verifies SWR-MG-29#d
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_phone_that_has_seen_nothing_opens_an_old_bundle_and_the_epoch_does_not_stop_it() {
        // The limit of the epoch, pinned so nobody reads more into it: with nothing seen, an old bundle is as good as the
        // newest. A new phone, a fresh install and a phone after an erase all hold 0.
        let old = sealed_at(1, json!({"sessionHistory":[]}));
        let _newer = sealed_at(9, json!({"sessionHistory":[]}));
        let r = ask(json!({"op":"openBundle","seenEpoch":0,"bundle":old["bundle"],"key":KEY}));
        assert_eq!((r["ok"].clone(), r["epoch"].clone()), (json!(true), json!(1)));
    }

    // @verifies SWR-MG-05
    // @verifies SWR-MG-29#b
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn the_format_version_stays_1_and_a_body_with_no_epoch_does_not_open() {
        // No version bump (the principal, 2026-11-19): no bundle from before the epoch should exist.
        let b = sealed_at(1, json!({}));
        assert_eq!(b["formatVersion"], 1);
        assert_eq!(ask(json!({"op":"readBundleHeader","bundle":b["bundle"]}))["formatVersion"], 1);
        assert_eq!(ask(json!({"op":"readHeader","header":{"kind":"uhdr","formatVersion":FORMAT_VERSION + 1}}))["reason"], "newer_version");
        // If one did exist, or a body named no usable epoch, it is not opened, whatever the phone has seen: it is not
        // taken for "the oldest", which a phone that has seen nothing would accept.
        for body in [json!({"items":{"sessionHistory":[]}}), json!({"epoch":0,"items":{}}), json!({"epoch":"4","items":{}})] {
            for seen in [0, 1] {
                let r = ask(json!({"op":"openBundle","seenEpoch":seen,"bundle":sealed_raw(1, body.clone()),"key":KEY}));
                assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("malformed")), "{body} seen {seen}");
                assert!(r.get("items").is_none());
            }
        }
    }

    // @verifies SWR-MG-03
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_bundle_cannot_carry_what_its_kind_omits_or_what_belongs_to_the_install() {
        for (kind, item) in [("uhdr","protocols"),("uhdr","limitsProfiles"),("uhdr","consentGrants"),("migrate","consentGrants"),("migrate","researchDeviceToken"),("migrate","lastPairedHelmetId")] {
            let r = migration_json(&json!({"op":"sealBundle","epoch":1,"kind":kind,"key":KEY,"nonce":NONCE,"items":{item:[]},"kdf":kdf("passphrase")}).to_string());
            assert!(r.is_err(), "{kind} must not seal {item}");
        }
        assert!(migration_json(&json!({"op":"sealBundle","epoch":1,"kind":"migrate","key":KEY,"nonce":NONCE,"items":{"protocols":[]},"kdf":kdf("passphrase")}).to_string()).is_ok());
    }

    // @verifies SWR-MG-02
    #[test]
    fn a_key_or_nonce_of_the_wrong_length_is_refused() {
        let short = migration_json(&json!({"op":"sealBundle","epoch":1,"kind":"uhdr","key":"00","nonce":NONCE,"items":{},"kdf":kdf("passphrase")}).to_string());
        assert!(short.is_err());
        let nonce = migration_json(&json!({"op":"sealBundle","epoch":1,"kind":"uhdr","key":KEY,"nonce":"00","items":{},"kdf":kdf("passphrase")}).to_string());
        assert!(nonce.is_err());
    }

    // @verifies SWR-MG-02
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_recovery_code_round_trips_through_its_text_and_survives_eye_and_case_errors() {
        let bytes: Vec<u8> = (0u8..32).map(|i| i.wrapping_mul(37).wrapping_add(11)).collect();
        let code = ask(json!({"op":"encodeRecoveryCode","bytes":hex(&bytes)}))["code"].as_str().unwrap().to_string();
        assert_eq!(code.len(), 56 + 13, "14 groups of four with 13 hyphens");
        for variant in [code.clone(), code.to_lowercase(), code.replace('-', " "), code.replace('-', "")] {
            let r = ask(json!({"op":"parseRecoveryCode","code":variant}));
            assert_eq!((r["ok"].clone(), r["secret"].clone()), (json!(true), json!(hex(&bytes))));
        }
        // O reads as 0 and I or L as 1, the look-alikes Crockford leaves out of the alphabet.
        let eyed = code.replace('0', "O").replace('1', "l");
        assert_eq!(ask(json!({"op":"parseRecoveryCode","code":eyed}))["secret"], json!(hex(&bytes)));
    }

    // @verifies SWR-MG-02
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_mistyped_recovery_code_is_a_typing_error_and_not_a_wrong_key() {
        let bytes = vec![7u8; 32];
        let code = ask(json!({"op":"encodeRecoveryCode","bytes":hex(&bytes)}))["code"].as_str().unwrap().to_string();
        let mut chars: Vec<char> = code.chars().collect();
        chars[0] = if chars[0] == '2' { '3' } else { '2' };
        let r = ask(json!({"op":"parseRecoveryCode","code":chars.iter().collect::<String>()}));
        assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("bad_checksum")));
        assert_eq!(ask(json!({"op":"parseRecoveryCode","code":"ABCD"}))["reason"], "malformed");
        assert_eq!(ask(json!({"op":"parseRecoveryCode","code":code.replace('-', "").replace(&code[..1], "U")}))["reason"], "malformed");
        assert!(migration_json(&json!({"op":"encodeRecoveryCode","bytes":"00"}).to_string()).is_err());
    }

    // @verifies SWR-MG-02
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn two_texts_never_name_one_code_because_the_padding_bits_are_zero() {
        let bytes = vec![0u8; 32];
        let code = ask(json!({"op":"encodeRecoveryCode","bytes":hex(&bytes)}))["code"].as_str().unwrap().replace('-', "");
        // Set a padding bit in the 52nd character (its low 4 bits carry no data) and re-derive a matching text: refused.
        let mut forged: Vec<char> = code.chars().collect();
        forged[51] = '1';
        let r = ask(json!({"op":"parseRecoveryCode","code":forged.iter().collect::<String>()}));
        assert_eq!(r["ok"], false);
    }

    // @verifies SWR-MG-02
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_passphrase_is_normalised_and_must_be_long_enough() {
        // NFKC: the full-width form and the plain form are one secret, so another phone's keyboard gives the same key.
        let plain = ask(json!({"op":"passphraseSecret","passphrase":"correct horse battery"}));
        let wide = ask(json!({"op":"passphraseSecret","passphrase":"ｃｏｒｒｅｃｔ horse battery"}));
        assert_eq!(plain["secret"], wide["secret"]);
        // A composed and a decomposed é are one secret.
        let a = ask(json!({"op":"passphraseSecret","passphrase":"caf\u{e9} au lait noir"}));
        let b = ask(json!({"op":"passphraseSecret","passphrase":"cafe\u{301} au lait noir"}));
        assert_eq!(a["secret"], b["secret"]);
        let short = ask(json!({"op":"passphraseSecret","passphrase":"too short"}));
        assert_eq!((short["ok"].clone(), short["reason"].clone()), (json!(false), json!("too_short")));
        assert_eq!(short["minChars"], bundle_key::PASSPHRASE_MIN_CHARS);
        // Characters, not bytes: twelve two-byte characters pass, eleven do not.
        assert_eq!(ask(json!({"op":"passphraseSecret","passphrase":"é".repeat(12)}))["ok"], true);
        assert_eq!(ask(json!({"op":"passphraseSecret","passphrase":"é".repeat(11)}))["ok"], false);
    }

    // @verifies SWR-MG-05
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn the_header_names_how_the_key_was_made_before_any_key_exists() {
        for method in ["recoveryCode", "passphrase"] {
            let b = ask(json!({"op":"sealBundle","epoch":1,"kind":"uhdr","key":KEY,"nonce":NONCE,"items":{},"kdf":kdf(method)}));
            let h = ask(json!({"op":"readBundleHeader","bundle":b["bundle"]}));
            assert_eq!(h["method"], method);
            assert_eq!(h["kdf"]["salt"], "5a".repeat(32));
            assert_eq!((h["kdf"]["memoryKiB"].clone(), h["kdf"]["iterations"].clone(), h["kdf"]["parallelism"].clone()), (json!(65536), json!(4), json!(1)));
        }
    }

    // @verifies SWR-MG-05
    #[test]
    fn a_bundle_cannot_ask_the_restoring_phone_for_a_cheaper_derivation() {
        let mut weak = kdf("passphrase");
        weak["memoryKiB"] = json!(8);
        assert!(migration_json(&json!({"op":"sealBundle","epoch":1,"kind":"uhdr","key":KEY,"nonce":NONCE,"items":{},"kdf":weak}).to_string()).is_err());
        // A header carrying such a block is refused whole on read, with no key needed.
        let header = json!({"kind":"uhdr","formatVersion":1,"nonce":NONCE,"kdf":weak}).to_string().into_bytes();
        let mut bytes = b"NPBK".to_vec();
        bytes.extend_from_slice(&(header.len() as u32).to_be_bytes());
        bytes.extend_from_slice(&header);
        bytes.extend_from_slice(&[0u8; 32]);
        let r = ask(json!({"op":"readBundleHeader","bundle":hex(&bytes)}));
        assert_eq!((r["ok"].clone(), r["reason"].clone()), (json!(false), json!("unsupported_kdf")));
        let o = ask(json!({"op":"openBundle","seenEpoch":0,"bundle":hex(&bytes),"key":KEY}));
        assert_eq!(o["reason"], "unsupported_kdf");
        // No kdf at all is not a bundle this build wrote.
        let none = json!({"kind":"uhdr","formatVersion":1,"nonce":NONCE}).to_string().into_bytes();
        let mut b2 = b"NPBK".to_vec();
        b2.extend_from_slice(&(none.len() as u32).to_be_bytes());
        b2.extend_from_slice(&none);
        b2.extend_from_slice(&[0u8; 32]);
        assert_eq!(ask(json!({"op":"readBundleHeader","bundle":hex(&b2)}))["ok"], false);
    }

    // @verifies SWR-MG-02
    #[test]
    fn an_unknown_key_method_or_a_short_salt_is_refused() {
        assert!(migration_json(&json!({"op":"bundleKdf","method":"fingerprint","salt":"5a".repeat(32)}).to_string()).is_err());
        assert!(migration_json(&json!({"op":"bundleKdf","method":"passphrase","salt":"5a"}).to_string()).is_err());
    }

    // @verifies REQ-PRES-05
    #[test]
    fn the_device_tier_is_a_small_sorted_subset_of_the_sealed_keys() {
        let m = ask(json!({"op":"itemKeys"}));
        let list = |name: &str| -> Vec<String> { m[name].as_array().unwrap().iter().map(|k| k.as_str().unwrap().to_string()).collect() };
        let (tier, sealed) = (list("deviceTier"), list("sealedAtRest"));
        let mut sorted = tier.clone();
        sorted.sort();
        sorted.dedup();
        assert_eq!(tier, sorted, "sorted, no duplicates");
        assert!(!tier.is_empty());
        for k in &tier {
            assert!(sealed.contains(k), "device-tier key {k} is a sealed key");
        }
        // Nothing about the person: no consent, history, onboarding or characterisation key is ever in it.
        for k in list("consent").into_iter().chain(m["carried"].as_object().unwrap().values().flat_map(|a| a.as_array().unwrap()).map(|k| k.as_str().unwrap().to_string())).chain(list("unsorted")) {
            assert!(!tier.contains(&k), "{k} says something about the person and is UHDR");
        }
    }

    // @verifies SWR-MG-06
    // @verifies SWR-MG-16#a
    #[test]
    fn the_sealed_set_is_every_stored_key_the_apps_sort() {
        let m = ask(json!({"op":"itemKeys"}));
        let sealed: Vec<String> = m["sealedAtRest"].as_array().unwrap().iter().map(|k| k.as_str().unwrap().to_string()).collect();
        let list = |name: &str| -> Vec<String> { m[name].as_array().unwrap().iter().map(|k| k.as_str().unwrap().to_string()).collect() };
        let mut sorted = sealed.clone();
        sorted.sort();
        sorted.dedup();
        assert_eq!(sealed, sorted, "sorted, no duplicates");
        for k in m["carried"].as_object().unwrap().values().flat_map(|a| a.as_array().unwrap()) {
            assert!(sealed.contains(&k.as_str().unwrap().to_string()), "carried key {k}");
        }
        for k in list("consent").into_iter().chain(list("perInstall")).chain(list("unsorted")) {
            assert!(sealed.contains(&k), "consent, per-install or unsorted key {k}");
        }
        assert_eq!(sealed.len(), all_keys().len(), "every key the apps store is sealed, so none is left in the clear by omission");
        assert!(sealed.contains(&"np.cvns.fault-ledger.last-acknowledged-session".to_string()), "the cardiac fault acknowledgement");
        assert!(sealed.contains(&"np.onboarding.bipa-accepted".to_string()), "the biometric-privacy consent record");
        assert!(sealed.iter().all(|k| all_keys().contains(k)), "every sealed key is a sorted key");
    }
}
