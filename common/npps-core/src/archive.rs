//! A recording sealed for a backup: one file, in chunks, under the backup's key (`SWR-MG-03`; `CLAUDE.md` §22).
//!
//! The backup bundle (`migration`) is one object that holds small records and is opened whole, which is right for settings and
//! wrong for a recording of tens of megabytes: it would have to be hex-coded and held in memory as one value, and rewritten
//! every night. A recording is sealed here instead, once, in chunks of [`CHUNK_BYTES`], so a platform handles one chunk at a
//! time and a recording that has been backed up is never sealed again (it does not change). The sealing is here, once, for every
//! platform (`CLAUDE.md` §22): a Kotlin, Swift or C# copy would be a second implementation of cryptography.
//!
//! # The file
//! `header ‖ chunk₀ ‖ chunk₁ ‖ … ‖ chunkₙ`, where the header is `"NPAR" ‖ version (1) ‖ salt (16 random bytes) ‖ kdf length (2,
//! big endian) ‖ kdf (JSON)`. The `kdf` is the bundle key's own block (`bundle_key`): how the key was made from the person's code or
//! passphrase, so a restore on a new phone knows what to ask for. It is present in the file that indexes the others and may be
//! empty in the rest. Every chunk is AES-256-GCM of up to [`CHUNK_BYTES`] bytes plus a 16-byte tag, **under a key of the file's own**
//! (below), with the chunk's number as the nonce and the whole header, the object ID and a final-chunk flag as the associated data
//! (`header ‖ ID length (2, big endian) ‖ ID ‖ flag`). So a chunk moved, repeated, dropped from the middle, or cut off the end
//! fails to open, and a wrong key and an edited file are not told apart (`authentication_failed`). Every chunk is full except
//! the last, which is the one the file ends with and may be empty only if the whole recording is.
//!
//! # The object ID (`SWR-MG-29` (a), `OI-PRIV-09`)
//! Every file of one backup is sealed under one key, and the place that keeps them is not trusted: it can hand back any file it
//! holds under any name. So a file is sealed as a named object, and whoever opens it says which object it expects: the ID is in
//! the associated data of every chunk and **is not in the file**. A file sealed as one recording does not open as another, or as
//! the index (`authentication_failed`, the same answer as a wrong key). The ID comes from what the opener already trusts (the
//! sealed index names each recording; the index has a fixed ID), never from the file or its name at the backup location, and it
//! is not written in the clear because a recording's name is UHDR. The core takes any ID that is not empty and fits the two-byte
//! length; what the IDs are is the driver's (`RecordingArchive`).
//!
//! # The file's own key (`SWR-MG-31`; the principal, 2026-11-19)
//! `file key = HKDF-SHA-256(key = the backup key, salt = the header's salt, info = "np.archive.file-key" ‖ the header)`, 32
//! bytes (RFC 5869). The backup key is one key for every recording of a backup, for as long as the person keeps the same
//! recovery code or passphrase, so sealing every file under it directly would rest the whole backup on no two files ever
//! being given the same nonce. Under a key per file the nonce is only a count within one file, which cannot repeat (the
//! count is refused above 2³² − 1), and two files share a key only if they draw the same 16 random bytes (about n² / 2¹²⁹
//! for n files). The header is in `info`, so the key is bound to the file it heads. The file key and the object ID do
//! different work: the key keeps two files from sharing a nonce, and the ID keeps one file from being opened as another.
//!
//! # Version
//! The version is 1. The object ID and the file's own key each changed the format and the number did not rise with either, by
//! ruling (the principal, 2026-11-19): no build that sealed a file in an earlier form was released, so no such file should
//! exist and there is no older reader to refuse this one. A file in an earlier form, were one to turn up, does not open:
//! its header is another shape and its chunks were sealed under another key and other associated data. The next change to
//! the format after a release bumps the version.
//!
//! | operation | arguments | result |
//! |-----------|-----------|--------|
//! | `archiveInfo` | none | `{chunkBytes, tagBytes, saltBytes, headerBytes}`: the fixed sizes a driver cuts a file by |
//! | `archiveStart` | `salt` (16 bytes, hex, random: a new one for every file), `kdf` (optional) | `{header}` (hex) |
//! | `archiveReadHeader` | `bytes`: the first bytes of a file (hex; at least [`HEADER_BYTES`] and as many more as the `kdf` needs, [`MAX_KDF`] at most) | `{ok: true, headerLength, kdf}` or `{ok: false, reason}`: `malformed`, `truncated` or `newer_version` |
//! | `archiveSealChunk` | `key`, `header` (hex, the whole header), `objectId`, `counter`, `last`, `data` (hex) | `{sealed}` (hex) |
//! | `archiveOpenChunk` | `key`, `header`, `objectId` (the object the caller expects), `counter`, `last`, `sealed` | `{ok: true, data}` or `{ok: false, reason: "authentication_failed"}` |

