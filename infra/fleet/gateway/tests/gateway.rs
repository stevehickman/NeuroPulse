//! The gateway against a fake ingest, with a client played by the shared core (`REQ-SHDR-07`).


use neurone_fleet_gateway::{serve, Config};
use neurone_npps_core::migration::migration_json;
use serde_json::{json, Value};
use std::io::{Read, Write};
use std::net::{SocketAddr, TcpListener, TcpStream};
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex};
use std::thread;

fn ask(v: Value) -> Result<Value, String> {
    migration_json(&v.to_string()).map(|s| serde_json::from_str(&s).unwrap())
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

/// A fake ingest: records each request it is sent whole, answers `status`.
fn fake_ingest(status: u16) -> (SocketAddr, Arc<Mutex<Vec<String>>>) {
    let l = TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = l.local_addr().unwrap();
    let seen = Arc::new(Mutex::new(Vec::new()));
    let s2 = seen.clone();
    thread::spawn(move || {
        for mut c in l.incoming().flatten() {
            // Read until the whole request (head and the body its Content-Length names) has arrived, however it was split.
            let mut got = Vec::new();
            let mut chunk = [0u8; 4096];
            loop {
                let n = c.read(&mut chunk).unwrap_or(0);
                got.extend_from_slice(&chunk[..n]);
                let text = String::from_utf8_lossy(&got).to_string();
                let done = text.split_once("\r\n\r\n").is_some_and(|(head, body)| {
                    let want = head.to_ascii_lowercase().split("content-length: ").nth(1)
                        .and_then(|r| r.split("\r\n").next()).and_then(|v| v.trim().parse::<usize>().ok()).unwrap_or(0);
                    body.len() >= want
                });
                if n == 0 || done {
                    break;
                }
            }
            s2.lock().unwrap().push(String::from_utf8_lossy(&got).to_string());
            let _ = c.write_all(format!("HTTP/1.1 {status} X\r\nContent-Length: 0\r\nConnection: close\r\n\r\n").as_bytes());
        }
    });
    (addr, seen)
}

struct Rig {
    gateway: SocketAddr,
    counters: Arc<neurone_fleet_gateway::Counters>,
    key_config: String,
    ingest_seen: Arc<Mutex<Vec<String>>>,
}

fn rig(ingest_status: u16) -> Rig {
    let key = ask(json!({ "op": "ohttpGenerateKey", "seed": hex(&[6u8; 32]), "keyId": 1 })).unwrap();
    let key_config = key["keyConfig"].as_str().unwrap().to_string();
    let (ingest, ingest_seen) = fake_ingest(ingest_status);
    let (gateway, counters) = serve(
        "127.0.0.1:0",
        Config { key_config_hex: key_config.clone(), private_key_hex: key["privateKey"].as_str().unwrap().into(), ingest: ingest.to_string() },
    )
    .unwrap();
    Rig { gateway, counters, key_config, ingest_seen }
}

fn http(addr: SocketAddr, method: &str, path: &str, content_type: Option<&str>, body: &[u8]) -> (u16, String, Vec<u8>) {
    let mut s = TcpStream::connect(addr).unwrap();
    let mut head = format!("{method} {path} HTTP/1.1\r\nHost: g\r\nContent-Length: {}\r\n", body.len());
    if let Some(ct) = content_type {
        head.push_str(&format!("Content-Type: {ct}\r\n"));
    }
    s.write_all(format!("{head}\r\n").as_bytes()).unwrap();
    s.write_all(body).unwrap();
    let mut out = Vec::new();
    s.read_to_end(&mut out).unwrap();
    let split = out.windows(4).position(|w| w == b"\r\n\r\n").unwrap();
    let head = String::from_utf8_lossy(&out[..split]).to_string();
    let status = head.split(' ').nth(1).unwrap().parse().unwrap();
    (status, head, out[split + 4..].to_vec())
}

fn seal(r: &Rig, method: &str, path: &str, token: &str, extra: Value) -> Value {
    let mut headers = vec![json!(["X-NP-Device-Token", token]), json!(["Content-Type", "application/octet-stream"])];
    headers.extend(extra.as_array().unwrap().iter().cloned());
    ask(json!({
        "op": "ohttpSeal", "keyConfig": r.key_config, "method": method, "scheme": "https", "authority": "fleet.example",
        "path": path, "headers": headers, "body": hex(b"the record"), "entropy": hex(&[2u8; 32]),
    }))
    .unwrap()
}

fn inner_status(sealed: &Value, response: &[u8]) -> u16 {
    ask(json!({ "op": "ohttpOpen", "enc": sealed["enc"], "secret": sealed["secret"], "response": hex(response) })).unwrap()["status"]
        .as_u64()
        .unwrap() as u16
}

// @verifies REQ-SHDR-07#a
#[test]
fn a_sealed_record_reaches_the_ingest_with_the_token_and_nothing_else_the_sender_wrote() {
    let r = rig(204);
    let sealed = seal(&r, "POST", "/v1/shdr", "tok123", json!([["X-Forwarded-For", "203.0.113.9"], ["Cookie", "a=b"]]));
    let (status, _, body) = http(r.gateway, "POST", "/gateway", Some("message/ohttp-req"), &unhex(sealed["request"].as_str().unwrap()));
    assert_eq!(status, 200);
    assert_eq!(inner_status(&sealed, &body), 204);
    let seen = r.ingest_seen.lock().unwrap();
    let got = seen.last().unwrap().to_ascii_lowercase();
    assert!(got.starts_with("post /v1/shdr "));
    assert!(got.contains("x-np-device-token: tok123"));
    assert!(got.contains("the record"));
    assert!(!got.contains("203.0.113.9") && !got.contains("cookie") && !got.contains("x-forwarded-for"), "nothing but the allowed headers is passed on");
    assert_eq!(r.counters.forwarded.load(Ordering::Relaxed), 1);
}

// @verifies REQ-SHDR-07#c
#[test]
fn the_ingests_refusal_and_its_absence_are_sealed_answers_not_successes() {
    let r = rig(500);
    let sealed = seal(&r, "POST", "/v1/shdr", "t", json!([]));
    let (status, _, body) = http(r.gateway, "POST", "/gateway", Some("message/ohttp-req"), &unhex(sealed["request"].as_str().unwrap()));
    assert_eq!((status, inner_status(&sealed, &body)), (200, 500));

    // An ingest that is not there: a sealed 502, never a hang or a plain error that the relay could read.
    let key = ask(json!({ "op": "ohttpGenerateKey", "seed": hex(&[6u8; 32]), "keyId": 1 })).unwrap();
    let dead = TcpListener::bind("127.0.0.1:0").unwrap().local_addr().unwrap();
    let (gw, _) = serve(
        "127.0.0.1:0",
        Config { key_config_hex: key["keyConfig"].as_str().unwrap().into(), private_key_hex: key["privateKey"].as_str().unwrap().into(), ingest: dead.to_string() },
    )
    .unwrap();
    let (status, _, body) = http(gw, "POST", "/gateway", Some("message/ohttp-req"), &unhex(sealed["request"].as_str().unwrap()));
    assert_eq!((status, inner_status(&sealed, &body)), (200, 502));
}

// @verifies REQ-SHDR-07#e
#[test]
fn the_gateway_is_not_an_open_proxy() {
    let r = rig(204);
    for (method, path) in [("GET", "/v1/shdr"), ("POST", "/admin"), ("POST", "/v1/shdr/../x")] {
        let sealed = seal(&r, method, path, "t", json!([]));
        let (status, _, body) = http(r.gateway, "POST", "/gateway", Some("message/ohttp-req"), &unhex(sealed["request"].as_str().unwrap()));
        assert_eq!((status, inner_status(&sealed, &body)), (200, 403), "{method} {path}");
    }
    assert!(r.ingest_seen.lock().unwrap().is_empty(), "nothing was passed on");
}

// @verifies REQ-SHDR-07#e
#[test]
fn a_message_that_does_not_open_or_is_not_one_is_refused_without_reaching_the_ingest() {
    let r = rig(204);
    let sealed = seal(&r, "POST", "/v1/shdr", "t", json!([]));
    let mut bytes = unhex(sealed["request"].as_str().unwrap());
    *bytes.last_mut().unwrap() ^= 1;
    assert_eq!(http(r.gateway, "POST", "/gateway", Some("message/ohttp-req"), &bytes).0, 400);
    assert_eq!(http(r.gateway, "POST", "/gateway", Some("application/json"), &bytes).0, 415);
    assert_eq!(http(r.gateway, "POST", "/gateway", Some("message/ohttp-req"), b"short").0, 400);
    assert_eq!(http(r.gateway, "GET", "/gateway", None, b"").0, 405);
    assert_eq!(http(r.gateway, "GET", "/other", None, b"").0, 404);
    assert!(r.ingest_seen.lock().unwrap().is_empty());
}

// @verifies REQ-SHDR-07#f
#[test]
fn the_key_configuration_is_published_and_the_gateway_answers_health() {
    let r = rig(204);
    let (status, head, body) = http(r.gateway, "GET", "/ohttp-keys", None, b"");
    assert_eq!(status, 200);
    assert!(head.to_ascii_lowercase().contains("content-type: application/ohttp-keys"));
    assert_eq!(&body[..2], &((r.key_config.len() / 2) as u16).to_be_bytes());
    assert_eq!(hex(&body[2..]), r.key_config);
    assert_eq!(http(r.gateway, "GET", "/healthz", None, b"").0, 200);
}

// @verifies REQ-SHDR-07#d
#[test]
fn the_gateway_refuses_to_start_on_a_key_that_is_not_hex() {
    let bad = serve("127.0.0.1:0", Config { key_config_hex: "zz".into(), private_key_hex: "00".into(), ingest: "x:1".into() });
    assert!(bad.is_err());
}
