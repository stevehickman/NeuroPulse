//! The user-held key of a migration or backup bundle: what the key is made from and how a person holds it (`SWR-MG-02`;
//! issue #569 package 6d, `CLAUDE.md` §5.1, §20).
//!
//! The UHDR key on the phone is bound to the device (a Keychain or Keystore seed and a device salt), so a new phone cannot
//! derive it. A bundle that moves to a new phone is therefore sealed under a key made from something the user holds: a
//! **recovery code** the app generates and shows once, or a **passphrase** the user chooses. Which of the two a bundle was
//! sealed with is recorded in its clear header with the salt and the cost parameters (`kdf`), so a restore on a new phone
//! knows what to ask for before it has any key (`SWR-MG-05`).
//!
//! What stays on the platform is the Argon2id computation itself (each app already carries a vetted implementation, the one
//! the UHDR key uses) and the randomness (salt and recovery code, from the platform's CSPRNG). What is decided here, once for
//! every app: the cost parameters, the recovery code's text form and its checksum, the passphrase's normalisation and
//! minimum length, and what a header's `kdf` block may say. Nothing here keeps a secret: a secret is an argument to one call.

use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use unicode_normalization::UnicodeNormalization;

type R<T> = Result<T, String>;

/// Argon2id cost for a bundle key. The same memory, passes and lanes as the UHDR key (`NP-FW-EMMC-001` §6, RFC 9106), so
/// each app's one Argon2 implementation serves both; only the output length differs (32 bytes, one AES-256 key).
pub const KDF_MEMORY_KIB: u64 = 65_536;
pub const KDF_ITERATIONS: u64 = 4;
pub const KDF_PARALLELISM: u64 = 1;
pub const KDF_OUTPUT_LEN: u64 = 32;
pub const KDF_SALT_LEN: usize = 32;

/// A recovery code carries 32 random bytes (a 256-bit secret).
pub const RECOVERY_BYTES: usize = 32;

/// `PROVISIONAL`: the fewest characters (Unicode scalar values after normalisation) a passphrase may have. No derivation
/// stands behind 12 (`UC-087`). Argon2id makes each guess cost 64 MiB, which slows an attacker but does not make a short
/// passphrase safe, and the recovery code is the choice for a user who wants no such limit.
pub const PASSPHRASE_MIN_CHARS: usize = 12;

const METHODS: [&str; 2] = ["recoveryCode", "passphrase"];
const ALPHABET: &[u8; 32] = b"0123456789ABCDEFGHJKMNPQRSTVWXYZ";
/// The check is 4 base-32 characters, 20 bits of SHA-256 over a domain tag and the 32 bytes.
const CHECK_CHARS: usize = 4;
const CODE_DOMAIN: &[u8] = b"NPBK-RECOVERY-CODE-1";

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn unhex(s: &str, what: &str) -> R<Vec<u8>> {
    if s.len() % 2 != 0 || !s.is_ascii() {
        return Err(format!("{what} is not hex"));
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).map_err(|_| format!("{what} is not hex"))).collect()
}

/// The `kdf` block of a bundle header for a method and a salt. The parameters are fixed here, so no platform spells them.
pub fn kdf_block(method: &str, salt: &[u8]) -> R<Value> {
    if !METHODS.contains(&method) {
        return Err(format!("unknown key method '{method}'"));
    }
    if salt.len() != KDF_SALT_LEN {
        return Err(format!("the salt is {KDF_SALT_LEN} bytes"));
    }
    Ok(json!({
        "method": method,
        "algorithm": "argon2id",
        "salt": hex(salt),
        "memoryKiB": KDF_MEMORY_KIB,
        "iterations": KDF_ITERATIONS,
        "parallelism": KDF_PARALLELISM,
        "outputLength": KDF_OUTPUT_LEN,
    }))
}

/// Check a header's `kdf` block. A block that names another algorithm, another cost or a salt of another length is
/// `unsupported_kdf`; this build derives nothing else (a different scheme would be a new format version, `SWR-MG-05`).
/// The parameters are not read from the header into a derivation: they are compared with the fixed ones, so a bundle
/// cannot ask the restoring phone for a cheaper derivation.
pub fn check_kdf(block: &Value) -> Result<(), &'static str> {
    let method = block["method"].as_str().ok_or("malformed")?;
    let salt = block["salt"].as_str().and_then(|s| unhex(s, "salt").ok()).ok_or("malformed")?;
    let want = kdf_block(method, &salt).map_err(|_| "unsupported_kdf")?;
    if *block == want {
        Ok(())
    } else {
        Err("unsupported_kdf")
    }
}

fn check_value(bytes: &[u8]) -> u32 {
    let mut h = Sha256::new();
    h.update(CODE_DOMAIN);
    h.update(bytes);
    let d = h.finalize();
    (u32::from(d[0]) << 12) | (u32::from(d[1]) << 4) | (u32::from(d[2]) >> 4)
}

