//! The core's compiler against the web compiler. `hub-descriptor-golden.json` holds the bytes
//! the web compiler writes for 22 definitions; `hub-descriptor-cases.json` holds those
//! definitions, the zone namespace they compile against, and the message the web compiler
//! gives for each refusal (`bun scripts/gen-hub-descriptor-golden.ts`). Every byte and every
//! message must match.

use neurone_npps_core::compiler::{compile_protocol, CompileOptions};
use serde_json::Value;
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

fn data(name: &str) -> Value {
    let p = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../app/NeurOneShared/TestData").join(name);
    serde_json::from_str(&fs::read_to_string(p).expect(name)).expect(name)
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

struct Ctx {
    zones: HashMap<String, Vec<u32>>,
    clinician: Vec<u32>,
    now: u32,
    uuid: [u8; 16],
}

fn ctx(cases: &Value) -> Ctx {
    let zones = cases["zones"]
        .as_object()
        .unwrap()
        .iter()
        .map(|(k, v)| (k.clone(), v.as_array().unwrap().iter().map(|n| n.as_u64().unwrap() as u32).collect()))
        .collect();
    let clinician = cases["clinicianSockets"].as_array().unwrap().iter().map(|n| n.as_u64().unwrap() as u32).collect();
    Ctx {
        zones,
        clinician,
        now: cases["compiledAt"].as_u64().unwrap() as u32,
        uuid: [cases["sessionUuidByte"].as_u64().unwrap() as u8; 16],
    }
}

fn opts<'a>(c: &'a Ctx, with_clinician: bool) -> CompileOptions<'a> {
    CompileOptions {
        device_serial: None,
        zones: Some(&c.zones),
        clinician_sockets: if with_clinician { Some(&c.clinician) } else { None },
        wavelength_rules: None,
        now_unix: c.now,
        session_uuid: c.uuid,
    }
}

#[test]
fn every_definition_compiles_to_the_web_compilers_bytes() {
    let cases = data("hub-descriptor-cases.json");
    let c = ctx(&cases);
    let mut failures = Vec::new();
    assert!(cases["cases"].as_object().unwrap().len() >= 22, "the golden definitions are present");
    for (name, case) in cases["cases"].as_object().unwrap() {
        match compile_protocol(&case["def"], &opts(&c, true)) {
            Ok(out) => {
                let got = hex(&out.blob);
                if got != case["hex"].as_str().unwrap() {
                    failures.push(format!("{name}: bytes differ\n  core: {got}\n  web:  {}", case["hex"]));
                }
            }
            Err(e) => failures.push(format!("{name}: refused: {e}")),
        }
    }
    assert!(failures.is_empty(), "{} divergence(s):\n{}", failures.len(), failures.join("\n"));
}

#[test]
fn the_golden_file_the_other_runtimes_read_agrees_with_the_cases_file() {
    let golden = data("hub-descriptor-golden.json");
    let cases = data("hub-descriptor-cases.json");
    for (name, case) in cases["cases"].as_object().unwrap() {
        assert_eq!(golden[name], case["hex"], "{name}");
    }
}

#[test]
fn every_refusal_reads_as_the_web_compilers_does() {
    let cases = data("hub-descriptor-cases.json");
    let c = ctx(&cases);
    let mut failures = Vec::new();
    let refused = cases["errors"].as_object().unwrap().values().filter(|c| !c["message"].is_null()).count();
    assert!(refused >= 15, "the refusal corpus is present");
    for (name, case) in cases["errors"].as_object().unwrap() {
        let with = !case["noClinicianSockets"].as_bool().unwrap();
        let got = compile_protocol(&case["def"], &opts(&c, with)).err();
        let want = case["message"].as_str();
        if got.as_deref() != want {
            failures.push(format!("{name}:\n  core: {got:?}\n  web:  {want:?}"));
        }
    }
    assert!(failures.is_empty(), "{} divergence(s):\n{}", failures.len(), failures.join("\n"));
}
