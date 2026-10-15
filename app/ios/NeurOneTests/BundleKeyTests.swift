//
//  BundleKeyTests.swift
//  NeurOneTests
//
//  The user-held bundle key through the C ABI binding, Argon2id included (SWR-MG-02). The Rust tests hold the rules (the
//  code's checksum, the passphrase's normalisation, the pinned cost); this holds the marshalling. The same cases run on
//  Kotlin.
//
//  Subject under test: BundleKeyDerivation (app/ios/NeurOne/Session/BundleKey.swift)

import XCTest
@testable import NeurOne

private struct CountingRandom: BundleRandomSource {
    func nextBytes(_ count: Int) throws -> Data { Data((0..<count).map { UInt8($0 % 251) }) }
}

final class BundleKeyTests: XCTestCase {

    // @verifies SWR-MG-02
    func testARecoveryCodeShownToTheUserDerivesTheSameKeyWhenTypedBack() throws {
        let made = try BundleKeyDerivation.newRecoveryCode(random: CountingRandom())
        let kdf = try BundleKeyDerivation.newKdf(method: .recoveryCode, random: CountingRandom())
        let first = try BundleKeyDerivation.deriveKey(secret: made.secret, kdf: kdf).hex
        guard case .ok(let typed) = try BundleKeyDerivation.recoveryCodeSecret(made.text.lowercased().replacingOccurrences(of: "-", with: " ")) else {
            return XCTFail("a code typed back must parse")
        }
        XCTAssertEqual(first, try BundleKeyDerivation.deriveKey(secret: typed, kdf: kdf).hex)
    }

    // @verifies SWR-MG-05
    func testAHeaderCannotAskThisPhoneForACheaperDerivation() throws {
        let made = try BundleKeyDerivation.newRecoveryCode(random: CountingRandom())
        var kdf = try BundleKeyDerivation.newKdf(method: .passphrase, random: CountingRandom())
        XCTAssertEqual(try BundleKeyDerivation.deriveKey(secret: made.secret, kdf: kdf).hex.count, 64)
        kdf["memoryKiB"] = 8
        XCTAssertThrowsError(try BundleKeyDerivation.deriveKey(secret: made.secret, kdf: kdf))
        kdf = try BundleKeyDerivation.newKdf(method: .passphrase, random: CountingRandom())
        kdf["algorithm"] = "argon2i"
        XCTAssertThrowsError(try BundleKeyDerivation.deriveKey(secret: made.secret, kdf: kdf))
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
        XCTAssertEqual(try BundleKeyDerivation.deriveKey(secret: a, kdf: kdf).hex,
                       try BundleKeyDerivation.deriveKey(secret: b, kdf: kdf).hex)
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
        let key = try BundleKeyDerivation.deriveKey(secret: made.secret, kdf: kdf)
        let sealed = try NppsCore.migration([
            "op": "sealBundle", "kind": "uhdr", "key": key.hex, "nonce": String(repeating: "a0", count: 12),
            "kdf": kdf, "items": [String: Any](),
        ])
        let header = try NppsCore.migration(["op": "readBundleHeader", "bundle": sealed["bundle"] as Any])
        XCTAssertEqual(header["method"] as? String, "recoveryCode")
        let opened = try NppsCore.migration(["op": "openBundle", "bundle": sealed["bundle"] as Any, "key": key.hex])
        XCTAssertEqual(opened["ok"] as? Bool, true)
        guard case .ok(let other) = try BundleKeyDerivation.passphraseSecret("a different passphrase") else { return XCTFail() }
        let otherKey = try BundleKeyDerivation.deriveKey(secret: other, kdf: kdf)
        XCTAssertNotEqual(otherKey.hex, key.hex)
        let denied = try NppsCore.migration(["op": "openBundle", "bundle": sealed["bundle"] as Any, "key": otherKey.hex])
        XCTAssertEqual(denied["reason"] as? String, "authentication_failed")
    }
}
