//! A recording sealed for a backup: one file, in chunks, under the backup's key (`SWR-MG-03`; `CLAUDE.md` §22).
//!
//! The backup bundle (`migration`) is one object that holds small records and is opened whole, which is right for settings and
//! wrong for a recording of tens of megabytes: it would have to be hex-coded and held in memory as one value, and rewritten
//! every night. A recording is sealed here instead, once, in chunks of [`CHUNK_BYTES`], so a platform handles one chunk at a
//! time and a recording that has been backed up is never sealed again (it does not change). The sealing is here, once, for every
//! platform (`CLAUDE.md` §22): a Kotlin, Swift or C# copy would be a second implementation of cryptography.
//!
//! # The file
//! `header ‖ chunk₀ ‖ chunk₁ ‖ … ‖ chunkₙ`, where the header is `"NPAR" ‖ version (1) ‖ prefix (8 bytes) ‖ kdf length (2, big
//! endian) ‖ kdf (JSON)`. The `kdf` is the bundle key's own block (`bundle_key`): how the key was made from the person's code or
//! passphrase, so a restore on a new phone knows what to ask for. It is present in the file that indexes the others and may be
//! empty in the rest. Every chunk is AES-256-GCM of up to [`CHUNK_BYTES`] bytes plus a 16-byte tag, under the nonce
//! `prefix ‖ counter (4, big endian)`, with the whole header and a final-chunk flag as the associated data. So a chunk moved,
//! repeated, dropped from the middle, or cut off the end fails to open, and a wrong key and an edited file are not told apart
//! (`authentication_failed`). Every chunk is full except the last, which is the one the file ends with and may be empty only if
//! the whole recording is.
//!
//! | operation | arguments | result |
//! |-----------|-----------|--------|
//! | `archiveInfo` | none | `{chunkBytes, tagBytes, prefixBytes, headerBytes}`: the fixed sizes a driver cuts a file by |
//! | `archiveStart` | `prefix` (8 bytes, hex, random), `kdf` (optional) | `{header}` (hex) |
//! | `archiveReadHeader` | `bytes`: the first bytes of a file (hex; at least [`HEADER_BYTES`] and as many more as the `kdf` needs, [`MAX_KDF`] at most) | `{ok: true, headerLength, kdf}` or `{ok: false, reason}`: `malformed`, `truncated` or `newer_version` |
//! | `archiveSealChunk` | `key`, `header` (hex, the whole header), `counter`, `last`, `data` (hex) | `{sealed}` (hex) |
//! | `archiveOpenChunk` | `key`, `header`, `counter`, `last`, `sealed` | `{ok: true, data}` or `{ok: false, reason: "authentication_failed"}` |

use crate::bundle_key;
use crate::migration::{hex, unhex};
use aes_gcm::aead::{Aead, KeyInit, Payload};
use aes_gcm::{Aes256Gcm, Nonce};
use serde_json::{json, Value};

type R<T> = Result<T, String>;

const MAGIC: &[u8; 4] = b"NPAR";
const VERSION: u8 = 1;
/// A chunk's plaintext length. A platform cuts a recording by this and the core refuses a longer one.
pub const CHUNK_BYTES: usize = 64 * 1024;
const TAG_BYTES: usize = 16;
const PREFIX_BYTES: usize = 8;
/// Magic, version, prefix and the length of the kdf: the part of the header that is always there.
pub const HEADER_BYTES: usize = 4 + 1 + PREFIX_BYTES + 2;
/// The longest kdf block a header may carry. The block is a few dozen bytes; anything near this is not one.
pub const MAX_KDF: usize = 1024;
const KEY_LEN: usize = 32;

fn cipher(req: &Value) -> R<Aes256Gcm> {
    let key = unhex(req["key"].as_str().ok_or("key is required")?, "key")?;
    if key.len() != KEY_LEN {
        return Err(format!("the key is {KEY_LEN} bytes"));
    }
    Aes256Gcm::new_from_slice(&key).map_err(|_| "the key is not usable".to_string())
}

