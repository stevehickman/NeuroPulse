//! The Oblivious HTTP gateway in front of the SHDR fleet ingest (RFC 9458; `NP-INFRA-001` §3.3b; `REQ-SHDR-07`; `OI-FLEET-01` (1)).
//!
//! A helmet's record arrives here sealed to this gateway's key, by way of a **relay** run by someone else. The gateway opens it,
//! forwards the one request it may (`POST /v1/shdr`) to the ingest service on the internal network, and seals the answer. It
//! sees the relay's address and the record, never the phone's address; the relay sees the phone's address and ciphertext, never
//! the record. **That only holds while one party does not run both**, and nothing here can check it.
//!
//! It keeps nothing: no log line is written per request, no address, token or body is stored, and the only thing it ever prints
//! is a count at shutdown. The sealing is the shared core's (`neurone_npps_core::ohttp`, `CLAUDE.md` §22); this file is HTTP
//! plumbing around it, on the standard library alone.
//!
//! | route | |
//! |---|---|
//! | `POST /gateway` with `Content-Type: message/ohttp-req` | `200 message/ohttp-res` (the ingest's status is *inside* it), or `400` when the message does not open |
//! | `GET /ohttp-keys` | `200 application/ohttp-keys`: the key configuration, length-prefixed |
//! | `GET /healthz` | `200` |

use neurone_npps_core::migration::migration_json;
use serde_json::{json, Value};
use std::io::{Read, Write};
use std::net::{SocketAddr, TcpListener, TcpStream, ToSocketAddrs};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

/// The one request the gateway will pass on. Anything else sealed to it is answered `403` inside the seal and goes nowhere, so the
/// gateway cannot be used to reach another service on the internal network.
pub const ALLOWED_METHOD: &str = "POST";
pub const ALLOWED_PATH: &str = "/v1/shdr";
/// Header fields passed to the ingest. Everything else the sender wrote is dropped.
pub const FORWARDED_HEADERS: [&str; 2] = ["x-np-device-token", "content-type"];

const MAX_HEAD: usize = 8 * 1024;
const MAX_BODY: usize = 1024 * 1024;
const TIMEOUT: Duration = Duration::from_secs(10);

pub struct Config {
    pub key_config_hex: String,
    pub private_key_hex: String,
    /// `host:port` of the ingest service, plain HTTP on the internal network.
    pub ingest: String,
}

#[derive(Default)]
pub struct Counters {
    pub forwarded: AtomicU64,
    pub refused: AtomicU64,
    pub ingest_failed: AtomicU64,
}

fn ask(request: Value) -> Result<Value, String> {
    let out = migration_json(&request.to_string())?;
    serde_json::from_str(&out).map_err(|e| e.to_string())
}

