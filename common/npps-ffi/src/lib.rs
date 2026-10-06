//! C ABI of the shared NPPS core. See `include/neurone_npps.h` for the contract. Every function
//! does marshalling and nothing else; what an input means is decided in `neurone_npps_core::api`.

use neurone_npps_core::api;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::slice;

const OK: i32 = 0;
const REFUSED: i32 = 1;
const INTERNAL: i32 = 2;
const BAD_ARGUMENT: i32 = 3;

/// Hand `bytes` to the caller as a leaked boxed slice; `npps_free` takes it back.
unsafe fn give(bytes: Vec<u8>, out: *mut *mut u8, out_len: *mut usize) {
    let boxed = bytes.into_boxed_slice();
    *out_len = boxed.len();
    *out = Box::into_raw(boxed) as *mut u8;
}

unsafe fn call(
    src: *const u8,
    src_len: usize,
    out: *mut *mut u8,
    out_len: *mut usize,
    f: impl FnOnce(&str) -> Result<Vec<u8>, String>,
) -> i32 {
    if src.is_null() || out.is_null() || out_len.is_null() {
        return BAD_ARGUMENT;
    }
    *out = std::ptr::null_mut();
    *out_len = 0;
    let Ok(text) = std::str::from_utf8(slice::from_raw_parts(src, src_len)) else { return BAD_ARGUMENT };
    // A panic must not unwind across the FFI boundary.
    match catch_unwind(AssertUnwindSafe(|| f(text))) {
        Ok(Ok(bytes)) => {
            give(bytes, out, out_len);
            OK
        }
        Ok(Err(msg)) => {
            give(msg.into_bytes(), out, out_len);
            REFUSED
        }
        Err(_) => {
            give(b"the NPPS core panicked".to_vec(), out, out_len);
            INTERNAL
        }
    }
}

/// # Safety
/// `src` points to `src_len` readable bytes; `out` and `out_len` are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn npps_parse_json(src: *const u8, src_len: usize, out: *mut *mut u8, out_len: *mut usize) -> i32 {
    call(src, src_len, out, out_len, |t| api::parse_json(t).map(String::into_bytes))
}

/// # Safety
/// As `npps_parse_json`.
#[no_mangle]
pub unsafe extern "C" fn npps_compile_json(req: *const u8, req_len: usize, out: *mut *mut u8, out_len: *mut usize) -> i32 {
    call(req, req_len, out, out_len, api::compile_json)
}

/// # Safety
/// `ptr` and `len` are exactly what a call returned through `out` and `out_len`, released once.
#[no_mangle]
pub unsafe extern "C" fn npps_free(ptr: *mut u8, len: usize) {
    if !ptr.is_null() {
        drop(Box::from_raw(slice::from_raw_parts_mut(ptr, len)));
    }
}

/// Allocate `len` writable bytes the caller fills with an input and later releases with
/// `npps_free(ptr, len)`. A host whose memory this library cannot address (WebAssembly) has no
/// other way to hand it a buffer. Never null; a zero length yields a valid, unreadable pointer.
///
/// # Safety
/// The returned bytes are uninitialised until the caller writes them, and must be freed with the
/// same `len`.
#[no_mangle]
pub unsafe extern "C" fn npps_alloc(len: usize) -> *mut u8 {
    Box::into_raw(vec![0u8; len].into_boxed_slice()) as *mut u8
}
