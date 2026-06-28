import Foundation
import LocalAuthentication
import Security
import NeuroPulseShared

// UHDR AES-256-XTS key derivation.
//
// Key scheme (NP-FW-EMMC-002 Rev A §C):
//   1. Biometric/PIN credential (WKMD): 32-byte CSPRNG seed in Keychain,
//      protected by SecAccessControl(.userPresence). Retrieved only after
//      successful biometric/PIN evaluation via LAContext.
//   2. Salt: 32-byte CSPRNG value stored in Keychain (non-synchronizable, device-bound).
//      Production path: hub-provisioned TRNG salt from eMMC Config partition via BLE.
//   3. KDF: Argon2id (memory=65536 KiB, iterations=4, parallelism=1, output=64B)
//      → K1 (first 32B) + K2 (last 32B) for AES-256-XTS.
//
// Key is NEVER persisted to Keychain or NeuroPulse servers.
// NeuroPulse cannot decrypt UHDR (CLAUDE.md §5.1).

// MARK: - Protocol seams (injectable for testing)

protocol BiometricEvaluating: Sendable {
    func canEvaluatePolicy(_ policy: LAPolicy, error: NSErrorPointer) -> Bool
    func evaluatePolicy(_ policy: LAPolicy, localizedReason: String) async throws -> Bool
}

extension LAContext: BiometricEvaluating {}

protocol CredentialStore: Sendable {
    // Returns 32-byte CSPRNG seed; creates and stores on first call.
    // authenticatedContext: the LAContext that was just evaluated, so the Keychain
    // doesn't trigger a second biometric prompt. Pass nil in tests.
    func loadOrCreateCredential(authenticatedContext: LAContext?) throws -> Data
    func deleteCredential()
}

// MARK: - Errors

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

// MARK: - UHDRKey

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
        // bzero is guaranteed not to be optimized away (unlike memset/initializeMemory).
        k1.withUnsafeMutableBytes { buf in if let p = buf.baseAddress { bzero(p, buf.count) } }
        k2.withUnsafeMutableBytes { buf in if let p = buf.baseAddress { bzero(p, buf.count) } }
    }
}

// MARK: - Keychain credential store (production)

private struct KeychainCredentialStore: CredentialStore {
    private static let service = "com.neuropulse.uhdr.biometric-seed"
    private static let account = "credential"

    func loadOrCreateCredential(authenticatedContext: LAContext?) throws -> Data {
        var readQuery: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:        Self.service,
            kSecAttrAccount:        Self.account,
            kSecMatchLimit:         kSecMatchLimitOne,
            kSecReturnData:         true,
            kSecAttrSynchronizable: false,
        ]
        if let ctx = authenticatedContext {
            readQuery[kSecUseAuthenticationContext] = ctx
        }
        var item: CFTypeRef?
        if SecItemCopyMatching(readQuery as CFDictionary, &item) == errSecSuccess,
           let data = item as? Data, data.count == 32 {
            return data
        }

        // Create a new 32-byte CSPRNG seed and store it in Keychain.
        var bytes = [UInt8](repeating: 0, count: 32)
        guard SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes) == errSecSuccess else {
            throw UHDRKeyError.derivationFailed
        }
        let seed = Data(bytes)

        // Protect with .userPresence: requires biometric (or passcode fallback).
        // This ties the seed to the Secure Enclave's access-control policy.
        guard let access = SecAccessControlCreateWithFlags(
            nil,
            kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
            .userPresence,
            nil
        ) else {
            throw UHDRKeyError.derivationFailed
        }

        var addQuery: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:        Self.service,
            kSecAttrAccount:        Self.account,
            kSecValueData:          seed,
            kSecAttrAccessControl:  access,
            kSecAttrSynchronizable: false,
        ]
        if let ctx = authenticatedContext {
            addQuery[kSecUseAuthenticationContext] = ctx
        }

        let status = SecItemAdd(addQuery as CFDictionary, nil)
        // errSecDuplicateItem means another call beat us to it — retry read.
        if status == errSecDuplicateItem {
            if SecItemCopyMatching(readQuery as CFDictionary, &item) == errSecSuccess,
               let data = item as? Data, data.count == 32 {
                return data
            }
        }
        guard status == errSecSuccess || status == errSecDuplicateItem else {
            throw UHDRKeyError.derivationFailed
        }
        return seed
    }

    func deleteCredential() {
        let query: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:        Self.service,
            kSecAttrAccount:        Self.account,
            kSecAttrSynchronizable: false,
        ]
        SecItemDelete(query as CFDictionary)
    }
}

