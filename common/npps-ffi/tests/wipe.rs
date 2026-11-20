//! What this library releases is zeros (`SWR-MG-28` (a), (g)). Freed memory cannot be read back safely, so these tests
//! do not: the library's allocator (`neurone_npps_core::wipe_alloc`) shows an observer each block at the moment it is
//! handed back, while it is still valid. A test binary can have one global allocator and the library's is it, so the
//! tests watch through that observer and not through an allocator of their own.

use neurone_npps_core::wipe_alloc::observe_releases;
use neurone_npps_ffi::{npps_alloc, npps_free, npps_migration_json};
use std::ptr;
use std::sync::atomic::{AtomicU8, AtomicUsize, Ordering};
use std::sync::Mutex;

static WATCH_PTR: AtomicUsize = AtomicUsize::new(0);
/// 0: the watched block has not been released; 1: it was all zeros when it was; 2: it was not.
static SEEN: AtomicU8 = AtomicU8::new(0);
/// While counting: the blocks released, and those that were not all zeros.
static COUNTING: AtomicUsize = AtomicUsize::new(0);
static RELEASED: AtomicUsize = AtomicUsize::new(0);
static NOT_ZERO: AtomicUsize = AtomicUsize::new(0);

fn observer(block: *const u8, len: usize) {
    let zero = unsafe { std::slice::from_raw_parts(block, len) }.iter().all(|b| *b == 0);
    if block as usize == WATCH_PTR.load(Ordering::SeqCst) && block as usize != 0 {
        SEEN.store(if zero { 1 } else { 2 }, Ordering::SeqCst);
        WATCH_PTR.store(0, Ordering::SeqCst);
    }
    if COUNTING.load(Ordering::SeqCst) != 0 {
        RELEASED.fetch_add(1, Ordering::SeqCst);
        if !zero {
            NOT_ZERO.fetch_add(1, Ordering::SeqCst);
        }
    }
}

/// One watch at a time: the tests of this file run on several threads.
static ONE: Mutex<()> = Mutex::new(());

fn one() -> std::sync::MutexGuard<'static, ()> {
    let guard = ONE.lock().unwrap_or_else(|e| e.into_inner());
    observe_releases(Some(observer));
    guard
}

/// What the block at `ptr` held when `release` handed it back: true if every byte was zero.
fn wiped_at_release(ptr: *mut u8, release: impl FnOnce()) -> bool {
    SEEN.store(0, Ordering::SeqCst);
    WATCH_PTR.store(ptr as usize, Ordering::SeqCst);
    release();
    match SEEN.load(Ordering::SeqCst) {
        1 => true,
        2 => false,
        _ => panic!("the block was never released"),
    }
}

// @verifies SWR-MG-28#g
#[test]
fn a_block_released_as_it_is_with_no_wipe_by_its_owner_is_zeros_when_the_allocator_takes_it_back() {
    let _one = one();
    // Nothing here wipes: no `npps_free`, no `Zeroizing`. This is the copy nobody named.
    let block = Box::into_raw(vec![0xA5u8; 64].into_boxed_slice()) as *mut u8;
    assert!(wiped_at_release(block, || unsafe { drop(Box::from_raw(std::slice::from_raw_parts_mut(block, 64))) }));
    // A buffer that grows leaves zeros where it was.
    let mut grows = vec![0x5Au8; 48];
    let was = grows.as_mut_ptr();
    assert!(wiped_at_release(was, || grows.reserve(1 << 16)));
    assert!(grows.iter().all(|b| *b == 0x5A), "and its contents moved with it");
}

// @verifies SWR-MG-28#g
#[test]
fn every_block_released_while_a_request_with_a_passphrase_is_answered_is_zeros() {
    let _one = one();
    let request = br#"{"op":"passphraseSecret","passphrase":"correct horse battery staple"}"#;
    let (mut out, mut len) = (ptr::null_mut(), 0usize);
    RELEASED.store(0, Ordering::SeqCst);
    NOT_ZERO.store(0, Ordering::SeqCst);
    COUNTING.store(1, Ordering::SeqCst);
    let code = unsafe { npps_migration_json(request.as_ptr(), request.len(), &mut out, &mut len) };
    unsafe { npps_free(out, len) };
    COUNTING.store(0, Ordering::SeqCst);
    assert_eq!(code, 0);
    // The parsed request alone is several blocks (the object, its keys, the passphrase as a string of its own).
    assert!(RELEASED.load(Ordering::SeqCst) >= 4, "only {} blocks were seen", RELEASED.load(Ordering::SeqCst));
    assert_eq!(NOT_ZERO.load(Ordering::SeqCst), 0);
}

// @verifies SWR-MG-28#a
#[test]
fn an_answer_that_carries_a_key_is_zero_when_npps_free_releases_it() {
    let _one = one();
    // `deriveUhdrKey` answers with a key, as hex in JSON: the kind of buffer the finding is about.
    let request = format!(r#"{{"op":"deriveUhdrKey","seed":"{}","salt":"{}"}}"#, "07".repeat(32), "09".repeat(32));
    let (mut out, mut len) = (ptr::null_mut(), 0usize);
    let code = unsafe { npps_migration_json(request.as_ptr(), request.len(), &mut out, &mut len) };
    assert_eq!(code, 0);
    let answer = unsafe { std::slice::from_raw_parts(out, len) }.to_vec();
    assert!(String::from_utf8(answer).unwrap().contains(r#""key":""#), "the answer holds a key");
    assert!(wiped_at_release(out, || unsafe { npps_free(out, len) }));
}

// @verifies SWR-MG-28#a
#[test]
fn an_input_buffer_from_npps_alloc_is_zero_when_npps_free_releases_it() {
    let _one = one();
    let passphrase = b"correct horse battery staple";
    let buffer = unsafe { npps_alloc(passphrase.len()) };
    unsafe { ptr::copy_nonoverlapping(passphrase.as_ptr(), buffer, passphrase.len()) };
    assert!(wiped_at_release(buffer, || unsafe { npps_free(buffer, passphrase.len()) }));
    // Null is still accepted and releases nothing.
    unsafe { npps_free(ptr::null_mut(), 0) };
}
