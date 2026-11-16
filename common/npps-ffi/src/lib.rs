//! C ABI of the shared NPPS core. See `include/neurone_npps.h` for the contract. Every function
//! does marshalling and nothing else; what an input means is decided in `neurone_npps_core::api`.

use neurone_npps_core::api;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::slice;
use zeroize::Zeroize;

use neurone_npps_core::constants::status::{BAD_ARGUMENT, INTERNAL_ERROR, OK, REFUSED};

/// Hand `bytes` to the caller as a leaked boxed slice; `npps_free` takes it back.
///
/// An answer may hold a key or plaintext UHDR (`npps_migration_json`). Shrinking a vector to its length may move it and
/// release the old block as it is, so a vector with spare capacity is copied to an exact one and cleared instead
/// (SWR-MG-28 (a)).
unsafe fn give(mut bytes: Vec<u8>, out: *mut *mut u8, out_len: *mut usize) {
    let boxed = if bytes.len() == bytes.capacity() {
        bytes.into_boxed_slice()
    } else {
        let exact: Box<[u8]> = Box::from(&bytes[..]);
        bytes.zeroize();
        exact
    };
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
            INTERNAL_ERROR
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
/// As `npps_parse_json`.
#[no_mangle]
pub unsafe extern "C" fn npps_content_hash_json(req: *const u8, req_len: usize, out: *mut *mut u8, out_len: *mut usize) -> i32 {
    call(req, req_len, out, out_len, |t| api::content_hash_json(t).map(String::into_bytes))
}

/// # Safety
/// As `npps_parse_json`.
#[no_mangle]
pub unsafe extern "C" fn npps_namespace_json(req: *const u8, req_len: usize, out: *mut *mut u8, out_len: *mut usize) -> i32 {
    call(req, req_len, out, out_len, |t| api::namespace_json(t).map(String::into_bytes))
}

/// # Safety
/// As `npps_parse_json`.
#[no_mangle]
pub unsafe extern "C" fn npps_serialize_json(req: *const u8, req_len: usize, out: *mut *mut u8, out_len: *mut usize) -> i32 {
    call(req, req_len, out, out_len, |t| api::serialize_json(t).map(String::into_bytes))
}

/// # Safety
/// As `npps_parse_json`.
#[no_mangle]
pub unsafe extern "C" fn npps_validate_json(req: *const u8, req_len: usize, out: *mut *mut u8, out_len: *mut usize) -> i32 {
    call(req, req_len, out, out_len, |t| api::validate_json(t).map(String::into_bytes))
}

/// # Safety
/// As `npps_parse_json`.
#[no_mangle]
pub unsafe extern "C" fn npps_resolve_limits_json(req: *const u8, req_len: usize, out: *mut *mut u8, out_len: *mut usize) -> i32 {
    call(req, req_len, out, out_len, |t| api::resolve_limits_json(t).map(String::into_bytes))
}

/// # Safety
/// As `npps_parse_json`.
#[no_mangle]
pub unsafe extern "C" fn npps_library_json(req: *const u8, req_len: usize, out: *mut *mut u8, out_len: *mut usize) -> i32 {
    call(req, req_len, out, out_len, |t| api::library_json(t).map(String::into_bytes))
}

/// # Safety
/// As `npps_parse_json`.
#[no_mangle]
pub unsafe extern "C" fn npps_migration_json(req: *const u8, req_len: usize, out: *mut *mut u8, out_len: *mut usize) -> i32 {
    call(req, req_len, out, out_len, |t| api::migration_json(t).map(String::into_bytes))
}

/// Release a buffer this library handed out, **after filling it with zeros** (SWR-MG-28 (a)). A request or an answer of
/// `npps_migration_json` can hold a passphrase, a recovery secret, a key or plaintext UHDR, and released memory is not
/// cleared by anyone else: in WebAssembly the linear memory never shrinks, so it would stay readable as long as the page.
///
/// # Safety
/// `ptr` and `len` are exactly what a call returned through `out` and `out_len`, released once.
#[no_mangle]
pub unsafe extern "C" fn npps_free(ptr: *mut u8, len: usize) {
    if !ptr.is_null() {
        let bytes = slice::from_raw_parts_mut(ptr, len);
        bytes.zeroize();
        drop(Box::from_raw(bytes));
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
