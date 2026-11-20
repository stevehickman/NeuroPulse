//! C ABI of the shared NPPS core. See `include/neurone_npps.h` for the contract. Every function
//! does marshalling and nothing else; what an input means is decided in `neurone_npps_core::api`.

use neurone_npps_core::api;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::slice;
use zeroize::Zeroize;

use neurone_npps_core::constants::status::{BAD_ARGUMENT, INTERNAL_ERROR, OK, REFUSED};

/// Every block this library releases is overwritten with zeros first (`SWR-MG-28` (g)): the parsed copy of a request, a
/// buffer that grew, an intermediate result. See `neurone_npps_core::wipe_alloc` for why and for what it does not reach.
#[global_allocator]
static ALLOCATOR: neurone_npps_core::wipe_alloc::Wiping<std::alloc::System> = neurone_npps_core::wipe_alloc::Wiping(std::alloc::System);

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

/// `npps_migration_json` for an operation that takes or returns a key (`SWR-MG-32`): the request's secrets travel
/// beside it in `secrets` (each a 4-byte big-endian length, then the bytes; the n-th is `{"$secret": n}` in the JSON), and
/// the answer's secrets come back beside the answer in `secret_out`, framed the same way. Neither is ever text. The caller
/// overwrites `secrets` when the call returns (it is the caller's buffer), copies what it needs from `secret_out` into a
/// buffer it will clear, and releases `out` and `secret_out` with `npps_free`, which wipes them. On a refusal `out` holds
/// the message and `secret_out` is empty.
///
/// # Safety
/// As `npps_parse_json`; `secrets` points to `secrets_len` readable bytes (it may be null if `secrets_len` is 0), and
/// `secret_out` and `secret_out_len` are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn npps_migration_secret_json(
    req: *const u8,
    req_len: usize,
    secrets: *const u8,
    secrets_len: usize,
    out: *mut *mut u8,
    out_len: *mut usize,
    secret_out: *mut *mut u8,
    secret_out_len: *mut usize,
) -> i32 {
    if secret_out.is_null() || secret_out_len.is_null() || (secrets.is_null() && secrets_len != 0) {
        return BAD_ARGUMENT;
    }
    *secret_out = std::ptr::null_mut();
    *secret_out_len = 0;
    let secrets = if secrets_len == 0 { &[][..] } else { slice::from_raw_parts(secrets, secrets_len) };
    call(req, req_len, out, out_len, |t| {
        let (answer, beside) = api::migration_secret_json(t, secrets)?;
        if !beside.is_empty() {
            // An exact copy for the caller; the core's own buffer is wiped when `beside` is dropped.
            give(beside.to_vec(), secret_out, secret_out_len);
        }
        Ok(answer.into_bytes())
    })
}

/// Release a buffer, wiped first (`SWR-MG-28` (a)). An answer of `npps_migration_json` can hold a key, a recovery secret or
/// plaintext UHDR items, and an input allocated with `npps_alloc` can hold a passphrase; the allocator does not clear what
/// it takes back, and WebAssembly linear memory is never returned to the system, so a buffer dropped as it is stays
/// readable for the life of the process or the page. Every buffer is wiped, not only a secret one: this function cannot
/// tell which is which, and a wipe that depended on the content would say which calls carried a secret.
///
/// # Safety
/// `ptr` and `len` are exactly what a call returned through `out` and `out_len`, released once.
#[no_mangle]
pub unsafe extern "C" fn npps_free(ptr: *mut u8, len: usize) {
    if !ptr.is_null() {
        let mut buffer = Box::from_raw(slice::from_raw_parts_mut(ptr, len));
        buffer.zeroize();
        drop(buffer);
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
