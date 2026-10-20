//! Oblivious HTTP (RFC 9458) for the fleet upload: the sender's address never reaches the server that reads the record
//! (`OI-FLEET-01` (1), `REQ-SHDR-07`; `NP-INFRA-001` §3.3b; `CLAUDE.md` §22).
//!
//! The helmet's SHDR record carries a permanent device token, and a plain upload shows the fleet server the connection's
//! network address beside it (`NP-INFRA-001` §3.3a). Here the request is sealed to the *gateway's* key and sent through a
//! *relay* run by someone else: the relay sees the address and an opaque ciphertext, the gateway sees the request and the
//! relay's address. **That separation is the whole protection and it is organisational, not mathematical: the relay and the
//! gateway must not be run by one party, and the relay must keep no log.** Nothing in this file can enforce that.
//!
//! Suite, fixed: DHKEM(X25519, HKDF-SHA256), HKDF-SHA256, AES-128-GCM (`kem_id 0x0020`, `kdf_id 0x0001`, `aead_id 0x0001`,
//! the RFC's own suite); a key configuration naming any other is refused. The request is a binary HTTP message (RFC 9292,
//! known-length form), padded with zero bytes to a multiple of [`PAD_TO`] so the length says less.
//!
//! Entropy comes from the host, as everywhere in this core: an ephemeral key needs 32 bytes, a response needs 16.
//!
//! | `op` | request | answer |
//! |------|---------|--------|
//! | `ohttpGenerateKey` | `seed` (32 bytes, hex), `keyId` (0–255) | `{keyConfig, privateKey}`; `keyConfig` is one key configuration without the length prefix of `application/ohttp-keys` |
//! | `ohttpSeal` | `keyConfig` (hex), `method`, `scheme`, `authority`, `path`, `headers` (`[[name, value]]`), `body` (hex), `entropy` (32 bytes, hex) | `{request, enc, secret}` (hex): send `request` as `message/ohttp-req`; keep `enc` and `secret` in memory until the answer, then drop them |
//! | `ohttpOpen` | `enc`, `secret`, `response` (hex) | `{status, headers, body}`, or an error when the response does not authenticate |
//! | `ohttpGatewayOpen` | `privateKey`, `keyConfig`, `request` (hex) | `{method, scheme, authority, path, headers, body, enc, secret}` |
//! | `ohttpGatewaySeal` | `enc`, `secret`, `status`, `headers`, `body` (hex), `entropy` (16 bytes, hex) | `{response}` (hex), sent as `message/ohttp-res` |

use crate::migration::{hex, unhex};
use hpke::aead::AesGcm128;
use hpke::kdf::HkdfSha256;
use hpke::kem::X25519HkdfSha256;
use hpke::{Deserializable, Kem as KemTrait, OpModeR, OpModeS, Serializable};
use hkdf::Hkdf;
use serde_json::{json, Value};
use sha2::Sha256;
use zeroize::Zeroizing;

type R<T> = Result<T, String>;
type Kem = X25519HkdfSha256;

const KEM_ID: u16 = 0x0020;
const KDF_ID: u16 = 0x0001;
const AEAD_ID: u16 = 0x0001;
const NK: usize = 16;
const NN: usize = 12;
/// A request is padded with zero bytes to a multiple of this (RFC 9292 §3.8 allows trailing zero padding).
pub const PAD_TO: usize = 1024;
const MAX_MESSAGE: usize = 1 << 20;

/// A "random" source that hands out the bytes the host supplied and refuses to invent more: the core has no entropy of its own.
struct HostEntropy {
    bytes: Zeroizing<Vec<u8>>,
    used: usize,
}