use crate::bundle_key;
use crate::migration::{hex, unhex};
use aes_gcm::aead::{Aead, KeyInit, Payload};
use aes_gcm::{Aes256Gcm, Nonce};
use hkdf::Hkdf;
use sha2::Sha256;
use serde_json::{json, Value};
use zeroize::Zeroizing;

type R<T> = Result<T, String>;

const MAGIC: &[u8; 4] = b"NPAR";
const VERSION: u8 = 1;
/// A chunk's plaintext length. A platform cuts a recording by this and the core refuses a longer one.
pub const CHUNK_BYTES: usize = 64 * 1024;
const TAG_BYTES: usize = 16;
/// The random bytes a file's own key is made with. 16 bytes: two files of one backup sharing them is about n² / 2¹²⁹.
const SALT_BYTES: usize = 16;
const FILE_KEY_INFO: &[u8] = b"np.archive.file-key";
/// Magic, version, salt and the length of the kdf: the part of the header that is always there.
pub const HEADER_BYTES: usize = 4 + 1 + SALT_BYTES + 2;
/// The longest kdf block a header may carry. The block is a few dozen bytes; anything near this is not one.
pub const MAX_KDF: usize = 1024;
const KEY_LEN: usize = 32;

/// The cipher for one file: the request's backup key and the file's `header` make the file's own key (the module text).
/// The backup key (bytes beside the request, a key the core keeps, or for now hex) and the file key are wiped when this
/// returns, and the cipher wipes its own copy when it is dropped (the `zeroize` feature of `aes-gcm`), `SWR-MG-28` (c).
fn cipher(req: &Value, header: &[u8]) -> R<Aes256Gcm> {
    let key = crate::secret_io::secret(req, "key", &["bundle"])?;
    if key.len() != KEY_LEN {
        return Err(format!("the key is {KEY_LEN} bytes"));
    }
    let mut file_key = Zeroizing::new([0u8; KEY_LEN]);
    Hkdf::<Sha256>::new(Some(&header[5..5 + SALT_BYTES]), &key)
        .expand_multi_info(&[FILE_KEY_INFO, header], &mut *file_key)
        .map_err(|_| "the file key could not be made".to_string())?;
    Aes256Gcm::new_from_slice(&*file_key).map_err(|_| "the key is not usable".to_string())
}

/// The file's header, the nonce of chunk `counter` (the count alone: the key is the file's own) and its associated data: the header, the object ID with its length, then whether it is the last
/// chunk. The length goes before the ID so no ID can be mistaken for another ID followed by a flag.
fn frame(req: &Value) -> R<(Vec<u8>, [u8; 12], Vec<u8>)> {
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
    nonce[8..].copy_from_slice(&(counter as u32).to_be_bytes());
    let id = req["objectId"].as_str().ok_or("objectId is required")?.as_bytes();
    if id.is_empty() {
        return Err("objectId is empty".into());
    }
    let id_len = u16::try_from(id.len()).map_err(|_| "objectId is too long".to_string())?;
    let mut aad = header.clone();
    aad.extend_from_slice(&id_len.to_be_bytes());
    aad.extend_from_slice(id);
    aad.push(u8::from(last));
    Ok((header, nonce, aad))
}

