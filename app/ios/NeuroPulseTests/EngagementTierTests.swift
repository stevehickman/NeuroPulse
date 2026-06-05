//
//  EngagementTierTests.swift
//  NeuroPulseTests
//
//  Subject: EngagementTier (app/ios/NeuroPulse/Analytics/EngagementTier.swift)
//
//  Verifies bucket boundaries, the incrementLaunchCount round-trip, and the
//  zero-launch edge case flagged by the code reviewer (count=0 falls through
//  to `default` which returns .established, inconsistent with the spec of
//  new = 1–5).
//
//  Each test seeds and tears down its own UserDefaults key so tests are
//  independent and order-insensitive.

import XCTest
@testable import NeuroPulse

final class EngagementTierTests: XCTestCase {

    private let key = "np.analytics.launch-count"

    override func setUp() {
        super.setUp()
        UserDefaults.standard.removeObject(forKey: key)
    }

    override func tearDown() {
        UserDefaults.standard.removeObject(forKey: key)
        super.tearDown()
    }

    // MARK: - bucket boundaries

    func testCountZeroReturnsNew() {
        // count=0 arises if current() is called before any incrementLaunchCount().
        // The spec defines new = 1–5 launches; 0 must also map to .new (not .established).
        UserDefaults.standard.set(0, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .new,
                       "count=0 must return .new, not .established (reviewer-flagged edge case)")
    }

    func testCountOneLowerBoundReturnsNew() {
        UserDefaults.standard.set(1, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .new)
    }

    func testCountFiveUpperBoundReturnsNew() {
        UserDefaults.standard.set(5, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .new)
    }

    func testCountSixLowerBoundReturnsActive() {
        UserDefaults.standard.set(6, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .active)
    }

    func testCountFiftyUpperBoundReturnsActive() {
        UserDefaults.standard.set(50, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .active)
    }

    func testCountFiftyOneLowerBoundReturnsEstablished() {
        UserDefaults.standard.set(51, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .established)
    }

    func testCountLargeReturnsEstablished() {
        UserDefaults.standard.set(10_000, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .established)
    }

    // MARK: - raw value labels

    func testRawValuesMatchTelemetrySpec() {
        // NP-APP-TELEMETRY-001 Rev B §3.2 specifies exact string labels.
        XCTAssertEqual(EngagementTier.new.rawValue,         "new")
        XCTAssertEqual(EngagementTier.active.rawValue,      "active")
        XCTAssertEqual(EngagementTier.established.rawValue, "established")
    }

    // MARK: - incrementLaunchCount round-trip

    func testIncrementFromZeroYieldsOne() {
        // key absent → integer(forKey:) returns 0; after increment should be 1
        EngagementTier.incrementLaunchCount()
        XCTAssertEqual(UserDefaults.standard.integer(forKey: key), 1)
    }

    func testIncrementIsAdditive() {
        UserDefaults.standard.set(4, forKey: key)
        EngagementTier.incrementLaunchCount()
        XCTAssertEqual(UserDefaults.standard.integer(forKey: key), 5)
    }

    func testIncrementThenCurrentCrossesBoundaryFromNewToActive() {
        UserDefaults.standard.set(5, forKey: key)
        EngagementTier.incrementLaunchCount()   // becomes 6

        XCTAssertEqual(EngagementTier.current(), .active,
                       "After 6th launch, tier must transition from .new to .active")
    }

    func testIncrementThenCurrentCrossesBoundaryFromActiveToEstablished() {
        UserDefaults.standard.set(50, forKey: key)
        EngagementTier.incrementLaunchCount()   // becomes 51

        XCTAssertEqual(EngagementTier.current(), .established,
                       "After 51st launch, tier must transition from .active to .established")
    }

    func testMultipleIncrementsAccumulate() {
        for _ in 0..<10 {
            EngagementTier.incrementLaunchCount()
        }
        XCTAssertEqual(UserDefaults.standard.integer(forKey: key), 10)
        XCTAssertEqual(EngagementTier.current(), .active)
    }
}
