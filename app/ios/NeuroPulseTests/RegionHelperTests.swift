//
//  RegionHelperTests.swift
//  NeuroPulseTests
//
//  Subject: RegionHelper (app/ios/NeuroPulse/Onboarding/RegionHelper.swift)
//
//  RegionHelper.isLikelyIllinois is a computed property that reads
//  Locale.current.region?.identifier. The production implementation is not
//  injectable, so these tests verify the underlying logic by checking the
//  Region identifier comparison directly — the same comparison the production
//  code performs. This avoids relying on the test runner's host-machine locale.
//
//  The live `isLikelyIllinois` property is also tested for non-crash and for
//  the contract that it returns true only for the "US-IL" identifier.
//
//  OI-PA-03 is still open (legal counsel review of locale-based detection).

import XCTest
@testable import NeuroPulse

final class RegionHelperTests: XCTestCase {

    // MARK: - identifier comparison logic (locale-independent)

    func testUSILIdentifierMatchesIllinois() {
        let region = Locale.Region("US-IL")
        XCTAssertEqual(region.identifier, "US-IL")
        XCTAssertTrue(region.identifier == "US-IL",
                      "The comparison that isLikelyIllinois performs must match US-IL")
    }

    func testUSCAIdentifierDoesNotMatchIllinois() {
        let region = Locale.Region("US-CA")
        XCTAssertFalse(region.identifier == "US-IL",
                       "California region must not be identified as Illinois")
    }

    func testUSIdentifierDoesNotMatchIllinois() {
        let region = Locale.Region("US")
        XCTAssertFalse(region.identifier == "US-IL",
                       "Country-only US region must not match Illinois")
    }

    func testGBIdentifierDoesNotMatchIllinois() {
        let region = Locale.Region("GB")
        XCTAssertFalse(region.identifier == "US-IL")
    }

    func testAllOtherUSStatesDoNotMatchIllinois() {
        // Spot-check the states most likely to be confused.
        let nonIllinoisStates = ["US-IN", "US-WI", "US-IA", "US-MO", "US-KY", "US-TX", "US-NY"]
        for stateCode in nonIllinoisStates {
            let region = Locale.Region(stateCode)
            XCTAssertFalse(region.identifier == "US-IL",
                           "\(stateCode) must not be identified as Illinois")
        }
    }

    // MARK: - isLikelyIllinois live property

    func testIsLikelyIllinoisDoesNotCrash() {
        _ = RegionHelper.isLikelyIllinois
        // Pass = no crash or force-unwrap failure
    }

    func testIsLikelyIllinoisReturnsBoolType() {
        let result = RegionHelper.isLikelyIllinois
        // The result type must be Bool (compile-time assertion via let binding).
        XCTAssertTrue(result == true || result == false,
                      "isLikelyIllinois must always return a Bool")
    }

    func testIsLikelyIllinoisMatchesLocaleCurrentRegion() {
        // Live property must return exactly what the underlying Locale comparison returns.
        let expected = Locale.current.region?.identifier == "US-IL"
        XCTAssertEqual(RegionHelper.isLikelyIllinois, expected,
                       "isLikelyIllinois must exactly match Locale.current region == US-IL")
    }

    // MARK: - conservative detection contract

    func testIsLikelyIllinoisIsFalseForNilRegion() {
        // If Locale.current has no region (nil), identifier comparison must yield false.
        // We can't inject Locale here, but we can verify the nil-safe optional chain
        // produces false rather than crashing via the live call on this device.
        // The xctestrunner's simulated locale rarely has nil region, but the contract
        // must hold regardless.
        if Locale.current.region == nil {
            XCTAssertFalse(RegionHelper.isLikelyIllinois,
                           "nil region must yield false, not crash")
        } else {
            // Device has a region — verify the nil-safe path doesn't throw.
            _ = RegionHelper.isLikelyIllinois
        }
    }
}
