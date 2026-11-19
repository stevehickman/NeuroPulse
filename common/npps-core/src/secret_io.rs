//! How a secret crosses the core's boundary without becoming text, and the keys the core holds (`SWR-MG-32`;
//! `OI-PRIV-12` stage 2, the principal, 2026-11-19).
//!
//! Every operation of `migration_json` takes one JSON text and answers with one. A key in it was a run of hex digits, and
//! so a string on the host (a JVM, JavaScript, Swift or C# string cannot be cleared) and a string again in the parsed
//! request. Two things replace that, each where it fits:
//!
//! # A secret beside the request (`{"$secret": n}`)
//! [`migration_secret_json`] takes the JSON and, beside it, a buffer of secrets the caller owns and clears: each is a
//! 4-byte big-endian length and then the bytes, and the n-th is named in the request as `{"$secret": n}` wherever a key,
//! a seed, a passphrase or a cloud secret belongs. The core reads them straight into wiped buffers and never makes text of
//! them. A secret in the **answer** goes the same way: the JSON says `{"$secret": n}` and the bytes are in the second
//! buffer the call returns, which the binding hands over as bytes and wipes. Through this entry point no answer carries a
//! secret as hex.
//!
//! # A key the core keeps (`{"$handle": n}`)
//! A key the core itself derives need never be handed out. `deriveBundleKey` with `"keep": "bundle"` (or `"wrapping"`)
//! answers `{"key": {"$handle": n}}`, and the operations that use the key take that in place of the bytes. A key the host
//! holds (the data key from the platform keystore, a signing seed, a cloud secret) is brought in once with `keyImport` and
//! used by its handle after that, so it crosses once and not on every call. The table's rules:
//!
//! 1. **Never reused.** A handle is a count that only goes up. One that was released or never issued is refused
//!    (`unknown_key`), so a stale handle cannot reach another key. It is not a capability: code in the same process can
//!    read this memory anyway.
//! 2. **Typed.** Each key has a purpose, and an operation names the purposes it takes. A backup key cannot seal a stored
//!    value, and a data key cannot seal a bundle. The two channel keys of a direct link are kept under a purpose of their
//!    own (`link`) that no request can ask for: the link's flow makes them, names them in its state by handle, and
//!    releases them when it ends (`link.rs`).
//! 3. **No export.** No operation gives a kept key back, as bytes or as hex.
//! 4. **Bounded.** At most [`MAX_KEYS`] at once (`UC-196`); the next is refused, so a host that forgets to release cannot
//!    grow the table without end.
//! 5. **Wiped** when released (`keyRelease`), all at once (`keyReleaseAll`: on lock, and when the data key is destroyed),
//!    and whole if a panic left the table's lock poisoned.
//! 6. **Lost with the process**, and in a browser with the WebAssembly instance (the binding drops it after a trap). A
//!    caller treats `unknown_key` as "bring the key in again".
//!
//! **This is the one place the core keeps anything between calls.** Everything else in it is still a function of its
//! request.
//!
//! | operation | arguments | result |
//! |-----------|-----------|--------|
//! | `keyImport` | `key` (`{"$secret": n}`), `purpose`: `data`, `signing` or `cloud` | `{key: {"$handle": n}}` |
//! | `keyRelease` | `key` (`{"$handle": n}`) | `{released: bool}`, false if there was no such key |
//! | `keyReleaseAll` | none | `{released: count}` |
//! | `keyCount` | none | `{count, max}`: how many keys are held, for a host's own leak check |
//!
//! A key given as hex text is still read for now. Once every host uses this entry point the core refuses it (the
//! principal, 2026-11-19); until then the hex form is the old path, not a second supported one.

use crate::migration::{hex, unhex};
use serde_json::{json, Value};
use std::cell::RefCell;
use std::collections::BTreeMap;
use std::sync::Mutex;
use zeroize::Zeroizing;

type R<T> = Result<T, String>;
type Secret = Zeroizing<Vec<u8>>;

/// The most keys held at once (`UC-196`). A migration holds a handful (a data key, a wrapping key, a bundle key, a
/// signing seed, a cloud secret); nothing derives 64.
pub const MAX_KEYS: usize = 64;

/// The purposes a host may bring a key in under; the rest are only ever derived here.
const IMPORTED: [&str; 3] = ["data", "signing", "cloud"];
/// The purposes `deriveBundleKey` may keep its result under.
pub(crate) const DERIVED: [&str; 2] = ["bundle", "wrapping"];
/// Purposes only the core itself keeps a key under: a direct link's channel keys (`link.rs`), and the secret an Oblivious
/// HTTP response is opened or sealed with (`ohttp.rs`). No request can bring one in or derive one.
const INTERNAL: [&str; 2] = ["link", "ohttp"];