impl rand_core09::RngCore for HostEntropy {
    fn next_u32(&mut self) -> u32 {
        let mut b = [0u8; 4];
        self.fill_bytes(&mut b);
        u32::from_le_bytes(b)
    }
    fn next_u64(&mut self) -> u64 {
        let mut b = [0u8; 8];
        self.fill_bytes(&mut b);
        u64::from_le_bytes(b)
    }
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        // Taking more than was given would repeat bytes, which for a key is a break; stop instead.
        let end = self.used + dest.len();
        assert!(end <= self.bytes.len(), "the host supplied too little entropy");
        dest.copy_from_slice(&self.bytes[self.used..end]);
        self.used = end;
    }
}
impl rand_core09::CryptoRng for HostEntropy {}

fn field(req: &Value, key: &str) -> R<Vec<u8>> {
    unhex(req[key].as_str().ok_or_else(|| format!("{key} is required"))?, key)
}

fn text<'a>(req: &'a Value, key: &str) -> R<&'a str> {
    req[key].as_str().ok_or_else(|| format!("{key} is required"))
}

// ── Key configuration (RFC 9458 §3.1) ────────────────────────────────────────────────────────────────────────────

fn key_config(key_id: u8, public: &[u8]) -> Vec<u8> {
    let mut c = vec![key_id];
    c.extend(KEM_ID.to_be_bytes());
    c.extend(public);
    c.extend(4u16.to_be_bytes());
    c.extend(KDF_ID.to_be_bytes());
    c.extend(AEAD_ID.to_be_bytes());
    c
}

/// (key id, the recipient's public key) of a configuration that names this module's KEM and offers its KDF and AEAD among
/// the suites it lists (RFC 9458 §3.1: a configuration lists every suite the gateway accepts, and the RFC's own example lists two).
fn parse_key_config(c: &[u8]) -> R<(u8, <Kem as KemTrait>::PublicKey)> {
    let refuse = || "not a key configuration for the supported suite".to_string();
    if c.len() < 1 + 2 + 32 + 2 {
        return Err(refuse());
    }
    let alg_len = u16::from_be_bytes([c[35], c[36]]) as usize;
    if alg_len == 0 || alg_len % 4 != 0 || c.len() != 37 + alg_len {
        return Err(refuse());
    }
    let ours = c[37..].chunks(4).any(|s| u16::from_be_bytes([s[0], s[1]]) == KDF_ID && u16::from_be_bytes([s[2], s[3]]) == AEAD_ID);
    if u16::from_be_bytes([c[1], c[2]]) != KEM_ID || !ours {
        return Err("the key configuration names a suite this build does not use".into());
    }
    let public = <Kem as KemTrait>::PublicKey::from_bytes(&c[3..35]).map_err(|_| "not a public key".to_string())?;
    Ok((c[0], public))
}

fn header(key_id: u8) -> Vec<u8> {
    let mut h = vec![key_id];
    h.extend(KEM_ID.to_be_bytes());
    h.extend(KDF_ID.to_be_bytes());
    h.extend(AEAD_ID.to_be_bytes());
    h
}

fn request_info(hdr: &[u8]) -> Vec<u8> {
    let mut info = b"message/bhttp request".to_vec();
    info.push(0);
    info.extend(hdr);
    info
}

// ── Binary HTTP (RFC 9292), known-length ─────────────────────────────────────────────────────────────────────────

fn put_varint(out: &mut Vec<u8>, n: usize) {
    let n = n as u64;
    match n {
        0..=63 => out.push(n as u8),
        64..=16383 => out.extend((0x4000 | n as u16).to_be_bytes()),
        _ => out.extend((0x8000_0000 | n as u32).to_be_bytes()),
    }
}

fn put_str(out: &mut Vec<u8>, s: &[u8]) {
    put_varint(out, s.len());
    out.extend(s);
}

struct Cursor<'a>(&'a [u8]);

