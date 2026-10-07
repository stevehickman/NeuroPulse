//! JNI entry points for `life.neurone.core.npps.NppsJni`. Each does marshalling and nothing
//! else; a refusal becomes a thrown `IllegalArgumentException` carrying the core's message,
//! which is what the Kotlin compiler it replaces already throws.

use jni::objects::{JByteArray, JClass, JString};
use jni::sys::{jbyteArray, jstring};
use jni::JNIEnv;
use neurone_npps_core::api;
use std::panic::{catch_unwind, AssertUnwindSafe};

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
