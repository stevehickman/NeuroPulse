//
//  NppsMigrationTests.swift
//  NeurOneTests
//
//  The migration rules through the C ABI binding (SWR-MG). The Rust tests hold the rules and the claim race; this holds
//  the layer between: the request marshalling and the refusal mapping. The same cases run on Kotlin and C#.
//
//  Subject under test: NppsCore.migration (app/ios/NeurOne/Session/NppsCore.swift)

import XCTest
@testable import NeurOne

final class NppsMigrationTests: XCTestCase {

    // @verifies SWR-MG-05
    func testANewerBundleIsRefusedWholeAndTheKindIsReadFirst() throws {
        let newer = try NppsCore.migration(["op": "readHeader", "header": ["kind": "migrate", "formatVersion": 99]])
        XCTAssertEqual(newer["ok"] as? Bool, false)
        XCTAssertEqual(newer["reason"] as? String, "newer_version")
        let ok = try NppsCore.migration(["op": "readHeader", "header": ["kind": "uhdr", "formatVersion": 1]])
        XCTAssertEqual(ok["kind"] as? String, "uhdr")
    }

    // @verifies SWR-MG-03
    func testTheNightlyKindNamesWhatItLeavesOut() throws {
        let inv = try NppsCore.migration(["op": "inventory", "kind": "uhdr"])
        let omitted = inv["omitted"] as? [String] ?? []
        XCTAssertTrue(omitted.contains("protocols") && omitted.contains("limitsProfiles"))
        XCTAssertTrue((inv["carried"] as? [String] ?? []).contains("withdrawalState"))
    }

    // @verifies SWR-MG-04
    func testAWithdrawalSurvivesARestoreAndNeverAskedStaysNull() throws {
        let merged = try NppsCore.migration([
            "op": "mergeWithdrawal", "local": [String: Any](),
            "incoming": ["blanketConsentWithdrawnAt": "2026-03-01T00:00:00Z"],
        ])
        XCTAssertEqual(merged["blanketConsentWithdrawnAt"] as? String, "2026-03-01T00:00:00Z")
        let never = try NppsCore.migration(["op": "mergeWithdrawal", "local": [String: Any](), "incoming": [String: Any]()])
        XCTAssertTrue(never["blanketConsentWithdrawnAt"] is NSNull)
    }

    // @verifies SWR-MG-08
    func testTheOldPhoneClearsOnlyOnAMatchingConfirmation() throws {
        let none = try NppsCore.migration(["op": "clearDecision", "bundleHash": "h", "confirmation": NSNull()])
        XCTAssertEqual(none["clear"] as? Bool, false)
        let ok = try NppsCore.migration(["op": "clearDecision", "bundleHash": "h", "confirmation": ["bundleHash": "h"]])
        XCTAssertEqual(ok["clear"] as? Bool, true)
    }

    // @verifies-none a harness self-check of the refusal mapping, not a requirement
    func testARequestTheCoreCannotReadIsARefusal() {
        XCTAssertThrowsError(try NppsCore.migration(["op": "nope"])) { error in
            XCTAssertTrue(error is NppsCore.Refusal)
        }
    }
}
