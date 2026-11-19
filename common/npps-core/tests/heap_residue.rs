//! Decoded keys and secrets are cleared before the core releases them (SWR-MG-28 (c); OI-PRIV-12).
//!
//! Reading memory after it is released is undefined, so these tests do not. They install an allocator that searches
//! every block **as it is released**, while it is still valid, for a chosen run of bytes (the decoded key), and count
//! the blocks that still held it. A buffer that grows is released too (the allocator moves it by allocate, copy,
//! release), so a secret left behind by a growing buffer is counted as well.
//!
//! What this can and cannot see. It sees the heap, and only the exact bytes named: the decoded key, not the hex text
//! of it in the request, which is the caller's and is in the JSON the core parsed (OI-PRIV-12 stage 2). It does not
//! see the stack.

use neurone_npps_core::api::migration_json;
use serde_json::{json, Value};
use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicU8, AtomicUsize, Ordering::SeqCst};
use std::sync::Mutex;

const MAX: usize = 96;
static NEEDLE: [AtomicU8; MAX] = [const { AtomicU8::new(0) }; MAX];
/// The needle's length; 0 means the search is off.
static LEN: AtomicUsize = AtomicUsize::new(0);
static HITS: AtomicUsize = AtomicUsize::new(0);
/// One search at a time.
static ONE: Mutex<()> = Mutex::new(());

struct Searching;

unsafe impl GlobalAlloc for Searching {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        System.alloc(layout)
    }
    // `realloc` is left as the default (allocate, copy, release), so a buffer that grew is searched as well.
    unsafe fn dealloc(&self, block: *mut u8, layout: Layout) {
        let n = LEN.load(SeqCst);
        if n != 0 && layout.size() >= n {
            let bytes = std::slice::from_raw_parts(block, layout.size());
            if bytes.windows(n).any(|w| w.iter().zip(&NEEDLE).all(|(b, want)| *b == want.load(SeqCst))) {
                HITS.fetch_add(1, SeqCst);
            }
        }
        System.dealloc(block, layout)
    }
}

#[global_allocator]
static ALLOCATOR: Searching = Searching;

/// How many blocks were released still holding `parts` (joined) while `work` ran. The needle is kept out of the heap.
fn released_holding(parts: &[&[u8]], work: impl FnOnce()) -> usize {
    let mut n = 0;
    for b in parts.iter().flat_map(|p| p.iter()) {
        NEEDLE[n].store(*b, SeqCst);
        n += 1;
    }
    HITS.store(0, SeqCst);
    LEN.store(n, SeqCst);
    work();
    LEN.store(0, SeqCst);
    HITS.load(SeqCst)
}

