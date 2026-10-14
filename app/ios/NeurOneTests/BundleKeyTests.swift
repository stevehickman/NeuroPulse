//
//  BundleKeyTests.swift
//  NeurOneTests
//
//  The user-held bundle key through the C ABI binding (SWR-MG-02). The Rust tests hold the rules (the code's checksum, the
//  passphrase's normalisation, the pinned cost); this holds the marshalling and the Argon2 seam. The same cases run on
//  Kotlin.
//
//  Subject under test: BundleKeyDerivation (app/ios/NeurOne/Session/BundleKey.swift)

import XCTest
@testable import NeurOne

private struct CountingRandom: BundleRandomSource {
    func nextBytes(_ count: Int) throws -> Data { Data((0..<count).map { UInt8($0 % 251) }) }
}

private struct ArgonCall {
    let memory: Int
    let iterations: Int
    let lanes: Int
    let out: Int
    let saltCount: Int
}

private final class RecordingArgon2 {
    var calls: [ArgonCall] = []
    lazy var fn: BundleArgon2 = { [unowned self] password, salt, memory, iterations, lanes, out in
        self.calls.append(ArgonCall(memory: memory, iterations: iterations, lanes: lanes, out: out, saltCount: salt.count))
        return Data((0..<out).map { UInt8(($0 * 7 + password.count) % 256) })
    }
}

final class BundleKeyTests: XCTestCase {

    // @verifies SWR-MG-02
    func testARecoveryCodeShownToTheUserDerivesTheSameKeyWhenTypedBack() throws {
        let made = try BundleKeyDerivation.newRecoveryCode(random: CountingRandom())
        let kdf = try BundleKeyDerivation.newKdf(method: .recoveryCode, random: CountingRandom())
        let argon = RecordingArgon2()
        let first = try BundleKeyDerivation.deriveKey(secret: made.secret, kdf: kdf, argon2: argon.fn).hex
        guard case .ok(let typed) = try BundleKeyDerivation.recoveryCodeSecret(made.text.lowercased().replacingOccurrences(of: "-", with: " ")) else {
            return XCTFail("a code typed back must parse")
        }
        XCTAssertEqual(first, try BundleKeyDerivation.deriveKey(secret: typed, kdf: kdf, argon2: argon.fn).hex)
    }

    // @verifies SWR-MG-02
    func testTheCostParametersAreTheCoresAndNeverTheHeaders() throws {
        let made = try BundleKeyDerivation.newRecoveryCode(random: CountingRandom())
        var kdf = try BundleKeyDerivation.newKdf(method: .passphrase, random: CountingRandom())
        let argon = RecordingArgon2()
        _ = try BundleKeyDerivation.deriveKey(secret: made.secret, kdf: kdf, argon2: argon.fn)
        XCTAssertEqual(argon.calls.count, 1)
        XCTAssertEqual(argon.calls[0].memory, 65_536)
        XCTAssertEqual(argon.calls[0].iterations, 4)
        XCTAssertEqual(argon.calls[0].lanes, 1)
        XCTAssertEqual(argon.calls[0].out, 32)
        XCTAssertEqual(argon.calls[0].saltCount, 32)
        kdf["memoryKiB"] = 8
        XCTAssertThrowsError(try BundleKeyDerivation.deriveKey(secret: made.secret, kdf: kdf, argon2: argon.fn))
        XCTAssertEqual(argon.calls.count, 1, "a refused block never reaches Argon2")
        // The production Argon2 itself refuses any cost but the bridge's.
        XCTAssertThrowsError(try BundleKeyDerivation.productionArgon2(Data([1]), Data(count: 32), 8, 1, 1, 32))
    }

    // @verifies SWR-MG-02
    func testAPassphraseIsRefusedWhenShortAndNormalisedWhenNot() throws {
        guard case .rejected(let reason, let minChars) = try BundleKeyDerivation.passphraseSecret("too short") else {
            return XCTFail("a short passphrase must be refused")
        }
        XCTAssertEqual(reason, "too_short")
        XCTAssertGreaterThan(minChars ?? 0, 0)
        let kdf = try BundleKeyDerivation.newKdf(method: .passphrase, random: CountingRandom())
        guard case .ok(let a) = try BundleKeyDerivation.passphraseSecret("caf\u{e9} au lait noir"),
              case .ok(let b) = try BundleKeyDerivation.passphraseSecret("cafe\u{301} au lait noir") else { return XCTFail() }
        let argon = RecordingArgon2()
        XCTAssertEqual(try BundleKeyDerivation.deriveKey(secret: a, kdf: kdf, argon2: argon.fn).hex,
                       try BundleKeyDerivation.deriveKey(secret: b, kdf: kdf, argon2: argon.fn).hex)
    }

    // @verifies SWR-MG-02
    func testAMistypedCodeIsATypingErrorAndNotAWrongKey() throws {
        let made = try BundleKeyDerivation.newRecoveryCode(random: CountingRandom())
        let head = made.text.prefix(1) == "2" ? "3" : "2"
        let flipped = head + made.text.dropFirst()
        guard case .rejected(let reason, _) = try BundleKeyDerivation.recoveryCodeSecret(flipped) else {
            return XCTFail("a mistyped code must be refused")
        }
        XCTAssertEqual(reason, "bad_checksum")
    }

    // @verifies SWR-MG-02
    func testTheDerivedKeySealsAndOpensABundleAndAnotherSecretCannotOpenIt() throws {
        let made = try BundleKeyDerivation.newRecoveryCode(random: CountingRandom())
        let kdf = try BundleKeyDerivation.newKdf(method: .recoveryCode, random: CountingRandom())
        let argon = RecordingArgon2()
        let key = try BundleKeyDerivation.deriveKey(secret: made.secret, kdf: kdf, argon2: argon.fn)
        let sealed = try NppsCore.migration([
            "op": "sealBundle", "kind": "uhdr", "key": key.hex, "nonce": String(repeating: "a0", count: 12),
            "kdf": kdf, "items": [String: Any](),
        ])
        let header = try NppsCore.migration(["op": "readBundleHeader", "bundle": sealed["bundle"] as Any])
        XCTAssertEqual(header["method"] as? String, "recoveryCode")
        let opened = try NppsCore.migration(["op": "openBundle", "bundle": sealed["bundle"] as Any, "key": key.hex])
        XCTAssertEqual(opened["ok"] as? Bool, true)
        guard case .ok(let other) = try BundleKeyDerivation.passphraseSecret("a different passphrase") else { return XCTFail() }
        let otherKey = try BundleKeyDerivation.deriveKey(secret: other, kdf: kdf, argon2: argon.fn)
        XCTAssertNotEqual(otherKey.hex, key.hex)
        let denied = try NppsCore.migration(["op": "openBundle", "bundle": sealed["bundle"] as Any, "key": otherKey.hex])
        XCTAssertEqual(denied["reason"] as? String, "authentication_failed")
    }
}
