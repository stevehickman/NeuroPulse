//! The core against the web parser. `app/NeurOneShared/TestData/npps-parse-golden.json` is the
//! web parser's own output for the whole shipped library, the shared fixtures and a corpus of
//! refusals (`bun scripts/gen-npps-parse-golden.ts`). Every entry and every message must match.

use neurone_npps_core::{parse_file, Entry, ParsedFile};
use serde_json::Value;
use std::fs;
use std::path::PathBuf;

fn root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn golden() -> Value {
    let p = root().join("app/NeurOneShared/TestData/npps-parse-golden.json");
    serde_json::from_str(&fs::read_to_string(p).expect("golden file")).expect("golden JSON")
}

/// What the web parser's `parseNPPSFile` and `parseNPPSLimits` reduce to, from the core's file result.
/// The web parser keeps only the first `limits` block; the core reports every one.
fn reduce(f: ParsedFile) -> Value {
    let entries: Vec<Value> = f
        .entries
        .into_iter()
        .map(|e| match e {
            Entry::Single(p) => serde_json::json!({ "kind": "single", "protocol": p }),
            Entry::Composite(c) => serde_json::json!({ "kind": "composite", "composite": c }),
        })
        .collect();
    serde_json::json!({
        "entries": entries,
        "zones": f.zones,
        "conditions": f.conditions,
        "wavelengthRules": f.wavelength_rules,
        "limits": f.limits.into_iter().next().unwrap_or(Value::Null),
    })
}

fn check(name: &str, src: &str, expected: &Value, failures: &mut Vec<String>) {
    match parse_file(src) {
        Ok(file) => {
            if let Some(err) = expected.get("error") {
                failures.push(format!("{name}: core accepted what the web parser refuses: {err}"));
                return;
            }
            let got = reduce(file);
            for key in ["entries", "zones", "conditions", "wavelengthRules", "limits"] {
                if got[key] != expected[key] {
                    failures.push(format!(
                        "{name}: {key} differ\n  core: {}\n  web:  {}",
                        serde_json::to_string(&got[key]).unwrap(),
                        serde_json::to_string(&expected[key]).unwrap()
                    ));
                }
            }
        }
        Err(e) => match expected.get("error").and_then(Value::as_str) {
            Some(msg) if msg == e.to_string() => {}
            Some(msg) => failures.push(format!("{name}: message differs\n  core: {e}\n  web:  {msg}")),
            None => failures.push(format!("{name}: core refused what the web parser accepts: {e}")),
        },
    }
}

#[test]
fn every_library_file_and_fixture_parses_as_the_web_parser_does() {
    let g = golden();
    let mut failures = Vec::new();
    let files = g["files"].as_object().expect("files");
    assert!(files.len() > 60, "the golden covers the shipped library");
    for (rel, expected) in files {
        let src = fs::read_to_string(root().join(rel)).unwrap_or_else(|_| panic!("read {rel}"));
        check(rel, &src, expected, &mut failures);
    }
    assert!(failures.is_empty(), "{} divergence(s):\n{}", failures.len(), failures.join("\n"));
}

#[test]
fn every_corpus_case_parses_or_is_refused_as_the_web_parser_does() {
    let g = golden();
    let mut failures = Vec::new();
    for (name, case) in g["cases"].as_object().expect("cases") {
        check(name, case["source"].as_str().unwrap(), case, &mut failures);
    }
    assert!(failures.is_empty(), "{} divergence(s):\n{}", failures.len(), failures.join("\n"));
}

/// `buildNamespace` and `validateNamespaceReferences`, on the cases the generator pins and on the
/// whole shipped library loaded as one namespace.
#[test]
fn namespaces_fold_and_validate_as_the_web_functions_do() {
    use neurone_npps_core::api::{namespace_json, parse_json};
    let g = golden();
    let cases = g["namespaces"].as_object().expect("namespaces");
    assert!(cases.contains_key("library") && cases.len() > 5, "the golden pins the namespace");
    let mut failures = Vec::new();
    for (name, case) in cases {
        let sources: Vec<String> = match case.get("paths") {
            Some(paths) => paths
                .as_array()
                .unwrap()
                .iter()
                .map(|p| fs::read_to_string(root().join(p.as_str().unwrap())).expect("library file"))
                .collect(),
            None => case["sources"].as_array().unwrap().iter().map(|s| s.as_str().unwrap().to_string()).collect(),
        };
        let files: Vec<Value> = sources
            .iter()
            .map(|s| serde_json::from_str(&parse_json(s).expect("namespace sources parse")).unwrap())
            .collect();
        let got: Value =
            serde_json::from_str(&namespace_json(&serde_json::json!({ "files": files }).to_string()).unwrap()).unwrap();
        for key in ["entries", "zones", "conditions", "errors", "referenceErrors"] {
            if got[key] != case[key] {
                failures.push(format!(
                    "{name}: {key} differ\n  core: {}\n  web:  {}",
                    serde_json::to_string(&got[key]).unwrap(),
                    serde_json::to_string(&case[key]).unwrap()
                ));
            }
        }
    }
    assert!(failures.is_empty(), "{} divergence(s):\n{}", failures.len(), failures.join("\n"));
}
