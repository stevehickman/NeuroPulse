//! AWS Signature Version 4 request signing, for the S3 cloud adapter (`SWR-MG-11`, GitHub #566,
//! `docs/np_sw_cloudstore_001.md` §4b). The signature is an HMAC-SHA-256 chain, so it is cryptographic work and lives here,
//! once, for every platform (`CLAUDE.md` §22): the Kotlin, Swift, C# and TypeScript adapters send a request description and
//! add the headers that come back. The core has no clock and no network: the caller supplies the time.
//!
//! | `op` | request | answer |
//! |------|---------|--------|
//! | `awsSigV4` | see below | `{authorization, signedHeaders, signature, headers}` |
//!
//! Request fields: `method`; `host` (as the HTTP client will send it); `path` (**already percent-encoded**, as sent: S3 signs
//! the path once-encoded and verbatim); `query` (a list of `[name, value]` pairs, **not** encoded: the core encodes and sorts
//! them); `headers` (an object of the extra headers the caller will send and wants signed, such as `range`); `payloadHash`
//! (64 lowercase hex digits, or the literal `UNSIGNED-PAYLOAD`, which S3 accepts over HTTPS); `region`; `service`
//! (`s3`); `accessKeyId`; `secretAccessKey`; `sessionToken` (optional, signed as `x-amz-security-token`); `amzDate`
//! (`YYYYMMDDTHHMMSSZ`, UTC).
//!
//! The answer's `headers` are the ones the caller **must add** to the request (`x-amz-date`, `x-amz-content-sha256`, and
//! `x-amz-security-token` when a token was given) together with `authorization`; the caller's own headers and `host` are
//! the caller's to send (an HTTP client sets `host` from the URL). Header names and values are checked for control
//! characters, so a value cannot add a line to the canonical request.
//!
//! The algorithm is Amazon's "Create a signed request" procedure (IAM User Guide, read 2026-10-27): canonical request, string
//! to sign, and the signing key `HMAC(HMAC(HMAC(HMAC("AWS4"+secret, date), region), service), "aws4_request")`. It is checked
//! below against four of Amazon's own S3 examples (GET object with a `Range`, GET `?lifecycle`, GET with a query, and a PUT
//! with extra headers), whose signatures were also reproduced by an independent stdlib implementation.

use crate::migration::hex;
use hmac::{Hmac, Mac};
use serde_json::{json, Map, Value};
use sha2::{Digest, Sha256};
use std::collections::BTreeMap;

type R<T> = Result<T, String>;

fn hmac(key: &[u8], data: &str) -> Vec<u8> {
    let mut m = <Hmac<Sha256> as Mac>::new_from_slice(key).expect("HMAC takes a key of any length");
    m.update(data.as_bytes());
    m.finalize().into_bytes().to_vec()
}

fn text<'a>(req: &'a Value, key: &str) -> R<&'a str> {
    req[key].as_str().filter(|s| !s.is_empty()).ok_or_else(|| format!("{key} is required"))
}

/// RFC 3986 unreserved characters stay; every other byte is `%XX` in upper case.
fn encode(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for b in s.bytes() {
        if b.is_ascii_alphanumeric() || matches!(b, b'-' | b'_' | b'.' | b'~') {
            out.push(b as char);
        } else {
            out.push_str(&format!("%{b:02X}"));
        }
    }
    out
}

fn clean(s: &str, what: &str) -> R<String> {
    if s.chars().any(|c| c.is_control()) {
        return Err(format!("{what} holds a control character"));
    }
    // Leading and trailing blanks go and runs of blanks collapse to one, as the canonical form requires.
    Ok(s.split_whitespace().collect::<Vec<_>>().join(" "))
}

