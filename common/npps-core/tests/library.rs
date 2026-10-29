//! The protocol library's rules (`library`): one set of decisions for every app. Each test names the requirement row
//! it verifies. The rules are pure functions of the JSON they are given, so these tests need no store.

use neurone_npps_core::api::library_json;
use serde_json::{json, Value};

fn call(req: Value) -> Value {
    serde_json::from_str(&library_json(&req.to_string()).expect("the operation succeeds")).unwrap()
}

fn refused(req: Value) -> String {
    library_json(&req.to_string()).expect_err("the operation is refused")
}

fn e(id: &str, family: &str, used: bool, version: &str, hw: &[u32]) -> Value {
    json!({ "id": id, "familyId": family, "used": used, "version": version, "hardwareVersions": hw })
}

// A used family of two variants (hardware versions 1 and 2), and an unused family of one.
fn library() -> Value {
    json!([e("a1", "FAM-A", true, "1.0", &[1]), e("a2", "FAM-A", false, "1.0", &[2]), e("b1", "FAM-B", false, "1.0", &[1])])
}

// @verifies SWR-PL-03
#[test]
fn an_unused_protocol_saves_in_place() {
    let r = call(json!({ "op": "saveCheck", "entries": library(), "id": "b1" }));
    assert_eq!(r["allowed"], true);
    let new = call(json!({ "op": "saveCheck", "entries": library(), "id": "not-yet-in-the-library" }));
    assert_eq!(new["allowed"], true, "a protocol the library does not hold yet is a first save");
}

// @verifies SWR-PL-02
// @verifies SWR-FM-03
#[test]
fn a_save_to_a_used_family_is_refused_and_offers_a_whole_family_clone() {
    let r = call(json!({ "op": "saveCheck", "entries": library(), "id": "a1" }));
    assert_eq!(r["allowed"], false);
    assert_eq!(r["reason"], "family_used");
    assert_eq!(r["offer"], "clone_family");
    // The sibling a2 was never uploaded, and it is immutable all the same: the use of any variant marks the family.
    let sib = call(json!({ "op": "saveCheck", "entries": library(), "id": "a2" }));
    assert_eq!(sib["allowed"], false);
    assert_eq!(sib["members"], json!(["a1", "a2"]));
}

// @verifies SWR-PL-02
#[test]
fn a_protocol_with_no_family_is_a_family_of_one() {
    let es = json!([{ "id": "solo", "used": true }, { "id": "other", "used": false }]);
    assert_eq!(call(json!({ "op": "saveCheck", "entries": es, "id": "solo" }))["allowed"], false);
    assert_eq!(call(json!({ "op": "saveCheck", "entries": es, "id": "other" }))["allowed"], true);
}

// @verifies SWR-FM-05
#[test]
fn a_used_family_cannot_be_deleted_and_can_be_archived() {
    let d = call(json!({ "op": "deleteCheck", "entries": library(), "id": "a2" }));
    assert_eq!(d["allowed"], false);
    assert_eq!(d["alternative"], "archive");
    assert_eq!(call(json!({ "op": "deleteCheck", "entries": library(), "id": "b1" }))["allowed"], true);
}

// @verifies SWR-WO-01
// @verifies SWR-FM-05
#[test]
fn archiving_a_family_removes_it_from_the_offered_list_and_keeps_every_variant() {
    let all = call(json!({ "op": "offered", "entries": library(), "archivedFamilies": [] }));
    assert_eq!(all["offered"], json!(["a1", "a2", "b1"]));
    let after = call(json!({ "op": "offered", "entries": library(), "archivedFamilies": ["FAM-A"] }));
    assert_eq!(after["offered"], json!(["b1"]));
    // The entries themselves are untouched: archive is a list state, and the definitions stay retrievable.
    assert_eq!(library().as_array().unwrap().len(), 3);
}

// @verifies SWR-FM-03
#[test]
fn a_clone_is_a_new_unused_family_with_a_copy_of_every_variant() {
    let r = call(json!({ "op": "cloneFamily", "entries": library(), "id": "a2", "newFamilyId": "FAM-C", "newIds": ["c1", "c2"] }));
    let clones = r["clones"].as_array().unwrap();
    assert_eq!(clones.len(), 2);
    assert!(clones.iter().all(|c| c["familyId"] == "FAM-C" && c["used"] == false && c["clonedFromFamily"] == "FAM-A"));
    assert_eq!(clones[0]["sourceId"], "a1");
    assert_eq!(clones[1]["newId"], "c2");
    assert!(refused(json!({ "op": "cloneFamily", "entries": library(), "id": "a1", "newFamilyId": "FAM-C", "newIds": ["c1"] })).contains("2 variant"));
    assert!(refused(json!({ "op": "cloneFamily", "entries": library(), "id": "a1", "newFamilyId": "FAM-A", "newIds": ["c1", "c2"] })).contains("new"));
    assert!(refused(json!({ "op": "cloneFamily", "entries": library(), "id": "a1", "newFamilyId": "FAM-C", "newIds": ["c1", "b1"] })).contains("unique"));
}