/// The nonce of chunk `counter` and its associated data (the header, then whether it is the last chunk).
fn frame(req: &Value) -> R<([u8; 12], Vec<u8>)> {
    let header = unhex(req["header"].as_str().ok_or("header is required")?, "header")?;
    let parsed = read_header(&header)?;
    let Value::Object(_) = parsed else { return Err("the header is not readable".into()) };
    if parsed["ok"] != true {
        return Err(format!("the header is not usable: {}", parsed["reason"].as_str().unwrap_or("malformed")));
    }
    if parsed["headerLength"].as_u64() != Some(header.len() as u64) {
        return Err("the header has bytes after it".into());
    }
    let counter = req["counter"].as_u64().ok_or("counter is required")?;
    if counter > u64::from(u32::MAX) {
        return Err("a recording has too many chunks".into());
    }
    let last = req["last"].as_bool().ok_or("last is required")?;
    let mut nonce = [0u8; 12];
    nonce[..PREFIX_BYTES].copy_from_slice(&header[5..5 + PREFIX_BYTES]);
    nonce[PREFIX_BYTES..].copy_from_slice(&(counter as u32).to_be_bytes());
    let mut aad = header;
    aad.push(u8::from(last));
    Ok((nonce, aad))
}

fn start(req: &Value) -> R<Value> {
    let prefix = unhex(req["prefix"].as_str().ok_or("prefix is required")?, "prefix")?;
    if prefix.len() != PREFIX_BYTES {
        return Err(format!("the prefix is {PREFIX_BYTES} bytes"));
    }
    let kdf = match req.get("kdf") {
        None | Some(Value::Null) => Vec::new(),
        Some(block) => {
            bundle_key::check_kdf(block).map_err(|e| format!("kdf: {e}"))?;
            block.to_string().into_bytes()
        }
    };
    if kdf.len() > MAX_KDF {
        return Err("the kdf block is too long".into());
    }
    let mut header = MAGIC.to_vec();
    header.push(VERSION);
    header.extend_from_slice(&prefix);
    header.extend_from_slice(&(kdf.len() as u16).to_be_bytes());
    header.extend_from_slice(&kdf);
    Ok(json!({ "header": hex(&header) }))
}

/// What the header at the front of `bytes` says, or why it cannot be read. Never panics on a short or edited file.
fn read_header(bytes: &[u8]) -> R<Value> {
    if bytes.len() < HEADER_BYTES {
        return Ok(json!({ "ok": false, "reason": "truncated" }));
    }
    if &bytes[..4] != MAGIC {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    }
    if bytes[4] != VERSION {
        return Ok(json!({ "ok": false, "reason": if bytes[4] > VERSION { "newer_version" } else { "malformed" } }));
    }
    let kdf_len = u16::from_be_bytes([bytes[13], bytes[14]]) as usize;
    if kdf_len > MAX_KDF {
        return Ok(json!({ "ok": false, "reason": "malformed" }));
    }
    let end = HEADER_BYTES + kdf_len;
    if bytes.len() < end {
        return Ok(json!({ "ok": false, "reason": "truncated" }));
    }
    let kdf = if kdf_len == 0 {
        Value::Null
    } else {
        match serde_json::from_slice::<Value>(&bytes[HEADER_BYTES..end]) {
            Ok(v) if bundle_key::check_kdf(&v).is_ok() => v,
            _ => return Ok(json!({ "ok": false, "reason": "malformed" })),
        }
    };
    Ok(json!({ "ok": true, "headerLength": end, "kdf": kdf }))
}

fn seal_chunk(req: &Value) -> R<Value> {
    let (nonce, aad) = frame(req)?;
    let data = unhex(req["data"].as_str().ok_or("data is required")?, "data")?;
    if data.len() > CHUNK_BYTES {
        return Err(format!("a chunk is at most {CHUNK_BYTES} bytes"));
    }
    let ct = cipher(req)?
        .encrypt(Nonce::from_slice(&nonce), Payload { msg: &data, aad: &aad })
        .map_err(|_| "encryption failed".to_string())?;
    Ok(json!({ "sealed": hex(&ct) }))
}

fn open_chunk(req: &Value) -> R<Value> {
    let (nonce, aad) = frame(req)?;
    let sealed = unhex(req["sealed"].as_str().ok_or("sealed is required")?, "sealed")?;
    if sealed.len() < TAG_BYTES || sealed.len() > CHUNK_BYTES + TAG_BYTES {
        return Ok(json!({ "ok": false, "reason": "authentication_failed" }));
    }
    Ok(match cipher(req)?.decrypt(Nonce::from_slice(&nonce), Payload { msg: &sealed, aad: &aad }) {
        Ok(plain) => json!({ "ok": true, "data": hex(&plain) }),
        Err(_) => json!({ "ok": false, "reason": "authentication_failed" }),
    })
}