impl<'a> Cursor<'a> {
    fn varint(&mut self) -> R<usize> {
        let first = *self.0.first().ok_or("truncated message")?;
        let len = 1usize << (first >> 6);
        if self.0.len() < len {
            return Err("truncated message".into());
        }
        let mut n = (first & 0x3f) as u64;
        for b in &self.0[1..len] {
            n = (n << 8) | *b as u64;
        }
        self.0 = &self.0[len..];
        usize::try_from(n).map_err(|_| "too long".to_string())
    }
    fn bytes(&mut self) -> R<&'a [u8]> {
        let n = self.varint()?;
        if n > self.0.len() {
            return Err("truncated message".into());
        }
        let (a, b) = self.0.split_at(n);
        self.0 = b;
        Ok(a)
    }
}

fn utf8(b: &[u8]) -> R<String> {
    String::from_utf8(b.to_vec()).map_err(|_| "not text".to_string())
}

fn encode_fields(headers: &[(String, String)]) -> Vec<u8> {
    let mut f = Vec::new();
    for (n, v) in headers {
        put_str(&mut f, n.to_ascii_lowercase().as_bytes());
        put_str(&mut f, v.as_bytes());
    }
    f
}

fn decode_fields(section: &[u8]) -> R<Vec<(String, String)>> {
    let mut c = Cursor(section);
    let mut out = Vec::new();
    while !c.0.is_empty() {
        out.push((utf8(c.bytes()?)?, utf8(c.bytes()?)?));
    }
    Ok(out)
}

struct Request {
    method: String,
    scheme: String,
    authority: String,
    path: String,
    headers: Vec<(String, String)>,
    body: Vec<u8>,
}

fn encode_request(r: &Request) -> Vec<u8> {
    let mut m = vec![0u8]; // framing indicator: known-length request
    for s in [&r.method, &r.scheme, &r.authority, &r.path] {
        put_str(&mut m, s.as_bytes());
    }
    put_str(&mut m, &encode_fields(&r.headers));
    put_str(&mut m, &r.body);
    put_varint(&mut m, 0); // no trailers
    let padded = m.len().div_ceil(PAD_TO) * PAD_TO;
    m.resize(padded, 0);
    m
}

/// A message may end after any whole field once the rest is empty (RFC 9292 §3.8: the sender may leave off what is empty, and the
/// RFC 9458 example does), so a missing section reads as an empty one. Anything that is there must be well formed.
fn decode_request(m: &[u8]) -> R<Request> {
    let mut c = Cursor(m);
    if c.varint()? != 0 {
        return Err("not a known-length binary HTTP request".into());
    }
    let (method, scheme, authority, path) = (utf8(c.bytes()?)?, utf8(c.bytes()?)?, utf8(c.bytes()?)?, utf8(c.bytes()?)?);
    let headers = if c.0.is_empty() { Vec::new() } else { decode_fields(c.bytes()?)? };
    let body = if c.0.is_empty() { Vec::new() } else { c.bytes()?.to_vec() };
    if !c.0.is_empty() {
        c.bytes()?; // trailers
    }
    if c.0.iter().any(|b| *b != 0) {
        return Err("anything after the trailers must be zero padding".into());
    }
    Ok(Request { method, scheme, authority, path, headers, body })
}

fn encode_response(status: u16, headers: &[(String, String)], body: &[u8]) -> Vec<u8> {
    let mut m = vec![1u8]; // framing indicator: known-length response
    put_varint(&mut m, status as usize);
    put_str(&mut m, &encode_fields(headers));
    put_str(&mut m, body);
    put_varint(&mut m, 0);
    m
}

fn decode_response(m: &[u8]) -> R<(u16, Vec<(String, String)>, Vec<u8>)> {
    let mut c = Cursor(m);
    if c.varint()? != 1 {
        return Err("not a known-length binary HTTP response".into());
    }
    let status = u16::try_from(c.varint()?).map_err(|_| "bad status".to_string())?;
    let headers = if c.0.is_empty() { Vec::new() } else { decode_fields(c.bytes()?)? };
    let body = if c.0.is_empty() { Vec::new() } else { c.bytes()?.to_vec() };
    if !c.0.is_empty() {
        c.bytes()?;
    }
    Ok((status, headers, body))
}