// MARK: - UHDRKeyManager

@MainActor
final class UHDRKeyManager: ObservableObject {

    @Published private(set) var isAuthenticated = false
    private(set) var activeKey: UHDRKey?

    private let biometricEvaluator: any BiometricEvaluating
    private let credentialStore: any CredentialStore

    // Production init — uses real LAContext and Keychain.
    init() {
        self.biometricEvaluator = LAContext()
        self.credentialStore    = KeychainCredentialStore()
    }

    // Testable init — inject mock seams.
    init(biometricContext: some BiometricEvaluating, credentialStore: some CredentialStore) {
        self.biometricEvaluator = biometricContext
        self.credentialStore    = credentialStore
    }

    // MARK: - Authentication

    func authenticate(reason: String = "Unlock your health data") async throws {
        var policyError: NSError?
        // .deviceOwnerAuthentication: biometric preferred, passcode fallback.
        // Ensures Parkinson's / post-stroke / no-biometric users can access UHDR via PIN.
        guard biometricEvaluator.canEvaluatePolicy(.deviceOwnerAuthentication,
                                                    error: &policyError) else {
            throw UHDRKeyError.biometricFailed(
                policyError ?? NSError(domain: "LAError", code: -1)
            )
        }

        do {
            _ = try await biometricEvaluator.evaluatePolicy(
                .deviceOwnerAuthentication, localizedReason: reason)
        } catch let laError as LAError where laError.code == .userCancel {
            throw UHDRKeyError.userCancelled
        } catch {
            throw UHDRKeyError.biometricFailed(error)
        }

        // Retrieve or create biometric seed. Pass the authenticated LAContext so
        // the Keychain doesn't trigger a second biometric prompt in production.
        let authenticatedContext = biometricEvaluator as? LAContext
        var credential = try credentialStore.loadOrCreateCredential(
            authenticatedContext: authenticatedContext)
        defer { credential.withUnsafeMutableBytes { buf in if let p = buf.baseAddress { bzero(p, buf.count) } } }

        let salt = try saltFromKeychain()
        let key = try deriveKey(credential: credential, salt: salt)
        activeKey = key
        isAuthenticated = true
    }

    func lock() {
        activeKey = nil
        isAuthenticated = false
    }

    // MARK: - Key derivation (Argon2id)

    private func deriveKey(credential: Data, salt: Data) throws -> UHDRKey {
        var keyMaterial = try npArgon2idDeriveKey(
            password: credential, salt: salt, outputLength: 64)
        defer { keyMaterial.withUnsafeMutableBytes { buf in if let p = buf.baseAddress { bzero(p, buf.count) } } }
        // Explicit Data copies before keyMaterial is zeroed by the defer block.
        return UHDRKey(k1: Data(keyMaterial[0..<32]), k2: Data(keyMaterial[32..<64]))
    }

    // MARK: - Salt

    // Salt: 32-byte CSPRNG value stored in Keychain (non-synchronizable, device-bound).
    // Production path: hub-provisioned TRNG salt from eMMC Config partition via BLE (OI-BLE-01, pending).
    // No linkable device identifiers (IDFV removed — salt entropy and stability are strictly better
    // with a stored random value than with SHA256 of a vendor-linkable UUID).
    private static let saltKeychainTag = "com.neuropulse.uhdr.salt"

    private func saltFromKeychain() throws -> Data {
        let query: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:        Self.saltKeychainTag,
            kSecAttrAccount:        "salt",
            kSecMatchLimit:         kSecMatchLimitOne,
            kSecReturnData:         true,
            kSecAttrSynchronizable: false,
        ]
        var item: CFTypeRef?
        if SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess,
           let saltData = item as? Data, saltData.count == 32 {
            return saltData
        }
        var bytes = [UInt8](repeating: 0, count: 32)
        guard SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes) == errSecSuccess else {
            throw UHDRKeyError.saltReadFailed
        }
        let saltData = Data(bytes)
        let addQuery: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:        Self.saltKeychainTag,
            kSecAttrAccount:        "salt",
            kSecValueData:          saltData,
            kSecAttrAccessible:     kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
            kSecAttrSynchronizable: false,
        ]
        SecItemAdd(addQuery as CFDictionary, nil)
        return saltData
    }
}
