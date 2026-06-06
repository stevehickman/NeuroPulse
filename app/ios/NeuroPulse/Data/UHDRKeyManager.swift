import Foundation
import CryptoKit
import LocalAuthentication
import Security
import UIKit   // UIDevice.current.identifierForVendor

// UHDR AES-256-XTS key derivation.
// Target KDF: Argon2id (memory=64MB, iterations=4, parallelism=1, output=64B → K1+K2 for XTS).
// iOS 18 does not ship Argon2id in CryptoKit or CommonCrypto. Production integration:
//   - Link swift-crypto-extras or the argon2 C library (BLAKE2 + Argon2 reference impl).
//   - Until then this module uses PBKDF2-SHA256 (rounds=200000) as a structurally identical
//     placeholder that maintains the same API, Keychain interactions, and data-partition contract.
//     When Argon2id is linked, swap derivePBKDF2 for deriveArgon2id below — no call-site changes.
//
// Key is NEVER persisted to Keychain or NeuroPulse servers.
// Key is derived fresh each session from biometric + TRNG salt in Config partition.
// NeuroPulse cannot decrypt UHDR (CLAUDE.md §5.1).

import CommonCrypto

enum UHDRKeyError: LocalizedError {
    case biometricFailed(Error)
    case saltReadFailed
    case derivationFailed
    case userCancelled

    var errorDescription: String? {
        switch self {
        case .biometricFailed(let e): return "Biometric authentication failed: \(e.localizedDescription)"
        case .saltReadFailed:         return "Could not read encryption salt from device."
        case .derivationFailed:       return "Key derivation failed."
        case .userCancelled:          return "Authentication cancelled."
        }
    }
}

// Holds the two 256-bit subkeys for AES-256-XTS (K1 = data key, K2 = tweak key).
// Kept in memory only; zeroed on deinit.
final class UHDRKey {
    private(set) var k1: Data  // 32 bytes
    private(set) var k2: Data  // 32 bytes

    init(k1: Data, k2: Data) {
        self.k1 = k1
        self.k2 = k2
    }

    deinit {
        // Zero key material on dealloc.
        k1.withUnsafeMutableBytes { ptr in ptr.baseAddress?.initializeMemory(as: UInt8.self, repeating: 0, count: ptr.count) }
        k2.withUnsafeMutableBytes { ptr in ptr.baseAddress?.initializeMemory(as: UInt8.self, repeating: 0, count: ptr.count) }
    }
}

@MainActor
final class UHDRKeyManager: ObservableObject {

    @Published private(set) var isAuthenticated = false
    private(set) var activeKey: UHDRKey?

    // Salt lives in eMMC Config partition, written at device provisioning.
    // Hub exposes it via a read-only BLE characteristic (not in scope for alpha;
    // placeholder uses a stable device-specific derivation from identifierForVendor).
    private var salt: Data {
        // Production: read 32-byte TRNG salt from hub Config partition over BLE.
        // Placeholder: derive from UIDevice.current.identifierForVendor (stable per app install).
        guard let uuid = UIDevice.current.identifierForVendor?.uuidString.data(using: .utf8) else {
            // identifierForVendor is nil only briefly after a factory wipe before
            // the first device unlock — a foreground app should not reach this path.
            // Use a Keychain-stored random value so different devices (if this path
            // is somehow reached) do not derive the same key from a constant.
            return saltFallbackFromKeychain()
        }
        return Data(SHA256.hash(data: uuid))
    }

    // Stable per-device random salt stored in the Keychain. Used only when
    // identifierForVendor is nil (see above). Replaced entirely when Argon2id
    // + hub-provisioned TRNG salt is integrated.
    private static let saltFallbackTag = "com.neuropulse.uhdr.salt-fallback"