/// A recovery code as text: 32 bytes in Crockford base-32 (52 characters, the last carrying 4 zero padding bits), then 4
/// check characters, in groups of four with hyphens. The alphabet leaves out I, L, O and U, and a parse accepts the
/// look-alikes (O for 0, I and L for 1), so a code copied by eye or read aloud survives.
pub fn encode_recovery_code(bytes: &[u8]) -> R<String> {
    if bytes.len() != RECOVERY_BYTES {
        return Err(format!("a recovery code is {RECOVERY_BYTES} bytes"));
    }
    let mut chars: Vec<u8> = Vec::with_capacity(56);
    let (mut acc, mut bits) = (0u32, 0u32);
    for &b in bytes {
        acc = (acc << 8) | u32::from(b);
        bits += 8;
        while bits >= 5 {
            bits -= 5;
            chars.push(ALPHABET[((acc >> bits) & 31) as usize]);
        }
    }
    if bits > 0 {
        chars.push(ALPHABET[((acc << (5 - bits)) & 31) as usize]);
    }
    let check = check_value(bytes);
    for i in (0..CHECK_CHARS).rev() {
        chars.push(ALPHABET[((check >> (5 * i)) & 31) as usize]);
    }
    let text: Vec<String> = chars.chunks(4).map(|c| String::from_utf8_lossy(c).into_owned()).collect();
    Ok(text.join("-"))
}

/// Read a recovery code typed or pasted by a person. Case, hyphens and spaces do not matter. Returns the 32 bytes, or a
/// reason: `malformed` (wrong length or a character outside the alphabet) or `bad_checksum` (a typing error, which is not
/// the same as a wrong key and is reported separately so the screen can say so).
pub fn parse_recovery_code(text: &str) -> Result<Vec<u8>, &'static str> {
    let mut vals: Vec<u32> = Vec::with_capacity(56);
    for c in text.chars().filter(|c| !c.is_whitespace() && *c != '-') {
        let up = match c.to_ascii_uppercase() {
            'O' => '0',
            'I' | 'L' => '1',
            other => other,
        };
        let v = ALPHABET.iter().position(|a| *a as char == up).ok_or("malformed")?;
        vals.push(v as u32);
    }
    if vals.len() != 52 + CHECK_CHARS {
        return Err("malformed");
    }
    let (data, check) = vals.split_at(52);
    let (mut acc, mut bits) = (0u32, 0u32);
    let mut bytes = Vec::with_capacity(RECOVERY_BYTES);
    for &v in data {
        acc = ((acc << 5) | v) & 0x00ff_ffff;
        bits += 5;
        if bits >= 8 {
            bits -= 8;
            bytes.push(((acc >> bits) & 0xff) as u8);
        }
    }
    // 52 characters are 260 bits for 256 of data: the 4 left over must be zero, or two texts would name one code.
    if bits != 4 || acc & 0xf != 0 || bytes.len() != RECOVERY_BYTES {
        return Err("malformed");
    }
    let got = check.iter().fold(0u32, |a, v| (a << 5) | v);
    if got != check_value(&bytes) {
        return Err("bad_checksum");
    }
    Ok(bytes)
}

/// The secret a passphrase stands for: Unicode NFKC (so the same words typed on another phone's keyboard are the same
/// bytes), UTF-8. Not trimmed: a space is a character. Shorter than [`PASSPHRASE_MIN_CHARS`] is `too_short`.
pub fn passphrase_secret(passphrase: &str) -> Result<Vec<u8>, &'static str> {
    let n: String = passphrase.nfkc().collect();
    if n.chars().count() < PASSPHRASE_MIN_CHARS {
        return Err("too_short");
    }
    Ok(n.into_bytes())
}

/// The operations of `migration_json` that belong to the key.
pub fn op(name: &str, req: &Value) -> Option<R<Value>> {
    Some(match name {
        "bundleKdf" => (|| {
            let salt = unhex(req["salt"].as_str().ok_or("salt is required")?, "salt")?;
            Ok(json!({ "kdf": kdf_block(req["method"].as_str().ok_or("method is required")?, &salt)? }))
        })(),
        "encodeRecoveryCode" => (|| {
            let b = unhex(req["bytes"].as_str().ok_or("bytes is required")?, "bytes")?;
            Ok(json!({ "code": encode_recovery_code(&b)? }))
        })(),
        "parseRecoveryCode" => Ok(match parse_recovery_code(req["code"].as_str().unwrap_or("")) {
            Ok(b) => json!({ "ok": true, "secret": hex(&b) }),
            Err(reason) => json!({ "ok": false, "reason": reason }),
        }),
        "passphraseSecret" => Ok(match passphrase_secret(req["passphrase"].as_str().unwrap_or("")) {
            Ok(b) => json!({ "ok": true, "secret": hex(&b), "minChars": PASSPHRASE_MIN_CHARS }),
            Err(reason) => json!({ "ok": false, "reason": reason, "minChars": PASSPHRASE_MIN_CHARS }),
        }),
        _ => return None,
    })
}
