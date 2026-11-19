//! The JNI library releases zeros (`SWR-MG-28` (g)). Its entry points need a JVM, which a Rust test does not have, so
//! this checks the one thing the entry points rely on: that this library's allocator is the wiping one. What the
//! allocator does with a request is tested where a request can be made (`common/npps-ffi/tests/wipe.rs`).
//! In this directory, not inside `lib.rs`, so that the requirements trace reads its tag.

use neurone_npps_core::wipe_alloc::observe_releases;
use std::sync::atomic::{AtomicU8, AtomicUsize, Ordering};

static WATCH_PTR: AtomicUsize = AtomicUsize::new(0);
/// 0: not released; 1: all zeros when it was; 2: not.
static SEEN: AtomicU8 = AtomicU8::new(0);

fn observer(block: *const u8, len: usize) {
    if block as usize == WATCH_PTR.load(Ordering::SeqCst) && block as usize != 0 {
        let zero = unsafe { std::slice::from_raw_parts(block, len) }.iter().all(|b| *b == 0);
        SEEN.store(if zero { 1 } else { 2 }, Ordering::SeqCst);
        WATCH_PTR.store(0, Ordering::SeqCst);
    }
}

// @verifies SWR-MG-28#g
#[test]
fn a_string_this_library_drops_as_it_is_is_zeros_when_it_is_released() {
    observe_releases(Some(observer));
    // What `read()` makes of a `JString`: a Rust `String` nobody wipes by name.
    let request = String::from(r#"{"op":"passphraseSecret","passphrase":"correct horse battery staple"}"#);
    WATCH_PTR.store(request.as_ptr() as usize, Ordering::SeqCst);
    drop(request);
    observe_releases(None);
    assert_eq!(SEEN.load(Ordering::SeqCst), 1, "0: the block was never released; 2: it was released with the request in it");
}