pub(crate) fn sign(req: &Value) -> R<Value> {
    let (method, host, path) = (text(req, "method")?, text(req, "host")?, text(req, "path")?);
    let (region, service) = (text(req, "region")?, text(req, "service")?);
    let (key_id, secret, amz_date) = (text(req, "accessKeyId")?, text(req, "secretAccessKey")?, text(req, "amzDate")?);
    let payload_hash = text(req, "payloadHash")?;
    let b = amz_date.as_bytes();
    let date_ok = b.len() == 16 && b[8] == b'T' && b[15] == b'Z' && b.iter().enumerate().all(|(i, c)| i == 8 || i == 15 || c.is_ascii_digit());
    if !date_ok {
        return Err("amzDate is YYYYMMDDTHHMMSSZ".into());
    }
    let hash_ok = payload_hash == "UNSIGNED-PAYLOAD" || (payload_hash.len() == 64 && payload_hash.bytes().all(|c| matches!(c, b'0'..=b'9' | b'a'..=b'f')));
    if !hash_ok {
        return Err("payloadHash is 64 lowercase hex digits or UNSIGNED-PAYLOAD".into());
    }
    if !path.starts_with('/') || path.chars().any(|c| c.is_control() || c == ' ') {
        return Err("path is an encoded absolute path".into());
    }
    if [method, host, region, service, key_id].iter().any(|s| s.chars().any(|c| c.is_control() || c.is_whitespace())) {
        return Err("method, host, region, service and accessKeyId hold no whitespace or control character".into());
    }

    // Headers: the caller's, then the three the signer owns. A caller header that collides with one of them is refused.
    let mut headers: BTreeMap<String, String> = BTreeMap::new();
    if let Some(extra) = req.get("headers").and_then(Value::as_object) {
        for (name, value) in extra {
            let n = clean(name, "a header name")?.to_ascii_lowercase();
            if n.is_empty() || n.contains(' ') || matches!(n.as_str(), "host" | "x-amz-date" | "x-amz-content-sha256" | "x-amz-security-token" | "authorization") {
                return Err(format!("header '{n}' is not the caller's to set"));
            }
            headers.insert(n, clean(value.as_str().ok_or("a header value is text")?, "a header value")?);
        }
    }
    let mut added = Map::new();
    added.insert("x-amz-date".into(), json!(amz_date));
    added.insert("x-amz-content-sha256".into(), json!(payload_hash));
    if let Some(token) = req.get("sessionToken").and_then(Value::as_str).filter(|t| !t.is_empty()) {
        added.insert("x-amz-security-token".into(), json!(clean(token, "the session token")?));
    }
    for (n, v) in &added {
        headers.insert(n.clone(), v.as_str().unwrap().to_string());
    }
    headers.insert("host".into(), clean(host, "host")?.to_ascii_lowercase());

    // Query: encode, then sort by encoded name and value.
    let mut pairs: Vec<(String, String)> = Vec::new();
    if let Some(q) = req.get("query").and_then(Value::as_array) {
        for p in q {
            let (n, v) = (p.get(0).and_then(Value::as_str), p.get(1).and_then(Value::as_str));
            let (Some(n), Some(v)) = (n, v) else { return Err("query is a list of [name, value] text pairs".into()) };
            pairs.push((encode(n), encode(v)));
        }
    }
    pairs.sort();
    let canonical_query = pairs.iter().map(|(n, v)| format!("{n}={v}")).collect::<Vec<_>>().join("&");

    let signed_headers = headers.keys().cloned().collect::<Vec<_>>().join(";");
    let canonical_headers: String = headers.iter().map(|(n, v)| format!("{n}:{v}\n")).collect();
    let canonical_request = format!("{method}\n{path}\n{canonical_query}\n{canonical_headers}\n{signed_headers}\n{payload_hash}");

    let date = &amz_date[..8];
    let scope = format!("{date}/{region}/{service}/aws4_request");
    let string_to_sign = format!("AWS4-HMAC-SHA256\n{amz_date}\n{scope}\n{}", hex(&Sha256::digest(canonical_request.as_bytes())));
    let k = hmac(format!("AWS4{secret}").as_bytes(), date);
    let k = hmac(&k, region);
    let k = hmac(&k, service);
    let k = hmac(&k, "aws4_request");
    let signature = hex(&hmac(&k, &string_to_sign));
    let authorization = format!("AWS4-HMAC-SHA256 Credential={key_id}/{scope}, SignedHeaders={signed_headers}, Signature={signature}");

    let mut out_headers = added;
    out_headers.insert("authorization".into(), json!(authorization));
    Ok(json!({ "authorization": authorization, "signedHeaders": signed_headers, "signature": signature, "headers": out_headers }))
}

