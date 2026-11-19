//! JNI entry points for `life.neurone.core.npps.NppsJni`. Each does marshalling and nothing
//! else; a refusal becomes a thrown `IllegalArgumentException` carrying the core's message,
//! which is what the Kotlin compiler it replaces already throws.

use jni::objects::{JByteArray, JClass, JString};
use jni::sys::{jbyteArray, jstring};
use jni::JNIEnv;
use neurone_npps_core::api;
use std::panic::{catch_unwind, AssertUnwindSafe};

/// Every block this library releases is overwritten with zeros first (`SWR-MG-28` (g)): the Rust copy of a request string
/// the JVM handed over, its parsed form, an answer before it became a Java string. See `neurone_npps_core::wipe_alloc`.
#[global_allocator]
static ALLOCATOR: neurone_npps_core::wipe_alloc::Wiping<std::alloc::System> = neurone_npps_core::wipe_alloc::Wiping(std::alloc::System);

fn throw(env: &mut JNIEnv, message: &str) {
    // A pending exception must not be overwritten.
    if !env.exception_check().unwrap_or(false) {
        let _ = env.throw_new("java/lang/IllegalArgumentException", message);
    }
}

fn read(env: &mut JNIEnv, s: &JString) -> Option<String> {
    match env.get_string(s) {
        Ok(j) => Some(j.into()),
        Err(_) => {
            throw(env, "the NPPS core could not read its string argument");
            None
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_core_npps_NppsJni_nativeParse<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    source: JString<'l>,
) -> jstring {
    let Some(text) = read(&mut env, &source) else { return std::ptr::null_mut() };
    // A panic must not unwind across the FFI boundary.
    match catch_unwind(AssertUnwindSafe(|| api::parse_json(&text))) {
        Ok(Ok(json)) => env.new_string(json).map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut()),
        Ok(Err(msg)) => {
            throw(&mut env, &msg);
            std::ptr::null_mut()
        }
        Err(_) => {
            throw(&mut env, "the NPPS core panicked while parsing");
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_core_npps_NppsJni_nativeCompile<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    request: JString<'l>,
) -> jbyteArray {
    let Some(text) = read(&mut env, &request) else { return std::ptr::null_mut() };
    match catch_unwind(AssertUnwindSafe(|| api::compile_json(&text))) {
        Ok(Ok(bytes)) => {
            let arr: Result<JByteArray, _> = env.byte_array_from_slice(&bytes);
            arr.map(|a| a.into_raw()).unwrap_or(std::ptr::null_mut())
        }
        Ok(Err(msg)) => {
            throw(&mut env, &msg);
            std::ptr::null_mut()
        }
        Err(_) => {
            throw(&mut env, "the NPPS core panicked while compiling");
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_core_npps_NppsJni_nativeContentHash<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    request: JString<'l>,
) -> jstring {
    let Some(text) = read(&mut env, &request) else { return std::ptr::null_mut() };
    match catch_unwind(AssertUnwindSafe(|| api::content_hash_json(&text))) {
        Ok(Ok(hash)) => env.new_string(hash).map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut()),
        Ok(Err(msg)) => {
            throw(&mut env, &msg);
            std::ptr::null_mut()
        }
        Err(_) => {
            throw(&mut env, "the NPPS core panicked while hashing");
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_core_npps_NppsJni_nativeNamespace<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    request: JString<'l>,
) -> jstring {
    let Some(text) = read(&mut env, &request) else { return std::ptr::null_mut() };
    match catch_unwind(AssertUnwindSafe(|| api::namespace_json(&text))) {
        Ok(Ok(json)) => env.new_string(json).map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut()),
        Ok(Err(msg)) => {
            throw(&mut env, &msg);
            std::ptr::null_mut()
        }
        Err(_) => {
            throw(&mut env, "the NPPS core panicked while building a namespace");
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_core_npps_NppsJni_nativeSerialize<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    request: JString<'l>,
) -> jstring {
    let Some(text) = read(&mut env, &request) else { return std::ptr::null_mut() };
    match catch_unwind(AssertUnwindSafe(|| api::serialize_json(&text))) {
        Ok(Ok(out)) => env.new_string(out).map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut()),
        Ok(Err(msg)) => {
            throw(&mut env, &msg);
            std::ptr::null_mut()
        }
        Err(_) => {
            throw(&mut env, "the NPPS core panicked while serializing");
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_core_npps_NppsJni_nativeValidate<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    request: JString<'l>,
) -> jstring {
    let Some(text) = read(&mut env, &request) else { return std::ptr::null_mut() };
    match catch_unwind(AssertUnwindSafe(|| api::validate_json(&text))) {
        Ok(Ok(out)) => env.new_string(out).map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut()),
        Ok(Err(msg)) => {
            throw(&mut env, &msg);
            std::ptr::null_mut()
        }
        Err(_) => {
            throw(&mut env, "the NPPS core panicked while validating");
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_core_npps_NppsJni_nativeResolveLimits<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    request: JString<'l>,
) -> jstring {
    let Some(text) = read(&mut env, &request) else { return std::ptr::null_mut() };
    match catch_unwind(AssertUnwindSafe(|| api::resolve_limits_json(&text))) {
        Ok(Ok(out)) => env.new_string(out).map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut()),
        Ok(Err(msg)) => {
            throw(&mut env, &msg);
            std::ptr::null_mut()
        }
        Err(_) => {
            throw(&mut env, "the NPPS core panicked while resolving limits");
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_core_npps_NppsJni_nativeLibrary<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    request: JString<'l>,
) -> jstring {
    let Some(text) = read(&mut env, &request) else { return std::ptr::null_mut() };
    match catch_unwind(AssertUnwindSafe(|| api::library_json(&text))) {
        Ok(Ok(out)) => env.new_string(out).map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut()),
        Ok(Err(msg)) => {
            throw(&mut env, &msg);
            std::ptr::null_mut()
        }
        Err(_) => {
            throw(&mut env, "the NPPS core panicked in the library rules");
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_core_npps_NppsJni_nativeMigration<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    request: JString<'l>,
) -> jstring {
    let Some(text) = read(&mut env, &request) else { return std::ptr::null_mut() };
    match catch_unwind(AssertUnwindSafe(|| api::migration_json(&text))) {
        Ok(Ok(out)) => env.new_string(out).map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut()),
        Ok(Err(msg)) => {
            throw(&mut env, &msg);
            std::ptr::null_mut()
        }
        Err(_) => {
            throw(&mut env, "the NPPS core panicked in the migration rules");
            std::ptr::null_mut()
        }
    }
}

/// `nativeMigration` for an operation that takes or returns a key (`SWR-MG-32`). `secrets` is read from the Java array
/// straight into a buffer that is wiped (never through a Java or a Rust string); the Kotlin side overwrites its array when
/// this returns. The answer is one byte array the Kotlin side splits and clears: a 4-byte big-endian length and the answer's
/// JSON, then the answer's secrets as the core framed them.
#[no_mangle]
pub extern "system" fn Java_life_neurone_core_npps_NppsJni_nativeMigrationSecret<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    request: JString<'l>,
    secrets: JByteArray<'l>,
) -> jbyteArray {
    let Some(text) = read(&mut env, &request) else { return std::ptr::null_mut() };
    let len = match env.get_array_length(&secrets) {
        Ok(n) => n as usize,
        Err(_) => {
            throw(&mut env, "the NPPS core could not read its secrets argument");
            return std::ptr::null_mut();
        }
    };
    let mut given = zeroize::Zeroizing::new(vec![0i8; len]);
    if env.get_byte_array_region(&secrets, 0, &mut given).is_err() {
        throw(&mut env, "the NPPS core could not read its secrets argument");
        return std::ptr::null_mut();
    }
    // SAFETY: `i8` and `u8` have the same size and alignment, and the slice is not used as `i8` while this view lives.
    let bytes = unsafe { std::slice::from_raw_parts(given.as_ptr() as *const u8, given.len()) };
    match catch_unwind(AssertUnwindSafe(|| api::migration_secret_json(&text, bytes))) {
        Ok(Ok((answer, beside))) => {
            let mut framed = zeroize::Zeroizing::new(Vec::with_capacity(4 + answer.len() + beside.len()));
            framed.extend_from_slice(&(answer.len() as u32).to_be_bytes());
            framed.extend_from_slice(answer.as_bytes());
            framed.extend_from_slice(&beside);
            env.byte_array_from_slice(&framed).map(|a| a.into_raw()).unwrap_or(std::ptr::null_mut())
        }
        Ok(Err(msg)) => {
            throw(&mut env, &msg);
            std::ptr::null_mut()
        }
        Err(_) => {
            throw(&mut env, "the NPPS core panicked in the migration rules");
            std::ptr::null_mut()
        }
    }
}

#[cfg(test)]
#[path = "tests/wipe.rs"]
mod wipe_tests;