struct Table {
    next: u64,
    keys: BTreeMap<u64, (&'static str, Secret)>,
}

static TABLE: Mutex<Table> = Mutex::new(Table { next: 1, keys: BTreeMap::new() });

fn with_table<T>(f: impl FnOnce(&mut Table) -> T) -> T {
    let mut table = TABLE.lock().unwrap_or_else(|poisoned| {
        // A panic while the table was held: what it holds cannot be trusted. Wipe it all; every handle is then unknown.
        let mut table = poisoned.into_inner();
        table.keys.clear();
        table
    });
    f(&mut table)
}

fn purpose_name(purpose: &str) -> Option<&'static str> {
    IMPORTED.iter().chain(DERIVED.iter()).chain(INTERNAL.iter()).copied().find(|p| *p == purpose)
}

/// Keep `key` under `purpose` and answer with its handle, as the JSON a request or an answer carries.
pub(crate) fn keep(purpose: &str, key: Secret) -> R<Value> {
    let purpose = purpose_name(purpose).ok_or_else(|| format!("'{purpose}' is not a purpose a key is kept under"))?;
    with_table(|t| {
        if t.keys.len() >= MAX_KEYS {
            return Err("too_many_keys".to_string());
        }
        let handle = t.next;
        t.next += 1;
        t.keys.insert(handle, (purpose, key));
        Ok(json!({ "$handle": handle }))
    })
}

/// What one call was given beside its request, and what it gives back beside its answer.
struct Call {
    given: Vec<Secret>,
    answered: Vec<Secret>,
}

thread_local! {
    static CALL: RefCell<Option<Call>> = const { RefCell::new(None) };
}

/// Ends the call when dropped, on a refusal and on a panic alike, so the next call on this thread starts with nothing.
struct CallGuard;

impl Drop for CallGuard {
    fn drop(&mut self) {
        CALL.with(|c| c.borrow_mut().take());
    }
}

fn unframe(mut bytes: &[u8]) -> R<Vec<Secret>> {
    let mut out = vec![];
    while !bytes.is_empty() {
        if bytes.len() < 4 {
            return Err("the secrets are not framed".into());
        }
        let len = u32::from_be_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]) as usize;
        if bytes.len() - 4 < len {
            return Err("the secrets are not framed".into());
        }
        // One buffer of the final size, so no shorter copy is left behind (`SWR-MG-28` (c)).
        let mut secret = Zeroizing::new(Vec::with_capacity(len));
        secret.extend_from_slice(&bytes[4..4 + len]);
        out.push(secret);
        bytes = &bytes[4 + len..];
    }
    Ok(out)
}

fn framed(secrets: &[Secret]) -> Secret {
    let mut out = Zeroizing::new(Vec::with_capacity(secrets.iter().map(|s| 4 + s.len()).sum()));
    for s in secrets {
        out.extend_from_slice(&(s.len() as u32).to_be_bytes());
        out.extend_from_slice(s);
    }
    out
}

/// `migration_json`, with the request's secrets beside it and the answer's secrets beside the answer (the module text).
/// `secrets` and the second value returned are framed: a 4-byte big-endian length, then the bytes, for each.
pub fn migration_secret_json(request: &str, secrets: &[u8]) -> Result<(String, Zeroizing<Vec<u8>>), String> {
    let given = unframe(secrets)?;
    CALL.with(|c| *c.borrow_mut() = Some(Call { given, answered: vec![] }));
    let _end = CallGuard;
    let answer = crate::migration::migration_json(request)?;
    let answered = CALL.with(|c| c.borrow_mut().take()).map(|call| call.answered).unwrap_or_default();
    Ok((answer, framed(&answered)))
}