// @verifies SWR-FM-06
#[test]
fn a_variant_is_added_to_a_used_family_for_a_new_hardware_version_only() {
    let ok = call(json!({ "op": "addVariantCheck", "entries": library(), "familyId": "FAM-A", "hardwareVersion": 3 }));
    assert_eq!(ok["allowed"], true);
    assert_eq!(ok["immutableFromCreation"], true, "the screen says it cannot be edited afterwards");
    let dup = call(json!({ "op": "addVariantCheck", "entries": library(), "familyId": "FAM-A", "hardwareVersion": 2 }));
    assert_eq!(dup["allowed"], false);
    assert_eq!(dup["reason"], "variant_exists");
    let unused = call(json!({ "op": "addVariantCheck", "entries": library(), "familyId": "FAM-B", "hardwareVersion": 2 }));
    assert_eq!(unused["immutableFromCreation"], false);
}

// @verifies SWR-HV-04
#[test]
fn the_variant_for_the_helmet_is_offered_latest_first_or_a_create_offer() {
    let es = json!([e("v1", "F", false, "1.0", &[1]), e("v2", "F", false, "1.2", &[1]), e("v3", "F", false, "1.10", &[1]), e("w", "F", false, "9.0", &[2])]);
    let r = call(json!({ "op": "variantFor", "entries": es, "familyId": "F", "helmetVersion": 1 }));
    assert_eq!(r["variants"], json!(["v3", "v2", "v1"]), "1.10 is later than 1.2");
    assert_eq!(r["offerCreate"], false);
    let none = call(json!({ "op": "variantFor", "entries": es, "familyId": "F", "helmetVersion": 3 }));
    assert_eq!(none["variants"], json!([]));
    assert_eq!(none["offerCreate"], true);
}

fn pbm_def(zone: &str) -> Value {
    json!({ "name": "P", "modalities": [{ "type": "pbm_transcranial", "enabled": true,
        "params": { "zones": "named", "zoneRefs": [zone], "frequencyHz": 40 } }] })
}

// @verifies SWR-HV-04
// @verifies SWR-HV-05
#[test]
fn the_versions_a_protocol_supports_follow_its_zones_and_the_layout_table() {
    let zones = json!({ "Frontal": { "1": [1, 2, 3] }, "Later": { "2": [1] }, "Both": { "1": [1], "2": [1] } });
    let v = |zone: &str| call(json!({ "op": "supportedVersions", "def": pbm_def(zone), "zonesByVersion": zones }))["versions"].clone();
    assert_eq!(v("Frontal"), json!([1]));
    assert_eq!(v("Both"), json!([1]), "the table holds the layouts that exist: version 1");
    assert_eq!(v("Later"), json!([]), "a zone with no list for any layout in the table is supported nowhere");
    let off = call(json!({ "op": "supportedVersions", "def": pbm_def("Frontal"), "zonesByVersion": zones, "clinicianSockets": [999] }));
    assert_eq!(off["versions"], json!([]), "a socket that is not on the layout is not supported");
}

// @verifies SWR-HV-04
#[test]
fn the_layout_table_has_version_one_and_a_layout_compares_with_itself() {
    let l = call(json!({ "op": "layouts" }));
    assert_eq!(l["1"]["sockets"].as_array().unwrap().len(), 80);
    assert!(l["1"]["sockets"][0]["xMm"].is_number());
    let d = call(json!({ "op": "layoutDiff", "a": 1, "b": 1 }));
    assert_eq!(d["both"].as_array().unwrap().len(), 80);
    assert_eq!(d["onlyA"], json!([]));
    assert_eq!(d["onlyB"], json!([]));
    assert!(refused(json!({ "op": "layoutDiff", "a": 1, "b": 7 })).contains("no layout"));
}

