//! A recording is sealed under a key of its own (`SWR-MG-31`; the principal, 2026-11-19; `OI-PRIV-12` (5)).
//!
//! The construction is checked from outside: these tests make the file key themselves from the published recipe
//! (`HKDF-SHA-256(key = backup key, salt = the header's salt, info = "np.archive.file-key" ‖ header)`) and open what the
//! core sealed with plain AES-256-GCM, so a change to the recipe is a failure here and not a silent change of format. The
//! file key is part of format version 2, with the object ID of `SWR-MG-29` (a): the two were added before anything was
//! released, so they share a version.

use aes_gcm::aead::{Aead, KeyInit, Payload};
use aes_gcm::{Aes256Gcm, Nonce};
use hkdf::Hkdf;
use neurone_npps_core::api::migration_json;
use serde_json::{json, Value};
use sha2::Sha256;

const BACKUP_KEY: [u8; 32] = [0x42; 32];
/// The object every chunk here is sealed as (`SWR-MG-29` (a)): it is in the associated data and not in the file.
const ID: &str = "recording:session_7.edf";

/// A chunk's associated data: the header, the object ID with its length, then whether it is the last chunk.
fn aad_of(header: &[u8], last: bool) -> Vec<u8> {
    let mut aad = header.to_vec();
    aad.extend_from_slice(&(ID.len() as u16).to_be_bytes());
    aad.extend_from_slice(ID.as_bytes());
    aad.push(u8::from(last));
    aad
}

fn ask(v: Value) -> Value {
    serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

fn header(salt: &[u8; 16]) -> Vec<u8> {
    unhex(ask(json!({ "op": "archiveStart", "salt": hex(salt) }))["header"].as_str().unwrap())
}

fn seal(header: &[u8], counter: u64, last: bool, data: &[u8]) -> Vec<u8> {
    let r = ask(json!({ "op": "archiveSealChunk", "key": hex(&BACKUP_KEY), "header": hex(header), "objectId": ID, "counter": counter, "last": last, "data": hex(data) }));
    unhex(r["sealed"].as_str().unwrap())
}

/// The file key, by the recipe in `archive.rs`'s module text.
fn file_key(header: &[u8]) -> [u8; 32] {
    let mut key = [0u8; 32];
    let mut info = b"np.archive.file-key".to_vec();
    info.extend_from_slice(header);
    Hkdf::<Sha256>::new(Some(&header[5..21]), &BACKUP_KEY).expand(&info, &mut key).unwrap();
    key
}

fn nonce(counter: u32) -> [u8; 12] {
    let mut n = [0u8; 12];
    n[8..].copy_from_slice(&counter.to_be_bytes());
    n
}

// @verifies SWR-MG-31#a
#[test]
fn a_chunk_opens_under_the_files_own_key_with_its_number_as_the_nonce_and_not_under_the_backup_key() {
    let h = header(&[0xa1; 16]);
    assert_eq!(h.len(), 23, "magic, version, a 16-byte salt and the kdf length");
    assert_eq!(&h[5..21], &[0xa1; 16]);
    for (counter, last) in [(0u32, false), (7, true)] {
        let sealed = seal(&h, counter.into(), last, b"one chunk of a recording");
        let aad = aad_of(&h, last);
        let own = Aes256Gcm::new_from_slice(&file_key(&h)).unwrap();
        let plain = own.decrypt(Nonce::from_slice(&nonce(counter)), Payload { msg: &sealed, aad: &aad }).expect("the file's own key opens it");
        assert_eq!(plain, b"one chunk of a recording");
        // The backup key itself opens nothing, with the new nonce or with the old one (the salt's first 8 bytes, then the count).
        let direct = Aes256Gcm::new_from_slice(&BACKUP_KEY).unwrap();
        let mut old_nonce = [0u8; 12];
        old_nonce[..8].copy_from_slice(&h[5..13]);
        old_nonce[8..].copy_from_slice(&counter.to_be_bytes());
        for n in [nonce(counter), old_nonce] {
            assert!(direct.decrypt(Nonce::from_slice(&n), Payload { msg: &sealed, aad: &aad }).is_err(), "the backup key sealed a chunk directly");
        }
    }
}

// @verifies SWR-MG-31#b
#[test]
fn two_files_of_one_backup_are_sealed_under_different_keys() {
    let (a, b) = (header(&[0x01; 16]), header(&[0x02; 16]));
    assert_ne!(file_key(&a), file_key(&b));
    let (sa, sb) = (seal(&a, 0, true, b"the same bytes"), seal(&b, 0, true, b"the same bytes"));
    // Same backup key, same chunk number, same plaintext: under one key and one nonce these would be equal but for the tag.
    assert_ne!(sa[..14], sb[..14], "two files produced the same keystream");
    // And neither file's key opens the other's chunk, even given the other's own associated data.
    let aad_b = aad_of(&b, true);
    let key_a = Aes256Gcm::new_from_slice(&file_key(&a)).unwrap();
    assert!(key_a.decrypt(Nonce::from_slice(&nonce(0)), Payload { msg: &sb, aad: &aad_b }).is_err());
    // Through the core: a chunk carried to another file does not open there.
    let moved = ask(json!({ "op": "archiveOpenChunk", "key": hex(&BACKUP_KEY), "header": hex(&b), "objectId": ID, "counter": 0, "last": true, "sealed": hex(&sa) }));
    assert_eq!(moved, json!({ "ok": false, "reason": "authentication_failed" }));
}

// @verifies SWR-MG-31#c
#[test]
fn a_file_sealed_the_old_way_is_not_opened_and_a_salt_of_another_length_is_refused() {
    // The form before the file key: the backup key itself, nonce = 8 random bytes then the count. Built here by hand
    // against today's header, it must not open: there is one way to seal a recording, not two.
    let h = header(&[0x33; 16]);
    let aad = aad_of(&h, true);
    let mut old_nonce = [0u8; 12];
    old_nonce[..8].copy_from_slice(&h[5..13]);
    let old = Aes256Gcm::new_from_slice(&BACKUP_KEY).unwrap().encrypt(Nonce::from_slice(&old_nonce), Payload { msg: b"old".as_slice(), aad: &aad }).unwrap();
    let r = ask(json!({ "op": "archiveOpenChunk", "key": hex(&BACKUP_KEY), "header": hex(&h), "objectId": ID, "counter": 0, "last": true, "sealed": hex(&old) }));
    assert_eq!(r, json!({ "ok": false, "reason": "authentication_failed" }));
    // A header cannot be started with the old 8-byte prefix, or with no salt.
    for bad in [json!({ "op": "archiveStart", "salt": "a1a2a3a4a5a6a7a8" }), json!({ "op": "archiveStart", "prefix": "a1a2a3a4a5a6a7a8" }), json!({ "op": "archiveStart" })] {
        assert!(migration_json(&bad.to_string()).is_err(), "{bad}");
    }
    assert_eq!(ask(json!({ "op": "archiveInfo" }))["saltBytes"], 16);
    assert!(ask(json!({ "op": "archiveInfo" })).get("prefixBytes").is_none());
}
