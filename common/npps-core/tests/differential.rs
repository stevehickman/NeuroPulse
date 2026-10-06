//! The core against the web parser. `app/NeurOneShared/TestData/npps-parse-golden.json` is the
//! web parser's own output for the whole shipped library, the shared fixtures and a corpus of
//! refusals (`bun scripts/gen-npps-parse-golden.ts`). Every entry and every message must match.

use neurone_npps_core::{parse_npps, Entry};
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

/// What the web parser's entry list reduces to: protocols and the count of composites.
fn reduce(entries: Vec<Entry>) -> Vec<Value> {
    entries
        .into_iter()
        .filter_map(|e| match e {
            Entry::Single(p) => Some(serde_json::json!({ "kind": "single", "protocol": p })),
            Entry::Skipped("composite") => Some(serde_json::json!({ "kind": "skipped", "what": "composite" })),
            Entry::Skipped(_) => None,
        })
        .collect()
}

fn check(name: &str, src: &str, expected: &Value, failures: &mut Vec<String>) {
    match parse_npps(src) {
        Ok(entries) => {
            if let Some(err) = expected.get("error") {
                failures.push(format!("{name}: core accepted what the web parser refuses: {err}"));
                return;
            }
            let got = Value::Array(reduce(entries));
            if got != expected["entries"] {
                failures.push(format!(
                    "{name}: entries differ\n  core: {}\n  web:  {}",
                    serde_json::to_string(&got).unwrap(),
                    serde_json::to_string(&expected["entries"]).unwrap()
                ));
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