fn headers_in(v: &Value) -> R<Vec<(String, String)>> {
    v.as_array()
        .ok_or("headers must be an array")?
        .iter()
        .map(|p| match (p[0].as_str(), p[1].as_str()) {
            (Some(n), Some(v)) => Ok((n.to_string(), v.to_string())),
            _ => Err("a header is [name, value] text".to_string()),
        })
        .collect()
}

fn headers_out(h: &[(String, String)]) -> Value {
    Value::Array(h.iter().map(|(n, v)| json!([n, v])).collect())
}

// ── Response encapsulation (RFC 9458 §4.4) ───────────────────────────────────────────────────────────────────────

fn response_keys(enc: &[u8], secret: &[u8], nonce: &[u8]) -> R<([u8; NK], [u8; NN])> {
    let salt = [enc, nonce].concat();
    let hk = Hkdf::<Sha256>::new(Some(&salt), secret);
    let (mut key, mut iv) = ([0u8; NK], [0u8; NN]);
    hk.expand(b"key", &mut key).map_err(|_| "key derivation")?;
    hk.expand(b"nonce", &mut iv).map_err(|_| "key derivation")?;
    Ok((key, iv))
}

fn aead_seal(key: &[u8; NK], iv: &[u8; NN], plaintext: &[u8]) -> R<Vec<u8>> {
    use aes_gcm::aead::{Aead, KeyInit};
    use aes_gcm::{Aes128Gcm, Nonce};
    Aes128Gcm::new(key.into()).encrypt(Nonce::from_slice(iv), plaintext).map_err(|_| "seal failed".to_string())
}

fn aead_open(key: &[u8; NK], iv: &[u8; NN], ct: &[u8]) -> R<Vec<u8>> {
    use aes_gcm::aead::{Aead, KeyInit};
    use aes_gcm::{Aes128Gcm, Nonce};
    Aes128Gcm::new(key.into()).decrypt(Nonce::from_slice(iv), ct).map_err(|_| "the response did not authenticate".to_string())
}

// ── The operations ───────────────────────────────────────────────────────────────────────────────────────────────

fn generate_key(req: &Value) -> R<Value> {
    let seed = Zeroizing::new(field(req, "seed")?);
    if seed.len() != 32 {
        return Err("a key seed is 32 bytes".into());
    }
    let key_id = u8::try_from(req["keyId"].as_u64().ok_or("keyId is required")?).map_err(|_| "keyId is 0 to 255".to_string())?;
    let (sk, pk) = Kem::derive_keypair(&seed);
    Ok(json!({ "keyConfig": hex(&key_config(key_id, &pk.to_bytes())), "privateKey": hex(&sk.to_bytes()) }))
}

fn seal(req: &Value) -> R<Value> {
    let (key_id, pk) = parse_key_config(&field(req, "keyConfig")?)?;
    let message = encode_request(&Request {
        method: text(req, "method")?.into(),
        scheme: text(req, "scheme")?.into(),
        authority: text(req, "authority")?.into(),
        path: text(req, "path")?.into(),
        headers: headers_in(&req["headers"])?,
        body: field(req, "body")?,
    });
    if message.len() > MAX_MESSAGE {
        return Err("the request is too large".into());
    }
    let entropy = Zeroizing::new(field(req, "entropy")?);
    if entropy.len() != 32 {
        return Err("a request needs 32 bytes of entropy".into());
    }
    let hdr = header(key_id);
    let mut rng = HostEntropy { bytes: entropy, used: 0 };
    let (enc, mut ctx) = hpke::setup_sender::<AesGcm128, HkdfSha256, Kem, _>(&OpModeS::Base, &pk, &request_info(&hdr), &mut rng)
        .map_err(|_| "could not seal the request".to_string())?;
    let ct = ctx.seal(&message, &[]).map_err(|_| "could not seal the request".to_string())?;
    let mut secret = Zeroizing::new([0u8; NK]);
    ctx.export(b"message/bhttp response", &mut *secret).map_err(|_| "export".to_string())?;
    let enc = enc.to_bytes();
    let request = [hdr, enc.to_vec(), ct].concat();
    Ok(json!({ "request": hex(&request), "enc": hex(&enc), "secret": hex(&*secret) }))
}

