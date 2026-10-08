//! JNI entry points for `life.neurone.desktop.BtleNative` (the desktop Bluetooth LE central).
//! Marshalling only: a handle, commands in, encoded events out. See `central`.

mod central;

use central::{Central, Command};
use jni::objects::{JByteArray, JClass, JString};
use jni::sys::{jbyteArray, jint, jlong};
use jni::JNIEnv;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::time::Duration;
use uuid::Uuid;

fn uuid(env: &mut JNIEnv, s: &JString) -> Option<Uuid> {
    let text: String = env.get_string(s).ok()?.into();
    Uuid::parse_str(&text).ok()
}

fn text(env: &mut JNIEnv, s: &JString) -> Option<String> {
    env.get_string(s).ok().map(Into::into)
}

/// Runs `f` against the handle; a null handle or a panic does nothing.
fn with<R>(handle: jlong, f: impl FnOnce(&Central) -> R) -> Option<R> {
    if handle == 0 {
        return None;
    }
    // SAFETY: the Kotlin owner stops its poller, then calls nativeFree exactly once.
    let central = unsafe { &*(handle as *const Central) };
    catch_unwind(AssertUnwindSafe(|| f(central))).ok()
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeCreate<'l>(
    _env: JNIEnv<'l>,
    _class: JClass<'l>,
) -> jlong {
    match catch_unwind(Central::start) {
        Ok(Ok(central)) => Box::into_raw(Box::new(central)) as jlong,
        _ => 0,
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeFree<'l>(
    _env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
) {
    if handle != 0 {
        // SAFETY: handle came from nativeCreate and is freed once.
        let central = unsafe { Box::from_raw(handle as *mut Central) };
        central.shutdown();
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativePoll<'l>(
    env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
    timeout_ms: jint,
) -> jbyteArray {
    let event = with(handle, |c| c.poll(Duration::from_millis(timeout_ms.max(0) as u64))).flatten();
    match event {
        Some(e) => env.byte_array_from_slice(&e.encode()).map(|a| a.into_raw()).unwrap_or(std::ptr::null_mut()),
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeRefresh<'l>(
    _env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
) {
    with(handle, |c| c.send(Command::Refresh));
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeStartScan<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
    service: JString<'l>,
) {
    if let Some(u) = uuid(&mut env, &service) {
        with(handle, |c| c.send(Command::StartScan(u)));
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeStopScan<'l>(
    _env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
) {
    with(handle, |c| c.send(Command::StopScan));
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeConnect<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
    device: JString<'l>,
) {
    if let Some(id) = text(&mut env, &device) {
        with(handle, |c| c.send(Command::Connect(id)));
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeDisconnect<'l>(
    _env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
) {
    with(handle, |c| c.send(Command::Disconnect));
}

/// `wanted` is the characteristic UUIDs joined by commas.
#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeDiscover<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
    service: JString<'l>,
    wanted: JString<'l>,
) {
    let (Some(service), Some(list)) = (uuid(&mut env, &service), text(&mut env, &wanted)) else { return };
    let wanted: Vec<Uuid> = list.split(',').filter_map(|s| Uuid::parse_str(s.trim()).ok()).collect();
    with(handle, |c| c.send(Command::Discover { service, wanted }));
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeEnableNotifications<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
    characteristic: JString<'l>,
) {
    if let Some(u) = uuid(&mut env, &characteristic) {
        with(handle, |c| c.send(Command::EnableNotifications(u)));
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeRead<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
    characteristic: JString<'l>,
) {
    if let Some(u) = uuid(&mut env, &characteristic) {
        with(handle, |c| c.send(Command::Read(u)));
    }
}

#[no_mangle]
pub extern "system" fn Java_life_neurone_desktop_BtleNative_nativeWrite<'l>(
    mut env: JNIEnv<'l>,
    _class: JClass<'l>,
    handle: jlong,
    characteristic: JString<'l>,
    value: JByteArray<'l>,
) {
    let Some(u) = uuid(&mut env, &characteristic) else { return };
    let Ok(bytes) = env.convert_byte_array(&value) else { return };
    with(handle, |c| c.send(Command::Write(u, bytes)));
}