fn random(count: usize) -> Vec<u8> {
    let mut b = vec![0u8; count];
    std::fs::File::open("/dev/urandom").and_then(|mut f| f.read_exact(&mut b)).expect("the system random source is unreadable");
    b
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

// ── A small HTTP/1.1 reader ─────────────────────────────────────────────────────────────────────────────────────────

struct Head {
    method: String,
    path: String,
    content_type: Option<String>,
    content_length: Option<usize>,
}

fn read_request(stream: &mut TcpStream) -> Result<(Head, Vec<u8>), u16> {
    let mut buf = Vec::new();
    let mut chunk = [0u8; 2048];
    let end = loop {
        if let Some(i) = buf.windows(4).position(|w| w == b"\r\n\r\n") {
            break i;
        }
        if buf.len() > MAX_HEAD {
            return Err(431);
        }
        match stream.read(&mut chunk) {
            Ok(0) | Err(_) => return Err(400),
            Ok(n) => buf.extend_from_slice(&chunk[..n]),
        }
    };
    let head = std::str::from_utf8(&buf[..end]).map_err(|_| 400u16)?;
    let mut lines = head.split("\r\n");
    let mut first = lines.next().unwrap_or("").split(' ');
    let (method, path) = (first.next().unwrap_or("").to_string(), first.next().unwrap_or("").to_string());
    let (mut content_type, mut content_length) = (None, None);
    for l in lines {
        let Some((k, v)) = l.split_once(':') else { return Err(400) };
        match k.trim().to_ascii_lowercase().as_str() {
            "content-type" => content_type = Some(v.trim().to_string()),
            "content-length" => content_length = Some(v.trim().parse::<usize>().map_err(|_| 400u16)?),
            "transfer-encoding" => return Err(411), // a length, not chunks
            _ => {}
        }
    }
    let mut body = buf[end + 4..].to_vec();
    let want = content_length.unwrap_or(0);
    if want > MAX_BODY {
        return Err(413);
    }
    while body.len() < want {
        match stream.read(&mut chunk) {
            Ok(0) | Err(_) => return Err(400),
            Ok(n) => body.extend_from_slice(&chunk[..n]),
        }
    }
    body.truncate(want);
    Ok((Head { method, path, content_type, content_length }, body))
}

fn reply(stream: &mut TcpStream, status: u16, content_type: &str, body: &[u8]) {
    let reason = match status {
        200 => "OK",
        400 => "Bad Request",
        404 => "Not Found",
        405 => "Method Not Allowed",
        411 => "Length Required",
        413 => "Payload Too Large",
        415 => "Unsupported Media Type",
        431 => "Request Header Fields Too Large",
        _ => "Error",
    };
    let head = format!(
        "HTTP/1.1 {status} {reason}\r\nContent-Type: {content_type}\r\nContent-Length: {}\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
        body.len()
    );
    let _ = stream.write_all(head.as_bytes()).and_then(|_| stream.write_all(body));
}

// ── Forwarding to the ingest ────────────────────────────────────────────────────────────────────────────────────────

/// POST the opened record to the ingest and return its status (502 when it could not be reached). No header the sender chose
/// beyond [`FORWARDED_HEADERS`] is passed on, and no address: the ingest sees this gateway.
fn forward(ingest: &str, headers: &[(String, String)], body: &[u8]) -> u16 {
    let Some(addr) = ingest.to_socket_addrs().ok().and_then(|mut a| a.next()) else { return 502 };
    let Ok(mut s) = TcpStream::connect_timeout(&addr, TIMEOUT) else { return 502 };
    let _ = s.set_read_timeout(Some(TIMEOUT));
    let _ = s.set_write_timeout(Some(TIMEOUT));
    let mut req = format!("POST {ALLOWED_PATH} HTTP/1.1\r\nHost: {ingest}\r\nConnection: close\r\nContent-Length: {}\r\n", body.len());
    for (k, v) in headers {
        if FORWARDED_HEADERS.contains(&k.as_str()) && !v.contains(['\r', '\n']) {
            req.push_str(&format!("{k}: {v}\r\n"));
        }
    }
    req.push_str("\r\n");
    let mut message = req.into_bytes();
    message.extend_from_slice(body);
    if s.write_all(&message).is_err() {
        return 502;
    }
    let mut answer = Vec::new();
    let _ = (&mut s).take(16 * 1024).read_to_end(&mut answer);
    std::str::from_utf8(&answer[..answer.len().min(64)])
        .ok()
        .and_then(|t| t.split(' ').nth(1))
        .and_then(|c| c.parse::<u16>().ok())
        .unwrap_or(502)
}

// ── The gateway ─────────────────────────────────────────────────────────────────────────────────────────────────────

fn handle(mut stream: TcpStream, cfg: &Config, counters: &Counters) {
    let _ = stream.set_read_timeout(Some(TIMEOUT));
    let _ = stream.set_write_timeout(Some(TIMEOUT));
    let (head, body) = match read_request(&mut stream) {
        Ok(r) => r,
        Err(code) => return reply(&mut stream, code, "text/plain", b""),
    };
    match (head.method.as_str(), head.path.as_str()) {
        ("GET", "/healthz") => reply(&mut stream, 200, "text/plain", b"ok"),
        ("GET", "/ohttp-keys") => {
            let config: Vec<u8> = (0..cfg.key_config_hex.len() / 2)
                .map(|i| u8::from_str_radix(&cfg.key_config_hex[2 * i..2 * i + 2], 16).unwrap_or(0))
                .collect();
            let mut out = (config.len() as u16).to_be_bytes().to_vec();
            out.extend(config);
            reply(&mut stream, 200, "application/ohttp-keys", &out)
        }
        ("POST", "/gateway") => {
            if head.content_type.as_deref() != Some("message/ohttp-req") {
                counters.refused.fetch_add(1, Ordering::Relaxed);
                return reply(&mut stream, 415, "text/plain", b"");
            }
            if head.content_length.is_none() {
                return reply(&mut stream, 411, "text/plain", b"");
            }
            let opened = ask(json!({
                "op": "ohttpGatewayOpen", "keyConfig": cfg.key_config_hex, "privateKey": cfg.private_key_hex, "request": hex(&body),
            }));
            let Ok(o) = opened else {
                counters.refused.fetch_add(1, Ordering::Relaxed);
                return reply(&mut stream, 400, "text/plain", b"");
            };
            let allowed = o["method"] == ALLOWED_METHOD && o["path"] == ALLOWED_PATH;
            let status = if !allowed {
                counters.refused.fetch_add(1, Ordering::Relaxed);
                403
            } else {
                let headers: Vec<(String, String)> = o["headers"]
                    .as_array()
                    .map(|a| a.iter().filter_map(|p| Some((p[0].as_str()?.to_string(), p[1].as_str()?.to_string()))).collect())
                    .unwrap_or_default();
                let record: Vec<u8> = o["body"].as_str().unwrap_or("").as_bytes().chunks(2)
                    .map(|c| u8::from_str_radix(std::str::from_utf8(c).unwrap_or("0"), 16).unwrap_or(0)).collect();
                let s = forward(&cfg.ingest, &headers, &record);
                if (200..300).contains(&s) {
                    counters.forwarded.fetch_add(1, Ordering::Relaxed);
                } else {
                    counters.ingest_failed.fetch_add(1, Ordering::Relaxed);
                }
                s
            };
            let sealed = ask(json!({
                "op": "ohttpGatewaySeal", "enc": o["enc"], "secret": o["secret"], "status": status, "headers": [], "body": "",
                "entropy": hex(&random(16)),
            }));
            match sealed.ok().and_then(|s| s["response"].as_str().map(str::to_string)) {
                Some(r) => {
                    let bytes: Vec<u8> = r.as_bytes().chunks(2).map(|c| u8::from_str_radix(std::str::from_utf8(c).unwrap_or("0"), 16).unwrap_or(0)).collect();
                    reply(&mut stream, 200, "message/ohttp-res", &bytes)
                }
                None => reply(&mut stream, 400, "text/plain", b""),
            }
        }
        (_, "/gateway") | (_, "/ohttp-keys") | (_, "/healthz") => reply(&mut stream, 405, "text/plain", b""),
        _ => reply(&mut stream, 404, "text/plain", b""),
    }
}

/// Bind and serve on a background thread per connection. Returns the bound address (for port 0) and the counters.
pub fn serve(listen: &str, cfg: Config) -> std::io::Result<(SocketAddr, Arc<Counters>)> {
    let hex_ok = |t: &str| !t.is_empty() && t.len() % 2 == 0 && t.bytes().all(|b| b.is_ascii_hexdigit());
    if !hex_ok(&cfg.key_config_hex) || !hex_ok(&cfg.private_key_hex) {
        return Err(std::io::Error::new(std::io::ErrorKind::InvalidInput, "the key configuration and private key must be hex"));
    }
    let listener = TcpListener::bind(listen)?;
    let addr = listener.local_addr()?;
    let cfg = Arc::new(cfg);
    let counters = Arc::new(Counters::default());
    let c = counters.clone();
    thread::spawn(move || {
        for stream in listener.incoming().flatten() {
            let (cfg, counters) = (cfg.clone(), c.clone());
            thread::spawn(move || handle(stream, &cfg, &counters));
        }
    });
    Ok((addr, counters))
}