    private func saltFallbackFromKeychain() -> Data {
        let query: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:        Self.saltFallbackTag,
            kSecAttrAccount:        "salt",
            kSecMatchLimit:         kSecMatchLimitOne,
            kSecReturnData:         true,
            kSecAttrSynchronizable: false
        ]
        var item: CFTypeRef?
        if SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess,
           let saltData = item as? Data, saltData.count == 32 {
            return saltData
        }
        var bytes = [UInt8](repeating: 0, count: 32)
        guard SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes) == errSecSuccess else {
            // CSPRNG failure is exceedingly unlikely — retain constant as last resort.
            return Data(repeating: 0xAB, count: 32)
        }
        let saltData = Data(bytes)
        let addQuery: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:        Self.saltFallbackTag,
            kSecAttrAccount:        "salt",
            kSecValueData:          saltData,
            kSecAttrAccessible:     kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
            kSecAttrSynchronizable: false
        ]
        SecItemAdd(addQuery as CFDictionary, nil)
        return saltData
    }

    // Prompt biometric, derive key, make it available for the session.
    func authenticate(reason: String = "Unlock your health data") async throws {
        let context = LAContext()
        var policyError: NSError?
        // .deviceOwnerAuthentication falls back to PIN/passcode when Face ID / Touch ID
        // is unavailable or locked out; .deviceOwnerAuthenticationWithBiometrics would
        // permanently block users who haven't enrolled biometrics.
        guard context.canEvaluatePolicy(.deviceOwnerAuthentication, error: &policyError) else {
            throw UHDRKeyError.biometricFailed(policyError ?? NSError(domain: "LAError", code: -1))
        }

        do {
            let success = try await context.evaluatePolicy(
                .deviceOwnerAuthentication, localizedReason: reason)
            // Note: evaluatePolicy never returns false on iOS — it either returns
            // true or throws. The guard below is unreachable dead code today, kept
            // as a defensive net in case the API behaviour changes in a future OS.
            // The REAL user-cancellation path is the LAError.userCancel catch below;
            // do not remove that catch thinking this guard handles it.
            guard success else { throw UHDRKeyError.userCancelled }
        } catch let laError as LAError where laError.code == .userCancel {
            throw UHDRKeyError.userCancelled
        } catch {
            throw UHDRKeyError.biometricFailed(error)
        }

        // Derive key material after successful authentication.
        // Production: replace "biometric-placeholder" with the actual biometric token
        // or PIN digest from the LAContext evaluation result.
        // PBKDF2 placeholder MUST NOT ship to production — see guards below.
        //
        // Two-layer protection:
        //   DEBUG builds   → #warning surfaces in Xcode issue navigator as a reminder
        //   Release builds → #error causes compile-time failure so this can never archive
        #if DEBUG
        #warning("UHDRKeyManager: biometric-placeholder password in use. Replace with Argon2id + real biometric/PIN credential before shipping any build outside development.")
        #else
        #error("UHDRKeyManager: PBKDF2 placeholder with hardcoded password must be replaced with Argon2id + real biometric/PIN credential before production.")
        #endif
        let key = try deriveKey(password: "biometric-placeholder", salt: salt)
        activeKey = key
        isAuthenticated = true
    }

    func lock() {
        activeKey = nil
        isAuthenticated = false
    }

    // MARK: - Key derivation (PBKDF2-SHA256 placeholder for Argon2id)

    private func deriveKey(password: String, salt: Data) throws -> UHDRKey {
        guard let passwordData = password.data(using: .utf8) else { throw UHDRKeyError.derivationFailed }
        var keyMaterial = Data(count: 64)  // 64 bytes → K1 (32B) + K2 (32B)

        let result: Int32 = keyMaterial.withUnsafeMutableBytes { keyPtr in
            salt.withUnsafeBytes { saltPtr in
                passwordData.withUnsafeBytes { pwdPtr in
                    CCKeyDerivationPBKDF(
                        CCPBKDFAlgorithm(kCCPBKDF2),
                        pwdPtr.baseAddress?.assumingMemoryBound(to: Int8.self),
                        passwordData.count,
                        saltPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        salt.count,
                        CCPseudoRandomAlgorithm(kCCPRFHmacAlgSHA256),
                        200_000,  // iterations — Argon2id target replaces this entirely
                        keyPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        64
                    )
                }
            }
        }

        guard result == kCCSuccess else { throw UHDRKeyError.derivationFailed }
        return UHDRKey(k1: keyMaterial[0..<32], k2: keyMaterial[32..<64])
    }
}
