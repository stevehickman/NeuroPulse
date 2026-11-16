//! What `npps_free` leaves behind (SWR-MG-28 (a), OI-PRIV-12 stage 1).
//!
//! Reading memory after it is released is undefined, so the test does not. It installs an allocator that looks at one
//! chosen block **at the moment it is released**, while it is still valid, and records whether every byte was zero.
//! The same test fails against an `npps_free` that only drops the buffer.

use neurone_npps_core::constants::status;
use neurone_npps_ffi::{npps_alloc, npps_free, npps_migration_json};
use std::alloc::{GlobalAlloc, Layout, System};
use std::ptr;
use std::sync::atomic::{AtomicUsize, Ordering::SeqCst};
use std::sync::Mutex;

/// The block being watched (0 for none) and what was seen when it was released: 0 not released, 1 all zero, 2 not.
static WATCHED: AtomicUsize = AtomicUsize::new(0);
static SEEN: AtomicUsize = AtomicUsize::new(0);
/// One watched block at a time.
static ONE: Mutex<()> = Mutex::new(());

struct Watching;

unsafe impl GlobalAlloc for Watching {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        System.alloc(layout)
    }
    unsafe fn dealloc(&self, block: *mut u8, layout: Layout) {
        if block as usize == WATCHED.load(SeqCst) && WATCHED.swap(0, SeqCst) != 0 {
            let zero = std::slice::from_raw_parts(block, layout.size()).iter().all(|b| *b == 0);
            SEEN.store(if zero { 1 } else { 2 }, SeqCst);
        }
        System.dealloc(block, layout)
    }
}

#[global_allocator]
static ALLOCATOR: Watching = Watching;

/// Release `block` with `npps_free` and say what the allocator saw in it as it went.
fn seen_on_free(block: *mut u8, len: usize) -> usize {
    SEEN.store(0, SeqCst);
    WATCHED.store(block as usize, SeqCst);
    unsafe { npps_free(block, len) };
    SEEN.load(SeqCst)
}

fn migration(request: &str) -> (i32, *mut u8, usize) {
    let (mut out, mut len) = (ptr::null_mut(), 0usize);
    let code = unsafe { npps_migration_json(request.as_ptr(), request.len(), &mut out, &mut len) };
    (code, out, len)
}

const KEY: &str = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

// @verifies SWR-MG-28#a
#[test]
fn an_answer_that_holds_plaintext_is_zero_when_npps_free_releases_it() {
    let _one = ONE.lock().unwrap_or_else(|e| e.into_inner());
    let seal = format!(r#"{{"op":"sealValue","key":"{KEY}","nonce":"a0a1a2a3a4a5a6a7a8a9aaab","name":"np.session.history","value":"the history of the person"}}"#);
    let (code, out, len) = migration(&seal);
    assert_eq!(code, status::OK);
    let sealed: serde_json::Value = serde_json::from_slice(unsafe { std::slice::from_raw_parts(out, len) }).unwrap();
    assert_eq!(seen_on_free(out, len), 1);

    let open = format!(r#"{{"op":"openValue","key":"{KEY}","name":"np.session.history","sealed":{}}}"#, sealed["sealed"]);
    let (code, out, len) = migration(&open);
    assert_eq!(code, status::OK);
    let answer = unsafe { std::slice::from_raw_parts(out, len) };
    assert!(String::from_utf8_lossy(answer).contains("the history of the person"), "the answer under test holds the plaintext");
    assert_eq!(seen_on_free(out, len), 1, "the plaintext was still in the answer when it was released");
}

// @verifies SWR-MG-28#a
#[test]
fn a_request_buffer_from_npps_alloc_is_zero_when_npps_free_releases_it() {
    let _one = ONE.lock().unwrap_or_else(|e| e.into_inner());
    // What a WebAssembly host does: allocate, write the request (here one that holds a passphrase), call, release.
    let request = br#"{"op":"passphraseSecret","passphrase":"correct horse battery staple"}"#;
    let block = unsafe { npps_alloc(request.len()) };
    unsafe { ptr::copy_nonoverlapping(request.as_ptr(), block, request.len()) };
    let (mut out, mut len) = (ptr::null_mut(), 0usize);
    assert_eq!(unsafe { npps_migration_json(block, request.len(), &mut out, &mut len) }, status::OK);
    assert_eq!(seen_on_free(out, len), 1, "the answer holds the secret the passphrase stands for");
    assert_eq!(seen_on_free(block, request.len()), 1, "the passphrase was still in the request when it was released");
}

// @verifies SWR-MG-28#a
#[test]
fn a_refusal_is_zero_when_released_and_a_null_pointer_is_ignored() {
    let _one = ONE.lock().unwrap_or_else(|e| e.into_inner());
    let (code, out, len) = migration(r#"{"op":"sealValue","key":"00","nonce":"00","name":"n","value":"v"}"#);
    assert_eq!(code, status::REFUSED);
    assert!(len > 0);
    assert_eq!(seen_on_free(out, len), 1);
    unsafe { npps_free(ptr::null_mut(), 0) };
}