fn open(req: &Value) -> R<Value> {
    let (enc, secret, response) = (field(req, "enc")?, Zeroizing::new(field(req, "secret")?), field(req, "response")?);
    if response.len() < NK + 16 {
        return Err("the response is too short".into());
    }
    let (nonce, ct) = response.split_at(NK);
    let (key, iv) = response_keys(&enc, &secret, nonce)?;
    let (status, headers, body) = decode_response(&aead_open(&key, &iv, ct)?)?;
    Ok(json!({ "status": status, "headers": headers_out(&headers), "body": hex(&body) }))
}

fn gateway_open(req: &Value) -> R<Value> {
    let (key_id, _) = parse_key_config(&field(req, "keyConfig")?)?;
    let sk_bytes = Zeroizing::new(field(req, "privateKey")?);
    let sk = <Kem as KemTrait>::PrivateKey::from_bytes(&sk_bytes).map_err(|_| "not a private key".to_string())?;
    let request = field(req, "request")?;
    if request.len() < 7 + 32 + 16 {
        return Err("the request is too short".into());
    }
    let hdr = header(key_id);
    if request[..7] != hdr[..] {
        return Err("the request is for another key or suite".into());
    }
    let enc_bytes = &request[7..39];
    let enc = <Kem as KemTrait>::EncappedKey::from_bytes(enc_bytes).map_err(|_| "not an encapsulated key".to_string())?;
    let mut ctx = hpke::setup_receiver::<AesGcm128, HkdfSha256, Kem>(&OpModeR::Base, &sk, &enc, &request_info(&hdr))
        .map_err(|_| "the request did not open".to_string())?;
    let plain = Zeroizing::new(ctx.open(&request[39..], &[]).map_err(|_| "the request did not open".to_string())?);
    let r = decode_request(&plain)?;
    let mut secret = Zeroizing::new([0u8; NK]);
    ctx.export(b"message/bhttp response", &mut *secret).map_err(|_| "export".to_string())?;
    Ok(json!({
        "method": r.method, "scheme": r.scheme, "authority": r.authority, "path": r.path,
        "headers": headers_out(&r.headers), "body": hex(&r.body),
        "enc": hex(enc_bytes), "secret": hex(&*secret),
    }))
}

fn gateway_seal(req: &Value) -> R<Value> {
    let (enc, secret) = (field(req, "enc")?, Zeroizing::new(field(req, "secret")?));
    let nonce = field(req, "entropy")?;
    if nonce.len() != NK {
        return Err("a response needs 16 bytes of entropy".into());
    }
    let status = u16::try_from(req["status"].as_u64().ok_or("status is required")?).map_err(|_| "bad status".to_string())?;
    let (key, iv) = response_keys(&enc, &secret, &nonce)?;
    let ct = aead_seal(&key, &iv, &encode_response(status, &headers_in(&req["headers"])?, &field(req, "body")?))?;
    Ok(json!({ "response": hex(&[nonce, ct].concat()) }))
}

