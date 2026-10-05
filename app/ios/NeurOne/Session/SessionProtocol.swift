import Foundation
import CryptoKit

// Session descriptor signing.
//
// The session descriptor is the binary blob of NP-FW-HUB-001 §4, written by
// HubDescriptorCompiler (OI-AND-WIRE-01). The hub verifies Ed25519 over its raw signed region
// and rejects unsigned or corrupted descriptors (CLAUDE.md §4.2). The JSON `NPSessionProtocol`
// and its `NPPR` frame that used to live here were a format no hub could parse, and are gone.

// MARK: - Protocol signer

struct SessionProtocolSigner {
    // The private signing key is generated once at first launch and stored in the iOS Keychain.
    // The corresponding public key is registered with the hub at pairing time.
    // Production: replace with secure enclave key where available.

    private static let keychainTag = "life.neurone.session-signing-key"

    /// Ed25519 over the descriptor's raw signed region (NP-FW-HUB-001 §4.1) — the bytes, not a digest.
    static func sign(_ region: Data) throws -> (signature: Data, fingerprint: String) {
        let signingKey = try loadOrCreateSigningKey()
        let signature = try signingKey.signature(for: region)
        let fingerprint = Data(signingKey.publicKey.rawRepresentation).prefix(8)
            .map { String(format: "%02x", $0) }.joined()
        return (Data(signature), fingerprint)
    }

    private static func loadOrCreateSigningKey() throws -> Curve25519.Signing.PrivateKey {
        // Attempt to load existing key from Keychain.
        // kSecAttrAccessible must be present in the read query so a key written by an
        // attacker with a different accessibility class cannot shadow the legitimate key
        // via SecItemCopyMatching. (NP-PRIV-ANALYSIS-002 LOW-12)
        let query: [CFString: Any] = [
            kSecClass:              kSecClassKey,
            kSecAttrApplicationTag: keychainTag.data(using: .utf8)!,
            kSecAttrAccessible:     kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
            kSecReturnData:         true
        ]
        var item: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &item)

        if status == errSecSuccess, let keyData = item as? Data {
            return try Curve25519.Signing.PrivateKey(rawRepresentation: keyData)
        }

        // Generate new key and store in Keychain
        let key = Curve25519.Signing.PrivateKey()
        let addQuery: [CFString: Any] = [
            kSecClass:                kSecClassKey,
            kSecAttrApplicationTag:   keychainTag.data(using: .utf8)!,
            kSecValueData:            key.rawRepresentation,
            kSecAttrAccessible:       kSecAttrAccessibleWhenUnlockedThisDeviceOnly
        ]
        SecItemAdd(addQuery as CFDictionary, nil)
        return key
    }

    static func publicKeyData() throws -> Data {
        let key = try loadOrCreateSigningKey()
        return key.publicKey.rawRepresentation
    }
}
