import Foundation
import Security

// The user-held key of a migration or backup bundle (SWR-MG-02, issue #569 package 6d).
//
// The UHDR key (UHDRKeyManager) is bound to this device: a Keychain seed that never leaves it and a device salt. A bundle
// moving to a new phone cannot use it, so a bundle is sealed under a key made from something the user holds: a recovery
// code the app shows once, or a passphrase the user chooses. The rules (the cost parameters, the code's text form and
// checksum, the passphrase's normalisation and minimum length, what a header's kdf block may say) are decided once in the
// shared core (`NppsCore.migration`), the Argon2id derivation included, so there is one implementation for every app
// (the device-bound UHDR key keeps `npArgon2idDeriveKey`). This file only marshals them.
//
// The secret and the key are memory-only, like the UHDR key: never written to the Keychain, a file or a log, never
// transmitted, zeroed on deinit. NeurOne never holds either (CLAUDE.md §5.1).

enum BundleKeyMethod: String {
    case recoveryCode
    case passphrase
}

/// Randomness from the platform's CSPRNG. The core is pure and has none, so a salt or a code comes from here.
protocol BundleRandomSource {
    func nextBytes(_ count: Int) throws -> Data
}

struct SystemBundleRandomSource: BundleRandomSource {
    func nextBytes(_ count: Int) throws -> Data {
        var bytes = [UInt8](repeating: 0, count: count)
        guard SecRandomCopyBytes(kSecRandomDefault, count, &bytes) == errSecSuccess else {
            throw BundleKeyError.noRandomness
        }
        return Data(bytes)
    }
}

enum BundleKeyError: Error {
    case noRandomness
    case wrongLength
    /// The header's kdf block is not the one the core writes for its method and salt, or the Argon2 call failed.
    case unsupportedKdf
}

/// What a secret from a person (or a fresh code) became. `reason` is a code for the screen to word from the locale files.
enum BundleSecretResult {
    case ok(BundleSecret)
    /// `malformed` or `bad_checksum` (a recovery code), `too_short` (a passphrase, with `minChars`).
    case rejected(reason: String, minChars: Int?)
}

/// Memory-only secret bytes, zeroed on deinit.
final class BundleSecret {
    private var material: Data

    fileprivate init(_ bytes: Data) { material = bytes }

    fileprivate func bytes() -> Data { material }

    deinit {
        // bzero is guaranteed not to be optimized away (unlike memset/initializeMemory).
        material.withUnsafeMutableBytes { buf in if let p = buf.baseAddress { bzero(p, buf.count) } }
    }
}

/// A recovery code just made: the `text` to show the user once, and the `secret` it stands for.
struct NewRecoveryCode {
    let text: String
    let secret: BundleSecret
}

/// The derived 32-byte AES-256-GCM key. Memory-only, zeroed on deinit.
final class BundleKey {
    private var material: Data

    fileprivate init(_ bytes: Data) { material = bytes }

    /// The key as the lowercase hex the core's sealBundle and openBundle take.
    var hex: String { material.map { String(format: "%02x", $0) }.joined() }

    deinit {
        material.withUnsafeMutableBytes { buf in if let p = buf.baseAddress { bzero(p, buf.count) } }
    }
}

enum BundleKeyDerivation {
    static let keyLength = 32
    static let saltLength = 32
    static let recoveryBytes = 32

    private static func hex(_ d: Data) -> String { d.map { String(format: "%02x", $0) }.joined() }

    private static func unhex(_ s: String) -> Data? {
        guard s.count % 2 == 0 else { return nil }
        var out = Data(capacity: s.count / 2)
        var i = s.startIndex
        while i < s.endIndex {
            let j = s.index(i, offsetBy: 2)
            guard let b = UInt8(s[i..<j], radix: 16) else { return nil }
            out.append(b)
            i = j
        }
        return out
    }

    /// A fresh recovery code. Show `text` once; the bundle can be opened only with it.
    static func newRecoveryCode(random: BundleRandomSource = SystemBundleRandomSource()) throws -> NewRecoveryCode {
        let bytes = try random.nextBytes(recoveryBytes)
        guard bytes.count == recoveryBytes else { throw BundleKeyError.wrongLength }
        let out = try NppsCore.migration(["op": "encodeRecoveryCode", "bytes": hex(bytes)])
        guard let text = out["code"] as? String else { throw BundleKeyError.unsupportedKdf }
        return NewRecoveryCode(text: text, secret: BundleSecret(bytes))
    }

    /// A recovery code typed or pasted by the user.
    static func recoveryCodeSecret(_ text: String) throws -> BundleSecretResult {
        try secretResult(NppsCore.migration(["op": "parseRecoveryCode", "code": text]))
    }

    /// A passphrase the user chose (or typed to restore). Normalised and length-checked by the core.
    static func passphraseSecret(_ text: String) throws -> BundleSecretResult {
        try secretResult(NppsCore.migration(["op": "passphraseSecret", "passphrase": text]))
    }

    private static func secretResult(_ r: [String: Any]) throws -> BundleSecretResult {
        if r["ok"] as? Bool == true, let h = r["secret"] as? String, let bytes = unhex(h) {
            return .ok(BundleSecret(bytes))
        }
        return .rejected(reason: r["reason"] as? String ?? "malformed", minChars: r["minChars"] as? Int)
    }

    /// The `kdf` block for the header of a bundle about to be sealed: a fresh random salt and the core's fixed cost.
    static func newKdf(method: BundleKeyMethod, random: BundleRandomSource = SystemBundleRandomSource()) throws -> [String: Any] {
        let salt = try random.nextBytes(saltLength)
        guard salt.count == saltLength else { throw BundleKeyError.wrongLength }
        let out = try NppsCore.migration(["op": "bundleKdf", "method": method.rawValue, "salt": hex(salt)])
        guard let kdf = out["kdf"] as? [String: Any] else { throw BundleKeyError.unsupportedKdf }
        return kdf
    }

    /// The bundle key for a secret and a header's `kdf` block: Argon2id computed by the shared core. The core checks the
    /// block against the fixed cost first, so a bundle cannot ask this phone for a cheaper derivation or another algorithm.
    /// Throws `unsupportedKdf` for any block the core refuses.
    static func deriveKey(secret: BundleSecret, kdf: [String: Any]) throws -> BundleKey {
        let r = try NppsCore.migration(["op": "deriveBundleKey", "secret": hex(secret.bytes()), "kdf": kdf])
        guard r["ok"] as? Bool == true, let h = r["key"] as? String, let raw = unhex(h), raw.count == keyLength else {
            throw BundleKeyError.unsupportedKdf
        }
        return BundleKey(raw)
    }
}