pub(crate) fn op(name: &str, req: &Value) -> Option<R<Value>> {
    Some(match name {
        "ohttpGenerateKey" => generate_key(req),
        "ohttpSeal" => seal(req),
        "ohttpOpen" => open(req),
        "ohttpGatewayOpen" => gateway_open(req),
        "ohttpGatewaySeal" => gateway_seal(req),
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ask(name: &str, v: Value) -> R<Value> {
        op(name, &v).unwrap()
    }
    fn bytes(n: usize, fill: u8) -> String {
        hex(&vec![fill; n])
    }

    fn key() -> (String, String) {
        let k = ask("ohttpGenerateKey", json!({ "seed": bytes(32, 7), "keyId": 1 })).unwrap();
        (k["keyConfig"].as_str().unwrap().into(), k["privateKey"].as_str().unwrap().into())
    }

    fn sealed(config: &str, body: &str) -> Value {
        ask("ohttpSeal", json!({
            "keyConfig": config, "method": "POST", "scheme": "https", "authority": "fleet.example", "path": "/v1/shdr",
            "headers": [["X-NP-Device-Token", "abc"], ["Content-Type", "application/octet-stream"]],
            "body": body, "entropy": bytes(32, 9),
        })).unwrap()
    }

    #[test]
    fn a_request_goes_through_and_the_answer_comes_back() {
        let (config, sk) = key();
        let s = sealed(&config, &hex(b"the record"));
        let g = ask("ohttpGatewayOpen", json!({ "keyConfig": config, "privateKey": sk, "request": s["request"] })).unwrap();
        assert_eq!(g["method"], "POST");
        assert_eq!(g["path"], "/v1/shdr");
        assert_eq!(g["headers"][0], json!(["x-np-device-token", "abc"]));
        assert_eq!(g["body"], hex(b"the record"));
        assert_eq!(g["enc"], s["enc"]);
        assert_eq!(g["secret"], s["secret"], "both ends derive the same response secret");
        let r = ask("ohttpGatewaySeal", json!({ "enc": g["enc"], "secret": g["secret"], "status": 200, "headers": [], "body": "", "entropy": bytes(16, 3) })).unwrap();
        let o = ask("ohttpOpen", json!({ "enc": s["enc"], "secret": s["secret"], "response": r["response"] })).unwrap();
        assert_eq!(o["status"], 200);
    }

    #[test]
    fn the_request_does_not_show_what_it_carries_and_its_length_is_a_bucket() {
        let (config, _) = key();
        let short = sealed(&config, &hex(b"x"));
        let longer = sealed(&config, &hex(&vec![0x41; 400]));
        let (a, b) = (short["request"].as_str().unwrap(), longer["request"].as_str().unwrap());
        assert_eq!(a.len(), b.len(), "two bodies in one bucket are one length");
        assert!(!a.contains(&hex(b"fleet.example")) && !a.contains(&hex(b"abc")));
        let big = sealed(&config, &hex(&vec![0x41; 2000]));
        assert!(big["request"].as_str().unwrap().len() > a.len());
        assert_eq!((a.len() / 2 - 7 - 32 - 16) % PAD_TO, 0);
    }

    #[test]
    fn a_wrong_key_a_tampered_request_and_a_tampered_response_are_refused() {
        let (config, sk) = key();
        let s = sealed(&config, &hex(b"r"));
        let other = ask("ohttpGenerateKey", json!({ "seed": bytes(32, 8), "keyId": 1 })).unwrap();
        assert!(ask("ohttpGatewayOpen", json!({ "keyConfig": config, "privateKey": other["privateKey"], "request": s["request"] })).is_err());
        let mut r = s["request"].as_str().unwrap().to_string();
        let last = r.pop().unwrap();
        r.push(if last == '0' { '1' } else { '0' });
        assert!(ask("ohttpGatewayOpen", json!({ "keyConfig": config, "privateKey": sk, "request": r })).is_err());
        let g = ask("ohttpGatewayOpen", json!({ "keyConfig": config, "privateKey": sk, "request": s["request"] })).unwrap();
        let resp = ask("ohttpGatewaySeal", json!({ "enc": g["enc"], "secret": g["secret"], "status": 200, "headers": [], "body": "", "entropy": bytes(16, 3) })).unwrap();
        let mut t = resp["response"].as_str().unwrap().to_string();
        let last = t.pop().unwrap();
        t.push(if last == '0' { '1' } else { '0' });
        assert!(ask("ohttpOpen", json!({ "enc": s["enc"], "secret": s["secret"], "response": t })).is_err());
        let wrong = ask("ohttpOpen", json!({ "enc": s["enc"], "secret": bytes(16, 1), "response": resp["response"] }));
        assert!(wrong.is_err());
    }

    #[test]
    fn only_the_one_suite_is_accepted_and_entropy_is_checked() {
        let (config, _) = key();
        let mut other = unhex(&config, "c").unwrap();
        other[2] = 0x10; // another KEM
        assert!(ask("ohttpSeal", json!({ "keyConfig": hex(&other), "method": "POST", "scheme": "https", "authority": "a", "path": "/", "headers": [], "body": "", "entropy": bytes(32, 1) })).is_err());
        assert!(ask("ohttpSeal", json!({ "keyConfig": config, "method": "POST", "scheme": "https", "authority": "a", "path": "/", "headers": [], "body": "", "entropy": bytes(31, 1) })).is_err());
    }
    // ── Published vectors ───────────────────────────────────────────────────────────────────────────────────────────

    fn h(s: &str) -> Vec<u8> {
        unhex(s, "v").unwrap()
    }

    /// RFC 9180 Appendix A.1.1 (DHKEM(X25519, HKDF-SHA256), HKDF-SHA256, AES-128-GCM, base mode), taken from the CFRG test-vector
    /// file the `hpke` crate ships (`test-vectors-5f503c5.json`, mode 0, KEM 0x0020, KDF 1, AEAD 1). The ephemeral key is derived
    /// from the vector's `ikmE` by the same path `seal` takes, with the host's entropy standing for `ikmE`, so this shows the
    /// feature set and the entropy plumbing produce the RFC's `enc`, ciphertext and exported secret.
    #[test]
    fn rfc_9180_a_1_1_sender_matches() {
        let v: Value = serde_json::from_str(include_str!("../tests/data/hpke-rfc9180-a1-1.json")).unwrap();
        let g = |k: &str| h(v[k].as_str().unwrap());
        let pk = <Kem as KemTrait>::PublicKey::from_bytes(&g("pkRm")).unwrap();
        let mut rng = HostEntropy { bytes: Zeroizing::new(g("ikmE")), used: 0 };
        let (enc, mut ctx) =
            hpke::setup_sender::<AesGcm128, HkdfSha256, Kem, _>(&OpModeS::Base, &pk, &g("info"), &mut rng).unwrap();
        assert_eq!(enc.to_bytes().to_vec(), g("enc"));
        for e in v["encryptions"].as_array().unwrap() {
            let ct = ctx.seal(&h(e["pt"].as_str().unwrap()), &h(e["aad"].as_str().unwrap())).unwrap();
            assert_eq!(ct, h(e["ct"].as_str().unwrap()));
        }
        for x in v["exports"].as_array().unwrap() {
            let mut out = vec![0u8; x["L"].as_u64().unwrap() as usize];
            ctx.export(&h(x["exporter_context"].as_str().unwrap()), &mut out).unwrap();
            assert_eq!(out, h(x["exported_value"].as_str().unwrap()));
        }
        // And the receiver, from the recipient's private key, opens what the sender sealed.
        let sk = <Kem as KemTrait>::PrivateKey::from_bytes(&g("skRm")).unwrap();
        let encapped = <Kem as KemTrait>::EncappedKey::from_bytes(&g("enc")).unwrap();
        let mut r = hpke::setup_receiver::<AesGcm128, HkdfSha256, Kem>(&OpModeR::Base, &sk, &encapped, &g("info")).unwrap();
        let e = &v["encryptions"][0];
        assert_eq!(r.open(&h(e["ct"].as_str().unwrap()), &h(e["aad"].as_str().unwrap())).unwrap(), h(e["pt"].as_str().unwrap()));
    }

    // RFC 9458 Appendix A (draft-ietf-ohai-ohttp, "Complete Example of a Request and Response").
    const A_SK: &str = "3c168975674b2fa8e465970b79c8dcf09f1c741626480bd4c6162fc5b6a98e1a";
    const A_CONFIG: &str = "01002031e1f05a740102115220e9af918f738674aec95f54db6e04eb705aae8e79815500080001000100010003";
    const A_REQUEST: &str = "010020000100014b28f881333e7c164ffc499ad9796f877f4e1051ee6d31bad19dec96c208b4726374e469135906992e1268c594d2a10c695d858c40a026e7965e7d86b83dd440b2c0185204b4d63525";
    const A_ENC: &str = "4b28f881333e7c164ffc499ad9796f877f4e1051ee6d31bad19dec96c208b472";
    const A_SECRET: &str = "62d87a6ba569ee81014c2641f52bea36";
    const A_NONCE: &str = "c789e7151fcba46158ca84b04464910d";
    const A_RESPONSE: &str = "c789e7151fcba46158ca84b04464910d86f9013e404feea014e7be4a441f234f857fbd";

    #[test]
    fn rfc_9458_appendix_a_request_opens_to_the_published_message() {
        let g = ask("ohttpGatewayOpen", json!({ "keyConfig": A_CONFIG, "privateKey": A_SK, "request": A_REQUEST })).unwrap();
        // 00034745540568747470730b6578616d706c652e636f6d012f: GET https://example.com/
        assert_eq!((g["method"].as_str(), g["scheme"].as_str(), g["authority"].as_str(), g["path"].as_str()),
                   (Some("GET"), Some("https"), Some("example.com"), Some("/")));
        assert_eq!(g["headers"], json!([]));
        assert_eq!(g["body"], "");
        assert_eq!(g["enc"], A_ENC, "the encapsulated key the response is salted with");
        assert_eq!(g["secret"], A_SECRET, "the exported response secret");
    }

    #[test]
    fn rfc_9458_appendix_a_response_keys_and_ciphertext_match() {
        let (key, nonce) = response_keys(&h(A_ENC), &h(A_SECRET), &h(A_NONCE)).unwrap();
        assert_eq!(hex(&key), "5d0172a080e428b16d298c4ea0db620d");
        assert_eq!(hex(&nonce), "f6bf1aeb88d6df87007fa263");
        let ct = aead_seal(&key, &nonce, &h("0140c8")).unwrap();
        assert_eq!(hex(&[h(A_NONCE), ct].concat()), A_RESPONSE);
    }

    #[test]
    fn rfc_9458_appendix_a_response_opens_to_a_200() {
        let o = ask("ohttpOpen", json!({ "enc": A_ENC, "secret": A_SECRET, "response": A_RESPONSE })).unwrap();
        assert_eq!(o["status"], 200);
        assert_eq!(o["body"], "");
    }

    #[test]
    fn the_rfc_9458_key_configuration_lists_two_suites_and_is_accepted_for_the_private_key_it_names() {
        let (id, pk) = parse_key_config(&h(A_CONFIG)).unwrap();
        assert_eq!(id, 1);
        let sk = <Kem as KemTrait>::PrivateKey::from_bytes(&h(A_SK)).unwrap();
        assert_eq!(pk.to_bytes(), <Kem as KemTrait>::sk_to_pk(&sk).to_bytes(), "the configuration's public key is the secret key's");
        // A configuration that offers only other suites is refused.
        let mut other = h(A_CONFIG);
        other[37..41].copy_from_slice(&[0, 2, 0, 1]); // HKDF-SHA384 only, once the second suite is also changed
        other[41..45].copy_from_slice(&[0, 2, 0, 3]);
        assert!(parse_key_config(&other).is_err());
    }
}

/// The browser build has no operating-system entropy and this core takes none (see `Cargo.toml`): refuse every request.
#[cfg(target_arch = "wasm32")]
fn no_entropy(_: &mut [u8]) -> Result<(), getrandom::Error> {
    Err(getrandom::Error::UNSUPPORTED)
}
#[cfg(target_arch = "wasm32")]
getrandom::register_custom_getrandom!(no_entropy);