fn start(req: &Value) -> R<Value> {
    let salt = unhex(req["salt"].as_str().ok_or("salt is required")?, "salt")?;
    if salt.len() != SALT_BYTES {
        return Err(format!("the salt is {SALT_BYTES} bytes"));
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
    header.extend_from_slice(&salt);
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
    let kdf_len = u16::from_be_bytes([bytes[HEADER_BYTES - 2], bytes[HEADER_BYTES - 1]]) as usize;
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
    let (header, nonce, aad) = frame(req)?;
    let data = unhex(req["data"].as_str().ok_or("data is required")?, "data")?;
    if data.len() > CHUNK_BYTES {
        return Err(format!("a chunk is at most {CHUNK_BYTES} bytes"));
    }
    let ct = cipher(req, &header)?
        .encrypt(Nonce::from_slice(&nonce), Payload { msg: &data, aad: &aad })
        .map_err(|_| "encryption failed".to_string())?;
    Ok(json!({ "sealed": hex(&ct) }))
}

fn open_chunk(req: &Value) -> R<Value> {
    let (header, nonce, aad) = frame(req)?;
    let sealed = unhex(req["sealed"].as_str().ok_or("sealed is required")?, "sealed")?;
    if sealed.len() < TAG_BYTES || sealed.len() > CHUNK_BYTES + TAG_BYTES {
        return Ok(json!({ "ok": false, "reason": "authentication_failed" }));
    }
    Ok(match cipher(req, &header)?.decrypt(Nonce::from_slice(&nonce), Payload { msg: &sealed, aad: &aad }) {
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
            "saltBytes": SALT_BYTES,
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
    const SALT: &str = "a1a2a3a4a5a6a7a8a9aaabacadaeafb0";
    const ID: &str = "recording:session_7.edf";

    fn ask(v: Value) -> Value {
        serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
    }

    fn kdf() -> Value {
        ask(json!({"op":"bundleKdf","method":"recoveryCode","salt":"11".repeat(32)}))["kdf"].clone()
    }

    fn header(with_kdf: bool) -> String {
        let mut req = json!({"op":"archiveStart","salt":SALT});
        if with_kdf {
            req["kdf"] = kdf();
        }
        ask(req)["header"].as_str().unwrap().to_string()
    }

    /// Seal `plain` the way a driver does: cut by the chunk size, count up, flag the last.
    fn seal(plain: &[u8], key: &str, header_hex: &str) -> Vec<u8> {
        seal_as(plain, key, header_hex, ID)
    }

    fn seal_as(plain: &[u8], key: &str, header_hex: &str, id: &str) -> Vec<u8> {
        let mut file = unhex(header_hex, "h").unwrap();
        let chunks: Vec<&[u8]> = if plain.is_empty() { vec![&[][..]] } else { plain.chunks(CHUNK_BYTES).collect() };
        for (i, c) in chunks.iter().enumerate() {
            let r = ask(json!({"op":"archiveSealChunk","key":key,"header":header_hex,"objectId":id,"counter":i,"last":i + 1 == chunks.len(),"data":hex(c)}));
            file.extend(unhex(r["sealed"].as_str().unwrap(), "s").unwrap());
        }
        file
    }

    /// Open a whole file the way a driver does, or say why not.
    fn open(file: &[u8], key: &str) -> Result<Vec<u8>, String> {
        open_as(file, key, ID)
    }

    fn open_as(file: &[u8], key: &str, id: &str) -> Result<Vec<u8>, String> {
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
            let r = ask(json!({"op":"archiveOpenChunk","key":key,"header":header_hex,"objectId":id,"counter":i,"last":i + 1 == n,"sealed":hex(part)}));
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
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
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
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
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
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
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
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
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
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_header_that_is_edited_is_refused_and_so_is_a_chunk_sealed_under_another_header() {
        let file = seal(b"abc", KEY, &header(false));
        let mut edited = file.clone();
        edited[6] ^= 1; // a byte of the salt: the file key and the associated data both change
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
        assert!(migration_json(&json!({"op":"archiveSealChunk","key":KEY,"header":h,"objectId":ID,"counter":0,"last":true,"data":too_long}).to_string()).is_err());
        assert!(migration_json(&json!({"op":"archiveSealChunk","key":KEY,"header":h,"objectId":ID,"counter":1u64 << 32,"last":true,"data":""}).to_string()).is_err());
        assert!(migration_json(&json!({"op":"archiveSealChunk","key":"00","header":h,"objectId":ID,"counter":0,"last":true,"data":""}).to_string()).is_err());
        // A kdf block the bundle key would refuse (a cheaper derivation) is not written into a header.
        let mut weak = kdf();
        weak["memoryKiB"] = json!(8);
        assert!(migration_json(&json!({"op":"archiveStart","salt":SALT,"kdf":weak}).to_string()).is_err());
        assert!(migration_json(&json!({"op":"archiveStart","salt":"00"}).to_string()).is_err());
        let info = ask(json!({"op":"archiveInfo"}));
        assert_eq!(info["chunkBytes"], CHUNK_BYTES);
    }

    // @verifies SWR-MG-29#a
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn a_file_sealed_as_one_object_does_not_open_as_another() {
        // Two recordings and the index of one backup: one key, and (the worst case) one header.
        let h = header(false);
        let (a, b) = (sample(CHUNK_BYTES + 9), sample(40));
        let file_a = seal_as(&a, KEY, &h, "recording:session_1.edf");
        let file_b = seal_as(&b, KEY, &h, "recording:session_2.edf");
        assert_eq!(open_as(&file_a, KEY, "recording:session_1.edf").unwrap(), a);
        assert_eq!(open_as(&file_b, KEY, "recording:session_2.edf").unwrap(), b);
        // The backup location hands back the other file: it does not open, and the answer is the one a wrong key gets.
        assert_eq!(open_as(&file_b, KEY, "recording:session_1.edf").unwrap_err(), "authentication_failed");
        assert_eq!(open_as(&file_a, KEY, "recording:session_2.edf").unwrap_err(), "authentication_failed");
        assert_eq!(open_as(&file_a, KEY, "index").unwrap_err(), "authentication_failed");
        // The ID is not in the file: the two files differ only in ciphertext, and neither holds a recording's name.
        assert_eq!(file_b.len(), unhex(&h, "h").unwrap().len() + b.len() + TAG_BYTES);
        assert!(!file_a.windows(9).any(|w| w == b"session_1"));
    }

    // @verifies SWR-MG-29#a
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn an_object_id_is_required_and_its_length_is_bound_with_it() {
        let h = header(false);
        for bad in [json!(null), json!(""), json!(7), json!("x".repeat(usize::from(u16::MAX) + 1))] {
            let mut req = json!({"op":"archiveSealChunk","key":KEY,"header":h,"counter":0,"last":true,"data":""});
            if !bad.is_null() {
                req["objectId"] = bad.clone();
            }
            assert!(migration_json(&req.to_string()).is_err(), "sealed with objectId {bad}");
            req["op"] = json!("archiveOpenChunk");
            req["sealed"] = json!("00".repeat(TAG_BYTES));
            assert!(migration_json(&req.to_string()).is_err(), "opened with objectId {bad}");
        }
        // An ID is not a prefix of another: "ab" then the last flag is not "a" with a longer tail.
        let file = seal_as(b"", KEY, &h, "ab");
        assert_eq!(open_as(&file, KEY, "a").unwrap_err(), "authentication_failed");
        assert_eq!(open_as(&file, KEY, "ab\u{1}").unwrap_err(), "authentication_failed");
    }

    // @verifies SWR-MG-29#a
    #[test]
    #[cfg_attr(feature = "refuse-hex-keys", ignore = "uses a secret as hex text, the form SWR-MG-32 (h) refuses")]
    fn the_version_stays_1_and_any_other_number_is_refused() {
        // No version bump for the object ID or the file key (the principal, 2026-11-19): no file of an earlier form should exist.
        let file = seal(b"abc", KEY, &header(false));
        assert_eq!(file[4], 1);
        let mut v2 = file.clone();
        v2[4] = 2;
        assert_eq!(open(&v2, KEY).unwrap_err(), "newer_version");
        let mut v0 = file;
        v0[4] = 0;
        assert_eq!(open(&v0, KEY).unwrap_err(), "malformed");
    }
}