fn ask(v: Value) -> Value {
    serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

/// A 32-byte key no other buffer would hold by chance. The searches look for its first [`PART`] bytes, so a partial copy
/// (the front of the key, left by a buffer that grew) is found as well as a whole one.
const RAW_KEY: [u8; 32] = [
    0xd3, 0x4c, 0x91, 0x7e, 0x02, 0xb8, 0x65, 0xfa, 0x1d, 0xa7, 0x30, 0xc9, 0x5b, 0xe4, 0x86, 0x2f, 0x79, 0x0a, 0xbd, 0x43, 0xe1, 0x58, 0x9c, 0x27, 0xf6, 0x3a, 0x84, 0xcd,
    0x12, 0x6f, 0xb0, 0x55,
];
const PART: usize = 8;
const NONCE: &str = "a0a1a2a3a4a5a6a7a8a9aaab";

// @verifies-none a check of this file's own detector: a buffer released with the bytes in it is counted, and one cleared first is not.
#[test]
fn the_detector_counts_a_block_released_with_the_bytes_in_it() {
    let _one = ONE.lock().unwrap_or_else(|e| e.into_inner());
    assert_eq!(released_holding(&[&RAW_KEY], || drop(std::hint::black_box(RAW_KEY.to_vec()))), 1);
    assert_eq!(released_holding(&[&RAW_KEY], || drop(zeroize::Zeroizing::new(std::hint::black_box(RAW_KEY.to_vec())))), 0);
    // A buffer that grows leaves its earlier copy behind, and that is counted too.
    let grown = released_holding(&[&RAW_KEY], || {
        let mut v = zeroize::Zeroizing::new(std::hint::black_box(RAW_KEY.to_vec()));
        v.reserve(4096);
    });
    assert_eq!(grown, 1);
}

// @verifies SWR-MG-28#c
#[test]
fn the_data_key_is_cleared_after_a_value_is_sealed_and_after_it_is_opened() {
    let _one = ONE.lock().unwrap_or_else(|e| e.into_inner());
    let key = hex(&RAW_KEY);
    let mut sealed = Value::Null;
    let n = released_holding(&[&RAW_KEY[..PART]], || {
        sealed = ask(json!({ "op": "sealValue", "key": key, "nonce": NONCE, "name": "np.session.history", "value": "history" }));
    });
    assert_eq!(n, 0, "sealValue released the decoded key uncleared");
    let n = released_holding(&[&RAW_KEY[..PART]], || {
        let opened = ask(json!({ "op": "openValue", "key": key, "name": "np.session.history", "sealed": sealed["sealed"] }));
        assert_eq!(opened["value"], "history");
    });
    assert_eq!(n, 0, "openValue released the decoded key uncleared");
}

// @verifies SWR-MG-28#c
#[test]
fn the_backup_key_is_cleared_after_a_bundle_and_a_recording_chunk_are_sealed_and_opened() {
    let _one = ONE.lock().unwrap_or_else(|e| e.into_inner());
    let key = hex(&RAW_KEY);
    let kdf = ask(json!({ "op": "bundleKdf", "method": "recoveryCode", "salt": "5a".repeat(32) }))["kdf"].clone();
    let header = ask(json!({ "op": "archiveStart", "salt": "0102030405060708090a0b0c0d0e0f10" }))["header"].clone();
    let n = released_holding(&[&RAW_KEY[..PART]], || {
        let b = ask(json!({ "op": "sealBundle", "kind": "uhdr", "key": key, "nonce": NONCE, "items": {}, "kdf": kdf }));
        let opened = ask(json!({ "op": "openBundle", "bundle": b["bundle"], "key": key }));
        assert_eq!(opened["ok"], true, "{opened}");
        let c = ask(json!({ "op": "archiveSealChunk", "key": key, "header": header, "counter": 0, "last": true, "data": "00ff" }));
        let o = ask(json!({ "op": "archiveOpenChunk", "key": key, "header": header, "counter": 0, "last": true, "sealed": c["sealed"] }));
        assert_eq!(o["data"], "00ff", "{o}");
    });
    assert_eq!(n, 0, "a bundle or archive operation released the decoded key uncleared");

    // The recording's own key (`SWR-MG-29`) is made from the backup key and the header, and is as much a key: made here
    // by the same recipe, on the stack, so that every copy of it on the heap is one the core made.
    let header_bytes: Vec<u8> = (0..header.as_str().unwrap().len()).step_by(2).map(|i| u8::from_str_radix(&header.as_str().unwrap()[i..i + 2], 16).unwrap()).collect();
    let mut file_key = [0u8; 32];
    hkdf::Hkdf::<sha2::Sha256>::new(Some(&header_bytes[5..21]), &RAW_KEY).expand_multi_info(&[b"np.archive.file-key", &header_bytes], &mut file_key).unwrap();
    let n = released_holding(&[&file_key[..PART]], || {
        let c = ask(json!({ "op": "archiveSealChunk", "key": key, "header": header, "counter": 1, "last": false, "data": "ab" }));
        let o = ask(json!({ "op": "archiveOpenChunk", "key": key, "header": header, "counter": 1, "last": false, "sealed": c["sealed"] }));
        assert_eq!(o["data"], "ab", "{o}");
    });
    assert_eq!(n, 0, "an archive operation released the file's own key uncleared");
}

// @verifies SWR-MG-28#c
#[test]
fn the_secret_a_passphrase_stands_for_is_cleared() {
    let _one = ONE.lock().unwrap_or_else(|e| e.into_inner());
    // Typed in full-width letters, so the normalised secret (plain ASCII) is not the text of the request: every copy of
    // these bytes on the heap is one the core made.
    let typed = "ｃｏｒｒｅｃｔ ｈｏｒｓｅ ｂａｔｔｅｒｙ";
    let secret = b"correct horse battery";
    let n = released_holding(&[secret], || {
        let r = ask(json!({ "op": "passphraseSecret", "passphrase": typed }));
        assert_eq!(r["secret"], hex(secret), "{r}");
    });
    assert_eq!(n, 0, "passphraseSecret released the normalised passphrase uncleared");
}

// @verifies SWR-MG-28#c
#[test]
fn the_prefixed_aws_secret_is_cleared_after_a_request_is_signed() {
    let _one = ONE.lock().unwrap_or_else(|e| e.into_inner());
    let secret = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
    let n = released_holding(&[b"AWS4", secret.as_bytes()], || {
        let r = ask(json!({ "op": "awsSigV4", "method": "GET", "host": "examplebucket.s3.amazonaws.com", "path": "/test.txt", "query": [],
            "headers": {}, "payloadHash": "UNSIGNED-PAYLOAD", "region": "us-east-1", "service": "s3",
            "accessKeyId": "AKIAIOSFODNN7EXAMPLE", "secretAccessKey": secret, "amzDate": "20130524T000000Z" }));
        assert!(r["signature"].is_string(), "{r}");
    });
    assert_eq!(n, 0, "awsSigV4 released \"AWS4\" + the secret uncleared");
}

// @verifies SWR-MG-28#c
#[test]
fn a_channel_key_and_the_pairing_randomness_are_cleared_after_use() {
    let _one = ONE.lock().unwrap_or_else(|e| e.into_inner());
    // A serving phone, already paired, sending an answer: the frame is sealed under the channel key in its state.
    let state = json!({ "step": "served", "cur": 1, "buf": "", "queue": [], "pairing": { "triesLeft": 2 },
        "ctx": { "sk": hex(&RAW_KEY), "rk": "11".repeat(32), "sn": 0, "rn": 0 } });
    let n = released_holding(&[&RAW_KEY[..PART]], || {
        let a = ask(json!({ "op": "flow", "flow": "serve", "state": state, "result": { "free": 1 } }));
        assert_eq!(a["effect"]["type"], "linkSend", "{a}");
    });
    assert_eq!(n, 0, "sealing a frame released the decoded channel key uncleared");

    // The restoring phone's first step: the driver's 96 random bytes become the SPAKE2 randomness.
    let random: Vec<u8> = (0..96u8).map(|i| i.wrapping_mul(151).wrapping_add(43)).collect();
    let start = ask(json!({ "op": "flow", "flow": "restore", "params": { "link": true, "objectStore": true, "pairing": { "code": "12345678" } } }));
    assert_eq!(start["effect"], json!({ "type": "random", "bytes": 96 }), "{start}");
    let n = released_holding(&[&random[..PART]], || {
        let a = ask(json!({ "op": "flow", "flow": "restore", "state": start["state"], "result": { "bytes": hex(&random) } }));
        assert_eq!(a["effect"]["type"], "linkSend", "{a}");
    });
    assert_eq!(n, 0, "the pairing released the driver's random bytes uncleared");
}
