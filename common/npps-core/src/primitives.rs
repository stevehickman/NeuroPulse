//! SHA-2 digests and Ed25519 for every platform (`OI-SOUPKT-01`, issue #569 package 6w).
//!
//! These lived in Kotlin: a hand-written SHA-256 and SHA-512 (with a bignum that derived the round constants), and a
//! port of TweetNaCl's Ed25519 that was not constant-time. The project rule is that cryptographic work is Rust
//! (`NP-SOUP-KOTLIN-001` §2a), so they are the `sha2` and `ed25519-dalek` crates here, reached by every platform
//! through the same `migration_json` entry as the rest of the core. That also gives iOS and the browser a verifier,
//! which they did not have (a signed study descriptor was refused there because nothing could check it).
//!
//! | `op` | request | answer |
//! |------|---------|--------|
//! | `sha256`, `sha512` | `bytes` (hex) | `{digest}` (hex) |
//! | `ed25519PublicKey` | `seed` (32 bytes, hex) | `{publicKey}` |
//! | `ed25519Sign` | `seed`, `message` (hex) | `{signature}` (64 bytes) |
//! | `ed25519Verify` | `publicKey` (32 bytes), `message`, `signature` (hex) | `{ok}`; a wrong length or a key that is not a point is `ok: false`, never an error |
//!
//! Verification is strict (`verify_strict`): it rejects a signature that is not in canonical form and a key of small
//! order, which the JDK's verifier accepts.

use crate::migration::{hex, unhex};
use ed25519_dalek::{Signature, Signer, SigningKey, VerifyingKey};
use serde_json::{json, Value};
use sha2::{Digest, Sha256, Sha512};
use zeroize::Zeroizing;

type R<T> = Result<T, String>;

fn field(req: &Value, key: &str) -> R<Vec<u8>> {
    unhex(req[key].as_str().ok_or_else(|| format!("{key} is required"))?, key)
}

fn seed_key(req: &Value) -> R<SigningKey> {
    let seed = Zeroizing::new(field(req, "seed")?);
    let bytes: [u8; 32] = seed.as_slice().try_into().map_err(|_| "an Ed25519 seed is 32 bytes".to_string())?;
    Ok(SigningKey::from_bytes(&bytes))
}

fn verify(req: &Value) -> R<bool> {
    let (key, message, signature) = (field(req, "publicKey")?, field(req, "message")?, field(req, "signature")?);
    let (Ok(key), Ok(signature)) = (<[u8; 32]>::try_from(key.as_slice()), <[u8; 64]>::try_from(signature.as_slice())) else {
        return Ok(false);
    };
    let Ok(key) = VerifyingKey::from_bytes(&key) else { return Ok(false) };
    Ok(key.verify_strict(&message, &Signature::from_bytes(&signature)).is_ok())
}

/// The operations of `migration_json` that are digests and signatures.
pub(crate) fn op(name: &str, req: &Value) -> Option<R<Value>> {
    Some(match name {
        "sha256" => field(req, "bytes").map(|b| json!({ "digest": hex(&Sha256::digest(&b)) })),
        "sha512" => field(req, "bytes").map(|b| json!({ "digest": hex(&Sha512::digest(&b)) })),
        "ed25519PublicKey" => seed_key(req).map(|k| json!({ "publicKey": hex(k.verifying_key().as_bytes()) })),
        "ed25519Sign" => (|| Ok(json!({ "signature": hex(&seed_key(req)?.sign(&field(req, "message")?).to_bytes()) })))(),
        "ed25519Verify" => verify(req).map(|ok| json!({ "ok": ok })),
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::migration::migration_json;

    fn ask(v: Value) -> Value {
        serde_json::from_str(&migration_json(&v.to_string()).unwrap()).unwrap()
    }

    // @verifies-none a primitive of the core, checked against published vectors here and against the JDK from Kotlin (`Ed25519JdkParityTests`); no requirement row names it yet.
    #[test]
    fn the_digests_match_the_published_vectors() {
        let d = |op: &str, bytes: &[u8]| ask(json!({ "op": op, "bytes": hex(bytes) }))["digest"].as_str().unwrap().to_string();
        assert_eq!(d("sha256", b""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        assert_eq!(d("sha256", b"abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        assert_eq!(
            d("sha512", b"abc"),
            "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"
        );
    }

    // @verifies-none a primitive of the core, checked against RFC 8032 section 7.1 here and against the JDK from Kotlin; no requirement row names it yet.
    #[test]
    fn ed25519_matches_rfc_8032_test_1_and_refuses_what_it_should() {
        let seed = "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60";
        let public = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
        let sig = "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b";
        assert_eq!(ask(json!({ "op": "ed25519PublicKey", "seed": seed }))["publicKey"], public);
        assert_eq!(ask(json!({ "op": "ed25519Sign", "seed": seed, "message": "" }))["signature"], sig);
        let v = |public: &str, message: &str, sig: &str| ask(json!({ "op": "ed25519Verify", "publicKey": public, "message": message, "signature": sig }))["ok"].clone();
        assert_eq!(v(public, "", sig), true);
        assert_eq!(v(public, "00", sig), false, "another message");
        assert_eq!(v(&public.replace('d', "e"), "", sig), false, "another key");
        let mut bad = sig.to_string();
        bad.replace_range(0..1, "f");
        assert_eq!(v(public, "", &bad), false, "an altered signature");
        // Wrong lengths and a key that is not a point are a plain refusal, not an error.
        assert_eq!(v("00", "", sig), false);
        assert_eq!(v(public, "", "00"), false);
        assert_eq!(v(&"ff".repeat(32), "", sig), false);
        assert!(migration_json(&json!({ "op": "ed25519Sign", "seed": "00", "message": "" }).to_string()).is_err());
    }
}
