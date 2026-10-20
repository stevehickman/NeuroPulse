//! The shared core's Oblivious HTTP against an independent implementation (Mozilla's `ohttp` crate, `rust-hpke` backend), in both
//! directions, through the gateway and on the wire (`REQ-SHDR-07`; `NP-SOUP-RUST-001` §6.2).
//! A matching mistake made on both sides of the core's own tests would pass them; it cannot pass these.

use neurone_npps_core::migration::migration_json;
use ohttp::hpke::{Aead, Kdf, Kem};
use ohttp::{ClientRequest, KeyConfig, Server, SymmetricSuite};
use serde_json::{json, Value};

fn ask(v: Value) -> Result<Value, String> {
    migration_json(&v.to_string()).map(|s| serde_json::from_str(&s).unwrap())
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn suite() -> Vec<SymmetricSuite> {
    vec![SymmetricSuite::new(Kdf::HkdfSha256, Aead::Aes128Gcm)]
}

/// The same key, derived by both implementations from one seed (DeriveKeyPair, RFC 9180 §7.1.3).
fn keys(seed: [u8; 32], id: u8) -> (KeyConfig, String, String) {
    let theirs = KeyConfig::derive(id, Kem::X25519Sha256, suite(), &seed).unwrap();
    let ours = ask(json!({ "op": "ohttpGenerateKey", "seed": hex(&seed), "keyId": id })).unwrap();
    (theirs, ours["keyConfig"].as_str().unwrap().into(), ours["privateKey"].as_str().unwrap().into())
}

// @verifies REQ-SHDR-07#h
#[test]
fn both_implementations_derive_the_same_key_configuration_from_one_seed() {
    let (theirs, ours, _) = keys([11u8; 32], 7);
    assert_eq!(hex(&theirs.encode().unwrap()), ours);
}

/// Binary HTTP (RFC 9292) known-length request: `POST https://fleet.example/v1/shdr`, one header, a body, no trailers.
/// Every length here is under 64, so each is one byte.
fn bhttp_request(token: &str, body: &[u8]) -> Vec<u8> {
    let field = |n: &str, v: &str| [&[n.len() as u8][..], n.as_bytes(), &[v.len() as u8], v.as_bytes()].concat();
    let fields = field("x-np-device-token", token);
    let mut m = vec![0u8];
    for part in ["POST", "https", "fleet.example", "/v1/shdr"] {
        m.push(part.len() as u8);
        m.extend(part.as_bytes());
    }
    m.push(fields.len() as u8);
    m.extend(fields);
    m.push(body.len() as u8);
    m.extend(body);
    m.push(0); // trailers
    m
}

// @verifies REQ-SHDR-07#h
#[test]
fn a_request_the_other_implementation_seals_opens_in_the_core_and_the_answer_comes_back() {
    let (theirs, ours, sk) = keys([12u8; 32], 1);
    let (enc_request, client_response) =
        ClientRequest::from_encoded_config(&theirs.encode().unwrap()).unwrap().encapsulate(&bhttp_request("tok", b"record")).unwrap();

    let g = ask(json!({ "op": "ohttpGatewayOpen", "keyConfig": ours, "privateKey": sk, "request": hex(&enc_request) })).unwrap();
    assert_eq!((g["method"].as_str(), g["authority"].as_str(), g["path"].as_str()), (Some("POST"), Some("fleet.example"), Some("/v1/shdr")));
    assert_eq!(g["headers"], json!([["x-np-device-token", "tok"]]));
    assert_eq!(g["body"], hex(b"record"));

    let reply = ask(json!({
        "op": "ohttpGatewaySeal", "enc": g["enc"], "secret": g["secret"], "status": 204, "headers": [], "body": "", "entropy": hex(&[5u8; 16]),
    }))
    .unwrap();
    let plain = client_response.decapsulate(&unhex(reply["response"].as_str().unwrap())).unwrap();
    assert_eq!(plain[0], 1, "a known-length response");
    assert_eq!(&plain[1..3], &[0x40, 0xcc], "status 204 as a two-byte varint");
}

// @verifies REQ-SHDR-07#h
#[test]
fn a_request_the_core_seals_opens_in_the_other_implementation_and_the_answer_comes_back() {
    let (theirs, ours, _) = keys([13u8; 32], 1);
    let sealed = ask(json!({
        "op": "ohttpSeal", "keyConfig": ours, "method": "POST", "scheme": "https", "authority": "fleet.example", "path": "/v1/shdr",
        "headers": [["x-np-device-token", "tok"]], "body": hex(b"record"), "entropy": hex(&[6u8; 32]),
    }))
    .unwrap();

    let server = Server::new(theirs).unwrap();
    let (plain, server_response) = server.decapsulate(&unhex(sealed["request"].as_str().unwrap())).unwrap();
    let want = bhttp_request("tok", b"record");
    assert_eq!(&plain[..want.len()], &want[..], "the message the other implementation reads is the one the core meant");
    assert!(plain[want.len()..].iter().all(|b| *b == 0), "and the rest is zero padding");
    assert_eq!(plain.len() % 1024, 0);

    let answer = server_response.encapsulate(&[1, 0x40, 0xcc]).unwrap();
    let o = ask(json!({ "op": "ohttpOpen", "enc": sealed["enc"], "secret": sealed["secret"], "response": hex(&answer) })).unwrap();
    assert_eq!(o["status"], 204);
}