// @verifies SWR-HV-04
#[test]
fn an_upload_for_another_hardware_version_is_refused_with_both_layouts() {
    let r = call(json!({ "op": "uploadGate", "helmetVersion": 2, "protocolVersions": [1], "contentHash": "h", "sourceHash": null }));
    assert_eq!(r["decision"], "refuse");
    assert_eq!(r["reason"], "hardware_mismatch");
    assert_eq!(r["markUsed"], false);
    assert_eq!(r["protocolVersions"], json!([1]));
    // There is no layout for version 2 yet, so no diff can be drawn for it: the entry is null rather than invented.
    assert!(r["layoutDiffs"][0]["diff"].is_null());
}

// @verifies SWR-PL-06
#[test]
fn an_unmodified_clone_is_refused_with_three_outcomes() {
    let r = call(json!({ "op": "uploadGate", "helmetVersion": 1, "protocolVersions": [1], "contentHash": "same", "sourceHash": "same" }));
    assert_eq!(r["decision"], "refuse");
    assert_eq!(r["reason"], "unmodified_clone");
    assert_eq!(r["outcomes"], json!(["upload_source", "edit_clone", "cancel"]));
    assert_eq!(r["markUsed"], false);
    let changed = call(json!({ "op": "uploadGate", "helmetVersion": 1, "protocolVersions": [1], "contentHash": "new", "sourceHash": "same" }));
    assert_eq!(changed["decision"], "allow");
    let not_a_clone = call(json!({ "op": "uploadGate", "helmetVersion": 1, "protocolVersions": [1], "contentHash": "x", "sourceHash": null }));
    assert_eq!(not_a_clone["decision"], "allow");
}

// @verifies SWR-PL-06
// @verifies SWR-HV-04
#[test]
fn the_hardware_refusal_comes_before_the_clone_refusal() {
    let r = call(json!({ "op": "uploadGate", "helmetVersion": 2, "protocolVersions": [1], "contentHash": "same", "sourceHash": "same" }));
    assert_eq!(r["reason"], "hardware_mismatch");
}

// @verifies SWR-UP-02
#[test]
fn an_upload_has_three_outcomes_and_the_used_marker_follows() {
    let up = |extra: Value| {
        let mut r = json!({ "op": "uploadResult", "sentHash": "h" });
        for (k, v) in extra.as_object().unwrap() {
            r[k] = v.clone();
        }
        call(r)
    };
    let confirmed = up(json!({ "helmetStatus": "accepted", "helmetHash": "h" }));
    assert_eq!((confirmed["outcome"].as_str(), confirmed["markUsed"].as_bool()), (Some("confirmed"), Some(true)));
    for unknown in [json!({}), json!({ "helmetStatus": "pending" }), json!({ "helmetStatus": "accepted" })] {
        let r = up(unknown);
        assert_eq!((r["outcome"].as_str(), r["markUsed"].as_bool()), (Some("unknown"), Some(true)), "no reply: the helmet may hold it");
    }
    let differ = up(json!({ "helmetStatus": "accepted", "helmetHash": "other" }));
    assert_eq!((differ["outcome"].as_str(), differ["markUsed"].as_bool()), (Some("confirmed_failed"), Some(false)));
    let refused = up(json!({ "helmetStatus": "refused", "helmetHash": "h" }));
    assert_eq!((refused["outcome"].as_str(), refused["markUsed"].as_bool()), (Some("confirmed_failed"), Some(false)));
}

// @verifies SWR-UP-02
#[test]
fn a_refusal_before_sending_or_for_hardware_clone_or_tier_marks_nothing() {
    let before = call(json!({ "op": "uploadResult", "sentHash": "h", "refusedBeforeSend": true }));
    assert_eq!((before["outcome"].as_str(), before["markUsed"].as_bool()), (Some("not_sent"), Some(false)));
    for why in ["hardware_version", "unmodified_clone", "tier"] {
        let r = call(json!({ "op": "uploadResult", "sentHash": "h", "helmetStatus": "refused", "helmetRefusal": why }));
        assert_eq!((r["outcome"].as_str(), r["markUsed"].as_bool()), (Some("not_a_failed_upload"), Some(false)), "{why}");
    }
}

