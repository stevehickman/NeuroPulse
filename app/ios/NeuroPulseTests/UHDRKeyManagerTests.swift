import XCTest
import LocalAuthentication
import NeuroPulseShared
@testable import NeuroPulse

// MARK: - Mock biometric context

final class MockBiometricContext: BiometricEvaluating, @unchecked Sendable {
    enum Behavior {
        case success
        case cannotEvaluate(NSError)
        case evaluateThrows(Error)
    }
    var behavior: Behavior

    init(behavior: Behavior) { self.behavior = behavior }

    func canEvaluatePolicy(_ policy: LAPolicy, error: NSErrorPointer) -> Bool {
        if case .cannotEvaluate(let e) = behavior {
            error?.pointee = e
            return false
        }
        return true
    }

    func evaluatePolicy(_ policy: LAPolicy, localizedReason: String) async throws -> Bool {
        switch behavior {
        case .success:               return true
        case .cannotEvaluate:        return false
        case .evaluateThrows(let e): throw e
        }
    }
}

// MARK: - Mock credential store

final class MockCredentialStore: CredentialStore, @unchecked Sendable {
    var credential: Data
    var shouldThrow: Bool
    private(set) var didDelete = false

    init(credential: Data = Data(repeating: 0xBE, count: 32), shouldThrow: Bool = false) {
        self.credential = credential
        self.shouldThrow = shouldThrow
    }

    func loadOrCreateCredential(authenticatedContext: LAContext?) throws -> Data {
        if shouldThrow { throw UHDRKeyError.derivationFailed }
        return credential
    }

    func deleteCredential() { didDelete = true }
}

// MARK: - UHDRKeyManagerTests

final class UHDRKeyManagerTests: XCTestCase {

    // MARK: testKeyStoredInKeychain (corrected: key must NOT be persisted)

    @MainActor
    func testKeyStoredInKeychain() {
        let manager = UHDRKeyManager()

        XCTAssertNil(manager.activeKey)
        XCTAssertFalse(manager.isAuthenticated)

        let suspectDefaultsKeys = ["uhdr.key", "uhdr_key", "np.uhdr.key", "UHDRKey"]
        for k in suspectDefaultsKeys {
            XCTAssertNil(UserDefaults.standard.data(forKey: k),
                         "UHDR key must never be written to UserDefaults (key: \(k)).")
        }

        XCTAssertFalse(keychainHasUHDRKeyItem(),
                       "UHDR key must never be persisted to the Keychain (memory-only contract).")
    }

    // MARK: testNoFallbackOnBioFailure

    func testNoFallbackOnBioFailure() async {
        // canEvaluatePolicy returns false → must throw UHDRKeyError.biometricFailed, no key
        let cannotEvaluate = MockBiometricContext(
            behavior: .cannotEvaluate(NSError(domain: LAErrorDomain,
                                               code: LAError.biometryNotAvailable.rawValue))
        )
        let manager1 = await MainActor.run {
            UHDRKeyManager(biometricContext: cannotEvaluate,
                           credentialStore: MockCredentialStore())
        }
        do {
            try await manager1.authenticate()
            XCTFail("canEvaluatePolicy=false must throw.")
        } catch let e as UHDRKeyError {
            if case .biometricFailed = e { /* expected */ }
            else { XCTFail("Expected .biometricFailed, got \(e)") }
        } catch {
            XCTFail("Expected UHDRKeyError, got \(error)")
        }
        let key1 = await MainActor.run { manager1.activeKey }
        XCTAssertNil(key1, "No key must be derived when canEvaluatePolicy is false.")

        // evaluatePolicy throws → must propagate, no key
        let evalFails = MockBiometricContext(
            behavior: .evaluateThrows(LAError(.authenticationFailed))
        )
        let manager2 = await MainActor.run {
            UHDRKeyManager(biometricContext: evalFails,
                           credentialStore: MockCredentialStore())
        }
        do {
            try await manager2.authenticate()
            XCTFail("evaluatePolicy throwing must propagate.")
        } catch { /* expected — any error is acceptable */ }
        let key2 = await MainActor.run { manager2.activeKey }
        XCTAssertNil(key2, "No key must be derived when evaluatePolicy throws.")
    }

    // MARK: testSuccessfulAuthDerivesKey

    func testSuccessfulAuthDerivesKey() async throws {
        let store = MockCredentialStore()
        let manager = await MainActor.run {
            UHDRKeyManager(biometricContext: MockBiometricContext(behavior: .success),
                           credentialStore: store)
        }
        try await manager.authenticate()
        let (key, authenticated) = await MainActor.run { (manager.activeKey, manager.isAuthenticated) }
        XCTAssertNotNil(key, "Key must be derived after successful auth.")
        XCTAssertTrue(authenticated)
        XCTAssertEqual(key?.k1.count, 32, "K1 must be 32 bytes.")
        XCTAssertEqual(key?.k2.count, 32, "K2 must be 32 bytes.")
        // K1 and K2 must differ (both all-zeros would indicate a broken derivation)
        XCTAssertNotEqual(key?.k1, key?.k2, "K1 and K2 should differ.")
    }