/// The operations of `migration_json` that are request signing.
pub(crate) fn op(name: &str, req: &Value) -> Option<R<Value>> {
    match name {
        "awsSigV4" => Some(sign(req)),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::migration::migration_json;

    const KEY_ID: &str = "AKIAIOSFODNN7EXAMPLE";
    const SECRET: &str = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
    const EMPTY: &str = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    fn ask(v: Value) -> Result<Value, String> {
        migration_json(&v.to_string()).map(|s| serde_json::from_str(&s).unwrap())
    }

    fn base(method: &str, path: &str, query: Value, headers: Value, payload_hash: &str) -> Value {
        json!({ "op": "awsSigV4", "method": method, "host": "examplebucket.s3.amazonaws.com", "path": path, "query": query,
                "headers": headers, "payloadHash": payload_hash, "region": "us-east-1", "service": "s3",
                "accessKeyId": KEY_ID, "secretAccessKey": SECRET, "amzDate": "20130524T000000Z" })
    }

    // @verifies-none a primitive of the core, checked against four of Amazon's published S3 signing examples here and by an independent implementation; no requirement row names it yet.
    #[test]
    fn amazons_published_s3_examples_are_reproduced() {
        let put_hash = hex(&Sha256::digest(b"Welcome to Amazon S3."));
        let cases = [
            ("GET object with a Range", base("GET", "/test.txt", json!([]), json!({ "range": "bytes=0-9" }), EMPTY),
             "f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41", "host;range;x-amz-content-sha256;x-amz-date"),
            ("GET ?lifecycle", base("GET", "/", json!([["lifecycle", ""]]), json!({}), EMPTY),
             "fea454ca298b7da1c68078a5d1bdbfbbe0d65c699e0f91ac7a200a0136783543", "host;x-amz-content-sha256;x-amz-date"),
            ("GET with a query, given out of order", base("GET", "/", json!([["prefix", "J"], ["max-keys", "2"]]), json!({}), EMPTY),
             "34b48302e7b5fa45bde8084f4b7868a86f0a534bc59db6670ed5711ef69dc6f7", "host;x-amz-content-sha256;x-amz-date"),
            ("PUT with extra headers", base("PUT", "/test%24file.text", json!([]),
             json!({ "Date": "Fri, 24 May 2013 00:00:00 GMT", "x-amz-storage-class": "REDUCED_REDUNDANCY" }), &put_hash),
             "98ad721746da40c64f1a55b78f14c238d841ea1380cd77a1b5971af0ece108bd", "date;host;x-amz-content-sha256;x-amz-date;x-amz-storage-class"),
        ];
        for (name, req, signature, signed) in cases {
            let a = ask(req).unwrap();
            assert_eq!(a["signature"], signature, "{name}");
            assert_eq!(a["signedHeaders"], signed, "{name}");
            assert_eq!(
                a["authorization"],
                format!("AWS4-HMAC-SHA256 Credential={KEY_ID}/20130524/us-east-1/s3/aws4_request, SignedHeaders={signed}, Signature={signature}"),
                "{name}"
            );
        }
    }

    // @verifies-none a primitive of the core; the headers a caller must add are part of the signing contract.
    #[test]
    fn the_answer_names_the_headers_the_caller_must_add() {
        let a = ask(base("GET", "/test.txt", json!([]), json!({}), "UNSIGNED-PAYLOAD")).unwrap();
        assert_eq!(a["headers"]["x-amz-date"], "20130524T000000Z");
        assert_eq!(a["headers"]["x-amz-content-sha256"], "UNSIGNED-PAYLOAD");
        assert_eq!(a["headers"]["authorization"], a["authorization"]);
        assert!(a["headers"].get("x-amz-security-token").is_none());
        assert!(a["headers"].get("host").is_none());
    }

    // @verifies-none a primitive of the core.
    #[test]
    fn a_session_token_is_signed_and_returned() {
        let mut r = base("GET", "/", json!([]), json!({}), EMPTY);
        let plain = ask(r.clone()).unwrap();
        r["sessionToken"] = json!("FQoGZXIvYXdzEXAMPLE");
        let a = ask(r).unwrap();
        assert_eq!(a["headers"]["x-amz-security-token"], "FQoGZXIvYXdzEXAMPLE");
        assert_eq!(a["signedHeaders"], "host;x-amz-content-sha256;x-amz-date;x-amz-security-token");
        assert_ne!(a["signature"], plain["signature"]);
    }

    // @verifies-none a primitive of the core.
    #[test]
    fn query_values_are_encoded_once_and_header_values_are_trimmed() {
        let a = ask(base("GET", "/", json!([["prefix", "a b/c+d"]]), json!({ "X-Note": "  two   words  " }), EMPTY)).unwrap();
        let b = ask(base("GET", "/", json!([["prefix", "a b/c+d"]]), json!({ "x-note": "two words" }), EMPTY)).unwrap();
        assert_eq!(a["signature"], b["signature"]);
        assert_eq!(a["signedHeaders"], "host;x-amz-content-sha256;x-amz-date;x-note");
        let plus = ask(base("GET", "/", json!([["prefix", "a+b"]]), json!({}), EMPTY)).unwrap();
        let space = ask(base("GET", "/", json!([["prefix", "a b"]]), json!({}), EMPTY)).unwrap();
        assert_ne!(plus["signature"], space["signature"], "a plus and a space are different values");
    }

    // @verifies-none a primitive of the core; every refusal is an error, never a signature over something else.
    #[test]
    fn malformed_requests_are_refused() {
        let ok = base("GET", "/", json!([]), json!({}), EMPTY);
        let with = |k: &str, v: Value| { let mut r = ok.clone(); r[k] = v; r };
        for (name, req) in [
            ("a bad date", with("amzDate", json!("2013-05-24"))),
            ("a payload hash of the wrong shape", with("payloadHash", json!("ABC"))),
            ("an upper-case payload hash", with("payloadHash", json!(EMPTY.to_uppercase()))),
            ("a relative path", with("path", json!("test.txt"))),
            ("a path with a space", with("path", json!("/a b"))),
            ("a missing region", with("region", json!(""))),
            ("a header value with a newline", with("headers", json!({ "x-a": "b\nc:d" }))),
            ("a caller header that collides with one the signer owns", with("headers", json!({ "X-Amz-Date": "x" }))),
            ("a host header from the caller", with("headers", json!({ "host": "evil" }))),
            ("a query that is not pairs", with("query", json!([["a"]]))),
        ] {
            assert!(ask(req).is_err(), "{name} must be refused");
        }
    }
}