/// The secret a request names at `field`: bytes beside the request (`{"$secret": n}`), a key the core keeps
/// (`{"$handle": n}`, if its purpose is one of `purposes`), or, for now, hex text. `purposes` empty means no kept key
/// belongs here.
pub(crate) fn secret(req: &Value, field: &str, purposes: &[&str]) -> R<Secret> {
    match &req[field] {
        Value::String(text) => Ok(Zeroizing::new(unhex(text, field)?)),
        Value::Object(o) => {
            if let Some(n) = o.get("$secret").and_then(Value::as_u64) {
                return CALL
                    .with(|c| c.borrow().as_ref().and_then(|call| call.given.get(n as usize).map(|s| {
                        let mut copy = Zeroizing::new(Vec::with_capacity(s.len()));
                        copy.extend_from_slice(s);
                        copy
                    })))
                    .ok_or_else(|| format!("{field} names a secret this call was not given"));
            }
            if let Some(handle) = o.get("$handle").and_then(Value::as_u64) {
                return with_table(|t| match t.keys.get(&handle) {
                    None => Err("unknown_key".to_string()),
                    Some((purpose, _)) if !purposes.contains(purpose) => Err(format!("wrong_key_purpose: {field} does not take a {purpose} key")),
                    Some((_, key)) => {
                        let mut copy = Zeroizing::new(Vec::with_capacity(key.len()));
                        copy.extend_from_slice(key);
                        Ok(copy)
                    }
                });
            }
            Err(format!("{field} is not a secret"))
        }
        _ => Err(format!("{field} is required")),
    }
}

/// The kept key a request names at `field`, and nothing else: not bytes beside the request and not hex. For a place where
/// the key is one the core itself made and kept (a link's channel keys), so that it can never be text there.
pub(crate) fn kept(req: &Value, field: &str, purposes: &[&str]) -> R<Secret> {
    if req[field].get("$handle").is_none() {
        return Err(format!("{field} is not a kept key"));
    }
    secret(req, field, purposes)
}

/// As [`secret`], for a key that is used once: if the request names a kept key, it is wiped and forgotten as it is read,
/// whether or not what it is then used for succeeds.
pub(crate) fn take(req: &Value, field: &str, purposes: &[&str]) -> R<Secret> {
    let key = secret(req, field, purposes)?;
    release(&req[field]);
    Ok(key)
}

/// Wipe and forget the key `handle` names (`{"$handle": n}`), if it names one. True if there was such a key.
pub(crate) fn release(handle: &Value) -> bool {
    handle.get("$handle").and_then(Value::as_u64).is_some_and(|h| with_table(|t| t.keys.remove(&h).is_some()))
}

/// As [`secret`], for a secret that is text (a passphrase, a cloud secret): plain text in the request for now, or UTF-8
/// bytes beside it, or a kept key.
pub(crate) fn secret_text(req: &Value, field: &str, purposes: &[&str]) -> R<Zeroizing<String>> {
    if let Value::String(text) = &req[field] {
        let mut copy = Zeroizing::new(String::with_capacity(text.len()));
        copy.push_str(text);
        return Ok(copy);
    }
    let mut bytes = secret(req, field, purposes)?;
    let text = String::from_utf8(std::mem::take(&mut *bytes)).map_err(|e| {
        let _wiped = Zeroizing::new(e.into_bytes());
        format!("{field} is not UTF-8")
    })?;
    Ok(Zeroizing::new(text))
}

/// A secret for the answer: beside it as bytes when the call came through [`migration_secret_json`], hex otherwise.
pub(crate) fn answer_secret(bytes: &[u8]) -> Value {
    CALL.with(|c| match c.borrow_mut().as_mut() {
        Some(call) => {
            let mut copy = Zeroizing::new(Vec::with_capacity(bytes.len()));
            copy.extend_from_slice(bytes);
            call.answered.push(copy);
            json!({ "$secret": call.answered.len() - 1 })
        }
        None => json!(hex(bytes)),
    })
}

/// The table's own operations.
pub(crate) fn op(name: &str, req: &Value) -> Option<R<Value>> {
    Some(match name {
        "keyImport" => (|| {
            let purpose = req["purpose"].as_str().ok_or("purpose is required")?;
            if !IMPORTED.contains(&purpose) {
                return Err(format!("a host cannot bring in a '{purpose}' key"));
            }
            // Only from beside the request: a key brought in as hex text would be the string this exists to avoid.
            if req["key"].get("$secret").is_none() {
                return Err("keyImport takes its key beside the request".into());
            }
            let key = secret(req, "key", &[])?;
            if key.is_empty() {
                return Err("the key is empty".into());
            }
            Ok(json!({ "key": keep(purpose, key)? }))
        })(),
        "keyRelease" => (|| {
            if req["key"].get("$handle").and_then(Value::as_u64).is_none() {
                return Err("key is a handle".into());
            }
            Ok(json!({ "released": release(&req["key"]) }))
        })(),
        "keyReleaseAll" => Ok(json!({ "released": with_table(|t| {
            let n = t.keys.len();
            t.keys.clear();
            n
        }) })),
        "keyCount" => Ok(json!({ "count": with_table(|t| t.keys.len()), "max": MAX_KEYS })),
        _ => return None,
    })
}