// @verifies SWR-UP-02
#[test]
fn every_reason_the_helmet_can_refuse_for_has_a_cause_a_screen_can_say() {
    let cause = |reason: u64| {
        let r = call(json!({ "op": "helmetCause", "status": "refused", "reason": reason }));
        r["cause"].as_str().map(str::to_owned)
    };
    for (reason, want) in [
        (19, "tier"), (27, "hardware_version"), (20, "not_ready"), (28, "not_ready"), (6, "signature"), (7, "wrong_device"),
        (5, "format_version"), (4, "damaged"), (2, "malformed"), (8, "malformed"), (9, "malformed"), (13, "session_active"),
        (25, "boot_refused"),
        // A reason the table does not know is still a refusal the person is told about, and 0xFF is the hub's "no reason".
        (255, "other"), (99, "other"),
    ] {
        assert_eq!(cause(reason).as_deref(), Some(want), "reason {reason}");
    }
    // An accepted upload and no answer are not refusals.
    assert_eq!(call(json!({ "op": "helmetCause", "status": "accepted", "reason": 19 }))["cause"], Value::Null);
    assert_eq!(call(json!({ "op": "helmetCause" }))["cause"], Value::Null);
}

// @verifies SWR-UP-02
// @verifies SWR-HV-08
// @verifies REQ-UPG-01
#[test]
fn only_a_tier_or_hardware_version_refusal_has_a_refusal_kind_and_only_unverified_is_not_ready() {
    let ask = |status: &str, reason: u64| call(json!({ "op": "helmetCause", "status": status, "reason": reason }));
    for (reason, kind, not_ready) in [
        (19, "tier", false), (20, "tier", true), (27, "hardware_version", false), (28, "hardware_version", true),
    ] {
        let r = ask("refused", reason);
        assert_eq!(r["refusalKind"], kind, "reason {reason}");
        assert_eq!(r["notReady"], not_ready, "reason {reason}");
    }
    for reason in [6, 13, 25, 99] {
        let r = ask("refused", reason);
        assert_eq!(r["refusalKind"], Value::Null, "a bad signature is a failure, not a tier (reason {reason})");
        assert_eq!(r["notReady"], false);
    }
    // Only a refusal has a kind: an accepted report that happens to carry a tier reason has none.
    let accepted = ask("accepted", 19);
    assert_eq!((accepted["refusalKind"].clone(), accepted["notReady"].clone()), (Value::Null, json!(false)));
}

// @verifies SWR-UP-02
#[test]
fn a_final_answer_for_other_bytes_is_unmatched_whatever_it_said() {
    let ask = |extra: Value| {
        let mut r = json!({ "op": "helmetCause", "sentHash": "sent" });
        for (k, v) in extra.as_object().unwrap() {
            r[k] = v.clone();
        }
        call(r)["cause"].clone()
    };
    assert_eq!(ask(json!({ "status": "accepted", "helmetHash": "other" })), "unmatched", "the helmet holds other bytes");
    assert_eq!(ask(json!({ "status": "refused", "reason": 6, "helmetHash": "other" })), "unmatched", "an earlier upload's refusal");
    // For this upload the reason decides, and an accepted one has nothing to say.
    assert_eq!(ask(json!({ "status": "refused", "reason": 19, "helmetHash": "sent" })), "tier");
    assert_eq!(ask(json!({ "status": "accepted", "helmetHash": "sent" })), Value::Null);
    // No answer, and a verdict still pending, are the unknown outcome.
    assert_eq!(ask(json!({})), Value::Null);
    assert_eq!(ask(json!({ "status": "pending", "helmetHash": "other" })), Value::Null);
    assert_eq!(ask(json!({ "status": "refused", "reason": 19 })), Value::Null, "a report with no hash is no answer");
}

// @verifies SWR-WO-03
#[test]
fn an_import_merges_the_used_flag_by_or_and_never_clears_it() {
    let r = call(json!({ "op": "mergeImport", "usedHashes": ["h-used"], "incoming": [
        { "id": "flagged", "hash": "h1", "used": true },
        { "id": "unflagged-but-used-here", "hash": "h-used", "used": false },
        { "id": "fresh", "hash": "h2", "used": false },
    ] }));
    let used: Vec<bool> = r["entries"].as_array().unwrap().iter().map(|x| x["used"].as_bool().unwrap()).collect();
    assert_eq!(used, vec![true, true, false]);
}

// @verifies SWR-WO-02
#[test]
fn the_stored_used_flag_is_the_family_id_and_the_bit_and_nothing_else() {
    let r = call(json!({ "op": "usedRecord", "familyId": "FAM-A" }));
    assert_eq!(r["record"], json!({ "familyId": "FAM-A" }));
    assert_eq!(call(json!({ "op": "usedRecord", "check": { "familyId": "FAM-A" } }))["valid"], true);
    for bad in [json!({ "familyId": "F", "device": "x" }), json!({ "familyId": "F", "count": 3 }), json!({ "familyId": "F", "at": 1 }), json!({}), json!("F")] {
        assert_eq!(call(json!({ "op": "usedRecord", "check": bad }))["valid"], false);
    }
}