    // MARK: testLockClearsKey

    func testLockClearsKey() async throws {
        let manager = await MainActor.run {
            UHDRKeyManager(biometricContext: MockBiometricContext(behavior: .success),
                           credentialStore: MockCredentialStore())
        }
        try await manager.authenticate()
        let hasKey = await MainActor.run { manager.activeKey != nil }
        XCTAssertTrue(hasKey)
        await MainActor.run { manager.lock() }
        let keyAfterLock = await MainActor.run { manager.activeKey }
        XCTAssertNil(keyAfterLock, "lock() must clear activeKey.")
        let authAfterLock = await MainActor.run { manager.isAuthenticated }
        XCTAssertFalse(authAfterLock, "lock() must clear isAuthenticated.")
    }

    // MARK: testArgon2idDeterministic (uses test-only variant with small memory)

    func testArgon2idDeterministic() throws {
        let password = Data("test-credential-32-bytes-padded!".utf8) // 32 bytes
        let salt     = Data("test-salt-1234567890123456789012".utf8) // 32 bytes
        let out1 = try npArgon2idDeriveKeyTestOnly(
            password: password, salt: salt, memoryKib: 64, iterations: 1, outputLength: 64)
        let out2 = try npArgon2idDeriveKeyTestOnly(
            password: password, salt: salt, memoryKib: 64, iterations: 1, outputLength: 64)
        XCTAssertEqual(out1, out2, "Argon2id must be deterministic for equal inputs.")
        XCTAssertEqual(out1.count, 64)
        // Different password → different output
        let differentPassword = Data("different-credential-32-bytes!!!".utf8)
        let out3 = try npArgon2idDeriveKeyTestOnly(
            password: differentPassword, salt: salt, memoryKib: 64, iterations: 1, outputLength: 64)
        XCTAssertNotEqual(out1, out3, "Different passwords must produce different keys.")
    }

    // MARK: testKeyNeverTransmitted (static analysis of production source)

    func testKeyNeverTransmitted() throws {
        let source = try Self.uhdrKeyManagerSource()

        let forbiddenNetworkingSymbols = [
            "import Network",
            "URLSession",
            "URLRequest",
            "NSURLConnection",
            "CFSocket",
            "Socket(",
            "dataTask",
            "uploadTask",
            "downloadTask"
        ]
        for symbol in forbiddenNetworkingSymbols {
            XCTAssertFalse(
                source.contains(symbol),
                "UHDRKeyManager.swift must not reference networking symbol '\(symbol)'."
            )
        }
        XCTAssertFalse(source.contains("kSecClassKey"),
                       "UHDRKeyManager.swift must not persist the derived key as kSecClassKey.")
        XCTAssertFalse(source.contains("UserDefaults"),
                       "UHDRKeyManager.swift must not write key material to UserDefaults.")
        // Privacy: no vendor-linkable device identifiers in key derivation path.
        XCTAssertFalse(source.contains("identifierForVendor"),
                       "UHDRKeyManager.swift must not use identifierForVendor as a salt source.")
        XCTAssertFalse(source.contains("UIDevice"),
                       "UHDRKeyManager.swift must not reference UIDevice.")
        // Security: no constant fallback salt — must throw on CSPRNG failure.
        XCTAssertFalse(source.contains("0xAB"),
                       "UHDRKeyManager.swift must not use a hardcoded constant as a fallback salt.")
    }

    // MARK: testCredentialStoreFailurePropagates

    func testCredentialStoreFailurePropagates() async {
        let store = MockCredentialStore(shouldThrow: true)
        let manager = await MainActor.run {
            UHDRKeyManager(biometricContext: MockBiometricContext(behavior: .success),
                           credentialStore: store)
        }
        do {
            try await manager.authenticate()
            XCTFail("Credential store failure must propagate.")
        } catch { /* expected */ }
        let key = await MainActor.run { manager.activeKey }
        XCTAssertNil(key, "No key must be derived when credential store throws.")
    }

    // MARK: - Helpers

    private func keychainHasUHDRKeyItem() -> Bool {
        let query: [String: Any] = [
            kSecClass as String:       kSecClassGenericPassword,
            kSecAttrService as String: "com.neuropulse.uhdr.key",
            kSecReturnData as String:  true,
            kSecMatchLimit as String:  kSecMatchLimitOne
        ]
        var item: CFTypeRef?
        return SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess
    }

    private static func uhdrKeyManagerSource(file: StaticString = #filePath) throws -> String {
        let testFileURL = URL(fileURLWithPath: "\(file)")
        let iosDir = testFileURL
            .deletingLastPathComponent()
            .deletingLastPathComponent()
        let sourceURL = iosDir
            .appendingPathComponent("NeuroPulse")
            .appendingPathComponent("Data")
            .appendingPathComponent("UHDRKeyManager.swift")
        return try String(contentsOf: sourceURL, encoding: .utf8)
    }
}