/// The operations of `migration_json` that belong to the archive.
pub fn op(name: &str, req: &Value) -> Option<R<Value>> {
    Some(match name {
        "archiveInfo" => Ok(json!({
            "chunkBytes": CHUNK_BYTES,
            "tagBytes": TAG_BYTES,
            "prefixBytes": PREFIX_BYTES,
            "headerBytes": HEADER_BYTES,
            "maxKdfBytes": MAX_KDF,
        })),
        "archiveStart" => start(req),
        "archiveReadHeader" => (|| {
            let bytes = unhex(req["bytes"].as_str().ok_or("bytes is required")?, "bytes")?;
            read_header(&bytes)
        })(),
        "archiveSealChunk" => seal_chunk(req),
        "archiveOpenChunk" => open_chunk(req),
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::migration::migration_json;

    const KEY: &str = "0101010101010101010101010101010101010101010101010101010101010101";
    const OTHER: &str = "0202020202020202020202020202020202020202020202020202020202020202";
    const PREFIX: &str = "a1a2a3a4a5a6a7a8";

    fn ask(v: Value) -> Value {
        serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
    }

    fn kdf() -> Value {
        ask(json!({"op":"bundleKdf","method":"recoveryCode","salt":"11".repeat(32)}))["kdf"].clone()
    }

    fn header(with_kdf: bool) -> String {
        let mut req = json!({"op":"archiveStart","prefix":PREFIX});
        if with_kdf {
            req["kdf"] = kdf();
        }
        ask(req)["header"].as_str().unwrap().to_string()
    }

    /// Seal `plain` the way a driver does: cut by the chunk size, count up, flag the last.
    fn seal(plain: &[u8], key: &str, header_hex: &str) -> Vec<u8> {
        let mut file = unhex(header_hex, "h").unwrap();
        let chunks: Vec<&[u8]> = if plain.is_empty() { vec![&[][..]] } else { plain.chunks(CHUNK_BYTES).collect() };
        for (i, c) in chunks.iter().enumerate() {
            let r = ask(json!({"op":"archiveSealChunk","key":key,"header":header_hex,"counter":i,"last":i + 1 == chunks.len(),"data":hex(c)}));
            file.extend(unhex(r["sealed"].as_str().unwrap(), "s").unwrap());
        }
        file
    }

    /// Open a whole file the way a driver does, or say why not.
    fn open(file: &[u8], key: &str) -> Result<Vec<u8>, String> {
        let head = ask(json!({"op":"archiveReadHeader","bytes":hex(&file[..file.len().min(HEADER_BYTES + MAX_KDF)])}));
        if head["ok"] != true {
            return Err(head["reason"].as_str().unwrap().to_string());
        }
        let hl = head["headerLength"].as_u64().unwrap() as usize;
        let header_hex = hex(&file[..hl]);
        let body = &file[hl..];
        let full = CHUNK_BYTES + TAG_BYTES;
        let n = body.len().div_ceil(full).max(1);
        let mut out = Vec::new();
        for i in 0..n {
            let part = &body[i * full..body.len().min((i + 1) * full)];
            let r = ask(json!({"op":"archiveOpenChunk","key":key,"header":header_hex,"counter":i,"last":i + 1 == n,"sealed":hex(part)}));
            if r["ok"] != true {
                return Err(r["reason"].as_str().unwrap().to_string());
            }
            out.extend(unhex(r["data"].as_str().unwrap(), "d").unwrap());
        }
        Ok(out)
    }

    fn sample(len: usize) -> Vec<u8> {
        (0..len).map(|i| (i * 31 % 251) as u8).collect()
    }

    // @verifies SWR-MG-03
    #[test]
    fn a_recording_round_trips_at_every_size_around_the_chunk() {
        let h = header(false);
        for len in [0, 1, CHUNK_BYTES - 1, CHUNK_BYTES, CHUNK_BYTES + 1, 3 * CHUNK_BYTES, 3 * CHUNK_BYTES + 7] {
            let plain = sample(len);
            let file = seal(&plain, KEY, &h);
            assert_eq!(open(&file, KEY).unwrap(), plain, "length {len}");
        }
    }

    // @verifies SWR-MG-03
    #[test]
    fn the_header_carries_the_kdf_block_and_reads_back() {
        let file = seal(b"index", KEY, &header(true));
        let head = ask(json!({"op":"archiveReadHeader","bytes":hex(&file[..file.len().min(HEADER_BYTES + MAX_KDF)])}));
        assert_eq!(head["ok"], true);
        assert_eq!(head["kdf"], kdf());
        let plain = ask(json!({"op":"archiveReadHeader","bytes":hex(&file)}));
        assert_eq!(plain["headerLength"].as_u64().unwrap() as usize, HEADER_BYTES + kdf().to_string().len());
        let none = ask(json!({"op":"archiveReadHeader","bytes":hex(&seal(b"x", KEY, &header(false)))}));
        assert_eq!(none["kdf"], Value::Null);
    }

    // @verifies SWR-MG-03
    #[test]
    fn a_wrong_key_and_an_edited_chunk_are_not_told_apart() {
        let file = seal(&sample(2 * CHUNK_BYTES + 5), KEY, &header(false));
        assert_eq!(open(&file, OTHER).unwrap_err(), "authentication_failed");
        let mut edited = file.clone();
        let at = HEADER_BYTES + 100;
        edited[at] ^= 1;
        assert_eq!(open(&edited, KEY).unwrap_err(), "authentication_failed");
    }

    // @verifies SWR-MG-03
    #[test]
    fn a_recording_cut_short_repeated_or_reordered_does_not_open() {
        let h = header(false);
        let hl = unhex(&h, "h").unwrap().len();
        let full = CHUNK_BYTES + TAG_BYTES;
        let file = seal(&sample(3 * CHUNK_BYTES), KEY, &h);
        // Whole chunks cut off the end: the new last chunk was not sealed as the last.
        assert_eq!(open(&file[..hl + 2 * full], KEY).unwrap_err(), "authentication_failed");
        // The chunks in another order.
        let mut swapped = file[..hl].to_vec();
        swapped.extend(&file[hl + full..hl + 2 * full]);
        swapped.extend(&file[hl..hl + full]);
        swapped.extend(&file[hl + 2 * full..]);
        assert_eq!(open(&swapped, KEY).unwrap_err(), "authentication_failed");
        // A chunk repeated in place of another.
        let mut repeated = file[..hl + full].to_vec();
        repeated.extend(&file[hl..hl + full]);
        repeated.extend(&file[hl + 2 * full..]);
        assert_eq!(open(&repeated, KEY).unwrap_err(), "authentication_failed");
    }

    // @verifies SWR-MG-03
    #[test]
    fn a_header_that_is_edited_is_refused_and_so_is_a_chunk_sealed_under_another_header() {
        let file = seal(b"abc", KEY, &header(false));
        let mut edited = file.clone();
        edited[6] ^= 1; // a byte of the prefix: the nonce and the associated data both change
        assert_eq!(open(&edited, KEY).unwrap_err(), "authentication_failed");
        let mut bad_magic = file.clone();
        bad_magic[0] = b'X';
        assert_eq!(open(&bad_magic, KEY).unwrap_err(), "malformed");
        let mut newer = file.clone();
        newer[4] = VERSION + 1;
        assert_eq!(open(&newer, KEY).unwrap_err(), "newer_version");
        assert_eq!(open(&file[..HEADER_BYTES - 1], KEY).unwrap_err(), "truncated");
    }

    // @verifies SWR-MG-03
    #[test]
    fn the_core_refuses_what_it_should_not_seal() {
        let h = header(false);
        let too_long = hex(&sample(CHUNK_BYTES + 1));
        assert!(migration_json(&json!({"op":"archiveSealChunk","key":KEY,"header":h,"counter":0,"last":true,"data":too_long}).to_string()).is_err());
        assert!(migration_json(&json!({"op":"archiveSealChunk","key":KEY,"header":h,"counter":1u64 << 32,"last":true,"data":""}).to_string()).is_err());
        assert!(migration_json(&json!({"op":"archiveSealChunk","key":"00","header":h,"counter":0,"last":true,"data":""}).to_string()).is_err());
        // A kdf block the bundle key would refuse (a cheaper derivation) is not written into a header.
        let mut weak = kdf();
        weak["memoryKiB"] = json!(8);
        assert!(migration_json(&json!({"op":"archiveStart","prefix":PREFIX,"kdf":weak}).to_string()).is_err());
        assert!(migration_json(&json!({"op":"archiveStart","prefix":"00"}).to_string()).is_err());
        let info = ask(json!({"op":"archiveInfo"}));
        assert_eq!(info["chunkBytes"], CHUNK_BYTES);
    }
}