fn variant_def(freq: u64, zone: &str) -> Value {
    json!({ "name": "V", "timingMode": { "type": "duration", "seconds": 600 }, "modalities": [
        { "type": "pbm_transcranial", "enabled": true, "params": { "zones": "named", "zoneRefs": [zone], "frequencyHz": freq, "sockets": [1, 2] } },
        { "type": "audio_entrainment", "enabled": true, "params": { "frequencyHz": 40 } } ] })
}

// @verifies SWR-FM-04
#[test]
fn an_intent_edit_is_offered_to_every_sibling_per_field_and_sockets_are_not() {
    let sibs = json!([
        { "id": "s1", "scope": "owner", "def": variant_def(40, "Occipital") },
        { "id": "s2", "scope": "owner", "def": variant_def(10, "Frontal") },
        { "id": "s3", "scope": "owner", "def": variant_def(10, "Frontal") },
    ]);
    let r = call(json!({ "op": "propagationOffer", "scope": "owner", "old": variant_def(40, "Frontal"), "new": variant_def(10, "Parietal"), "siblings": sibs }));
    let offers = r["offers"].as_array().unwrap();
    // s1 differs from the new intent (40 vs 10) and gets the frequency; s2 and s3 already have 10, so nothing is offered.
    assert_eq!(offers.len(), 1);
    assert_eq!(offers[0]["id"], "s1");
    let ch = offers[0]["changes"].as_array().unwrap();
    assert_eq!(ch.len(), 1, "the zone and socket change stays with the variant");
    assert_eq!(ch[0]["from"], 40);
    assert_eq!(ch[0]["to"], 10);
    assert_eq!(ch[0]["path"], json!(["modalities", 0, "params", "frequencyHz"]));
}

// @verifies SWR-FM-04
#[test]
fn a_socket_edit_offers_nothing() {
    let sibs = json!([{ "id": "s1", "scope": "owner", "def": variant_def(40, "Frontal") }]);
    let r = call(json!({ "op": "propagationOffer", "scope": "owner", "old": variant_def(40, "Frontal"), "new": variant_def(40, "Parietal"), "siblings": sibs }));
    assert_eq!(r["offers"], json!([]));
}

// @verifies SWR-FM-04
#[test]
fn a_family_does_not_propagate_across_the_owner_private_line() {
    let sibs = json!([{ "id": "mine", "scope": "private", "def": variant_def(40, "Frontal") }]);
    let r = call(json!({ "op": "propagationOffer", "scope": "owner", "old": variant_def(40, "Frontal"), "new": variant_def(10, "Frontal"), "siblings": sibs }));
    assert_eq!(r["offers"], json!([]));
}

// @verifies SWR-FM-04
#[test]
fn an_accepted_change_applies_and_a_refused_one_leaves_the_sibling_alone() {
    let sib = variant_def(40, "Occipital");
    let r = call(json!({ "op": "applyChanges", "def": sib, "changes": [{ "path": ["modalities", 0, "params", "frequencyHz"], "to": 10 }] }));
    assert_eq!(r["def"]["modalities"][0]["params"]["frequencyHz"], 10);
    assert_eq!(r["def"]["modalities"][0]["params"]["zoneRefs"], json!(["Occipital"]), "everything else is as it was");
    // Accepting nothing changes nothing.
    let none = call(json!({ "op": "applyChanges", "def": sib, "changes": [] }));
    assert_eq!(none["def"], sib);
    assert!(refused(json!({ "op": "applyChanges", "def": sib, "changes": [{ "path": ["modalities", 0, "params", "zoneRefs"], "to": ["X"] }] })).contains("never propagated"));
    assert!(refused(json!({ "op": "applyChanges", "def": sib, "changes": [{ "path": ["modalities", 7, "params", "frequencyHz"], "to": 1 }] })).contains("does not have"));
}

// @verifies SWR-FM-07
#[test]
fn with_one_layout_there_is_nothing_to_map() {
    let r = call(json!({ "op": "mapVariant", "sourceVersion": 1, "targetVersion": 1 }));
    assert_eq!(r["status"], "nothing_to_map");
    assert_eq!(r["changes"], json!([]));
    // Version 2 has no layout yet, so a retarget to it has nothing to map onto either; the mapping is built with version 2.
    let to_two = call(json!({ "op": "mapVariant", "sourceVersion": 1, "targetVersion": 2 }));
    assert_eq!(to_two["status"], "nothing_to_map");
}
