//! The C ABI through its own functions, against the web reference: the shipped library, the
//! corpus and the 22 golden descriptors, as bytes in and bytes out. What this adds over the
//! core's tests is the marshalling: buffer ownership, UTF-8 (messages contain `—` and `²`),
//! return codes and the failure paths.

use neurone_npps_ffi::{npps_alloc, npps_compile_json, npps_free, npps_namespace_json, npps_parse_json};
use serde_json::Value;
use std::fs;
use std::path::PathBuf;
use std::ptr;

fn root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn data(name: &str) -> Value {
    serde_json::from_str(&fs::read_to_string(root().join("app/NeurOneShared/TestData").join(name)).unwrap()).unwrap()
}

type Call = unsafe extern "C" fn(*const u8, usize, *mut *mut u8, *mut usize) -> i32;

fn run(f: Call, input: &[u8]) -> (i32, Vec<u8>) {
    let (mut out, mut len) = (ptr::null_mut(), 0usize);
    let code = unsafe { f(input.as_ptr(), input.len(), &mut out, &mut len) };
    let bytes = if out.is_null() { vec![] } else { unsafe { std::slice::from_raw_parts(out, len).to_vec() } };
    unsafe { npps_free(out, len) };
    (code, bytes)
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

#[test]
fn parse_agrees_with_the_web_parser_over_the_library_and_the_corpus() {
    let g = data("npps-parse-golden.json");
    let mut bad = Vec::new();
    let mut check = |name: &str, src: &str, want: &Value| {
        let (code, out) = run(npps_parse_json, src.as_bytes());
        let text = String::from_utf8(out).expect("UTF-8 out");
        match want.get("error").and_then(Value::as_str) {
            Some(msg) => {
                if code != 1 || text != msg {
                    bad.push(format!("{name}: code {code}, {text:?} vs {msg:?}"));
                }
            }
            None => {
                let got: Value = serde_json::from_str(&text).unwrap_or(Value::Null);
                // The web parser keeps only the first `limits` block; the core reports every one.
                let first_limits = got["limits"].as_array().and_then(|l| l.first().cloned()).unwrap_or(Value::Null);
                let same = ["entries", "zones", "conditions", "wavelengthRules"].iter().all(|k| got[k] == want[k])
                    && first_limits == want["limits"];
                if code != 0 || !same {
                    bad.push(format!("{name}: code {code}, the parse result differs"));
                }
            }
        }
    };
    for (rel, want) in g["files"].as_object().unwrap() {
        check(rel, &fs::read_to_string(root().join(rel)).unwrap(), want);
    }
    for (name, case) in g["cases"].as_object().unwrap() {
        check(name, case["source"].as_str().unwrap(), case);
    }
    assert!(bad.is_empty(), "{} divergence(s):\n{}", bad.len(), bad.join("\n"));
}

fn request(def: &Value, c: &Value, with_clinician: bool) -> Vec<u8> {
    let uuid = format!("{:02x}", c["sessionUuidByte"].as_u64().unwrap()).repeat(16);
    serde_json::json!({
        "def": def, "zones": c["zones"],
        "clinicianSockets": if with_clinician { c["clinicianSockets"].clone() } else { Value::Null },
        "deviceSerialHex": null, "nowUnix": c["compiledAt"], "sessionUuidHex": uuid, "wavelengthRules": null,
    })
    .to_string()
    .into_bytes()
}

#[test]
fn compile_agrees_with_the_web_compiler_in_bytes_and_messages() {
    let c = data("hub-descriptor-cases.json");
    for (name, case) in c["cases"].as_object().unwrap() {
        let (code, out) = run(npps_compile_json, &request(&case["def"], &c, true));
        assert_eq!(code, 0, "{name}: {}", String::from_utf8_lossy(&out));
        assert_eq!(hex(&out), case["hex"].as_str().unwrap(), "{name}");
    }
    let mut refused = 0;
    for (name, case) in c["errors"].as_object().unwrap() {
        let Some(want) = case["message"].as_str() else { continue };
        let with = !case["noClinicianSockets"].as_bool().unwrap();
        let (code, out) = run(npps_compile_json, &request(&case["def"], &c, with));
        assert_eq!(code, 1, "{name}");
        assert_eq!(String::from_utf8(out).unwrap(), want, "{name}");
        refused += 1;
    }
    assert!(refused >= 15);
}

#[test]
fn bad_arguments_are_refused_not_crashed() {
    let (mut out, mut len) = (ptr::null_mut(), 0usize);
    unsafe {
        assert_eq!(npps_parse_json(ptr::null(), 0, &mut out, &mut len), 3);
        assert_eq!(npps_parse_json(b"x".as_ptr(), 1, ptr::null_mut(), &mut len), 3);
        // Not UTF-8.
        assert_eq!(npps_parse_json([0xFFu8, 0xFE].as_ptr(), 2, &mut out, &mut len), 3);
        // A request that is not JSON is a refusal with a message, not a crash.
        assert_eq!(npps_compile_json(b"{".as_ptr(), 1, &mut out, &mut len), 1);
        let msg = std::slice::from_raw_parts(out, len).to_vec();
        npps_free(out, len);
        assert!(String::from_utf8(msg).unwrap().contains("not JSON"));
        // Freeing nothing is fine.
        npps_free(ptr::null_mut(), 0);
    }
}

#[test]
fn alloc_hands_out_writable_zeroed_bytes_that_free_takes_back() {
    unsafe {
        let p = npps_alloc(8);
        assert!(!p.is_null());
        assert_eq!(std::slice::from_raw_parts(p, 8), &[0u8; 8]);
        *p = 0xAB;
        npps_free(p, 8);
        // A zero length is valid and still freeable.
        let z = npps_alloc(0);
        assert!(!z.is_null());
        npps_free(z, 0);
    }
}

#[test]
fn namespace_folds_files_through_the_abi_and_reports_a_duplicate() {
    let file = |src: &str| -> Value {
        let (code, out) = run(npps_parse_json, src.as_bytes());
        assert_eq!(code, 0);
        serde_json::from_slice(&out).unwrap()
    };
    let files = vec![
        file("zone \"A\" {\n  sockets: [1]\n}\n"),
        file("zone \"A\" {\n  sockets: [2]\n}\nzone \"B\" {\n  sockets: [3]\n}\n"),
    ];
    let (code, out) = run(npps_namespace_json, serde_json::json!({ "files": files }).to_string().as_bytes());
    assert_eq!(code, 0);
    let ns: Value = serde_json::from_slice(&out).unwrap();
    assert_eq!(ns["zones"].as_array().unwrap().len(), 1, "A collides and is left undefined; B stays");
    assert!(ns["errors"][0].as_str().unwrap().starts_with("Duplicate zone name 'A'"));
    // A request that is not JSON is a refusal with a message, not a crash.
    let (code, out) = run(npps_namespace_json, b"not json");
    assert_eq!(code, 1);
    assert!(String::from_utf8(out).unwrap().contains("not JSON"));
}
