//! `npps_free` wipes what it releases (`SWR-MG-28` (a)). Freed memory cannot be read back safely, so this test binary's
//! allocator looks at one watched block at the moment it is handed back, while it is still valid.

use neurone_npps_ffi::{npps_alloc, npps_free, npps_migration_json};
use std::alloc::{GlobalAlloc, Layout, System};
use std::ptr;
use std::sync::atomic::{AtomicU8, AtomicUsize, Ordering};
use std::sync::Mutex;

static WATCH_PTR: AtomicUsize = AtomicUsize::new(0);
static WATCH_LEN: AtomicUsize = AtomicUsize::new(0);
/// 0: the watched block has not been released; 1: it was all zeros when it was; 2: it was not.
static SEEN: AtomicU8 = AtomicU8::new(0);

struct Watching;

unsafe impl GlobalAlloc for Watching {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        System.alloc(layout)
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        if ptr as usize == WATCH_PTR.load(Ordering::SeqCst) && ptr as usize != 0 {
            let len = WATCH_LEN.load(Ordering::SeqCst);
            let zero = (0..len).all(|i| *ptr.add(i) == 0);
            SEEN.store(if zero { 1 } else { 2 }, Ordering::SeqCst);
            WATCH_PTR.store(0, Ordering::SeqCst);
        }
        System.dealloc(ptr, layout)
    }
}

#[global_allocator]
static ALLOCATOR: Watching = Watching;

/// One watch at a time: the tests of this file run on several threads.
static ONE: Mutex<()> = Mutex::new(());

/// What the block at `ptr` held when `release` handed it back: true if every byte was zero.
fn wiped_at_release(ptr: *mut u8, len: usize, release: impl FnOnce()) -> bool {
    SEEN.store(0, Ordering::SeqCst);
    WATCH_LEN.store(len, Ordering::SeqCst);
    WATCH_PTR.store(ptr as usize, Ordering::SeqCst);
    release();
    match SEEN.load(Ordering::SeqCst) {
        1 => true,
        2 => false,
        _ => panic!("the block was never released"),
    }
}

// @verifies-none a check of this file's own allocator hook: a block released without a wipe is seen as not wiped
#[test]
fn the_hook_sees_a_block_that_is_released_as_it_is() {
    let _one = ONE.lock().unwrap();
    let block = Box::into_raw(vec![0xA5u8; 64].into_boxed_slice()) as *mut u8;
    let wiped = wiped_at_release(block, 64, || unsafe { drop(Box::from_raw(std::slice::from_raw_parts_mut(block, 64))) });
    assert!(!wiped);
}

// @verifies SWR-MG-28#a
#[test]
fn an_answer_that_carries_a_key_is_zero_when_npps_free_releases_it() {
    let _one = ONE.lock().unwrap();
    // `deriveUhdrKey` answers with a key, as hex in JSON: the kind of buffer the finding is about.
    let request = format!(r#"{{"op":"deriveUhdrKey","seed":"{}","salt":"{}"}}"#, "07".repeat(32), "09".repeat(32));
    let (mut out, mut len) = (ptr::null_mut(), 0usize);
    let code = unsafe { npps_migration_json(request.as_ptr(), request.len(), &mut out, &mut len) };
    assert_eq!(code, 0);
    let answer = unsafe { std::slice::from_raw_parts(out, len) }.to_vec();
    assert!(String::from_utf8(answer).unwrap().contains(r#""key":""#), "the answer holds a key");
    assert!(wiped_at_release(out, len, || unsafe { npps_free(out, len) }));
}

// @verifies SWR-MG-28#a
#[test]
fn an_input_buffer_from_npps_alloc_is_zero_when_npps_free_releases_it() {
    let _one = ONE.lock().unwrap();
    let passphrase = b"correct horse battery staple";
    let buffer = unsafe { npps_alloc(passphrase.len()) };
    unsafe { ptr::copy_nonoverlapping(passphrase.as_ptr(), buffer, passphrase.len()) };
    assert!(wiped_at_release(buffer, passphrase.len(), || unsafe { npps_free(buffer, passphrase.len()) }));
    // Null is still accepted and releases nothing.
    unsafe { npps_free(ptr::null_mut(), 0) };
}
