//
//  EngagementTierTests.swift
//  NeurOneTests
//
//  Subject: EngagementTier (app/ios/NeurOne/Analytics/EngagementTier.swift)
//
//  Verifies bucket boundaries, the incrementLaunchCount round-trip, and the
//  zero-launch edge case flagged by the code reviewer (count=0 falls through
//  to `default` which returns .established, inconsistent with the spec of
//  new = 1–5).
//
//  Each test seeds and tears down its own UserDefaults key so tests are
//  independent and order-insensitive.

import XCTest
@testable import NeurOne

final class EngagementTierTests: XCTestCase {

    private let key = "np.analytics.launch-count"

    override func setUp() {
        super.setUp()
        ProtectedDefaults.standard.removeObject(forKey: key)
    }

    override func tearDown() {
        ProtectedDefaults.standard.removeObject(forKey: key)
        super.tearDown()
    }

    // MARK: - bucket boundaries

    // @verifies REQ-SW03-07
    func testCountZeroReturnsNew() {
        // count=0 arises if current() is called before any incrementLaunchCount().
        // The spec defines new = 1–5 launches; 0 must also map to .new (not .established).
        ProtectedDefaults.standard.set(0, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .new,
                       "count=0 must return .new, not .established (reviewer-flagged edge case)")
    }

    // @verifies REQ-SW03-07
    func testCountOneLowerBoundReturnsNew() {
        ProtectedDefaults.standard.set(1, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .new)
    }

    // @verifies REQ-SW03-07
    func testCountFiveUpperBoundReturnsNew() {
        ProtectedDefaults.standard.set(5, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .new)
    }

    // @verifies REQ-SW03-07
    func testCountSixLowerBoundReturnsActive() {
        ProtectedDefaults.standard.set(6, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .active)
    }

    // @verifies REQ-SW03-07
    func testCountFiftyUpperBoundReturnsActive() {
        ProtectedDefaults.standard.set(50, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .active)
    }

    // @verifies REQ-SW03-07
    func testCountFiftyOneLowerBoundReturnsEstablished() {
        ProtectedDefaults.standard.set(51, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .established)
    }

    // @verifies REQ-SW03-07
    func testCountLargeReturnsEstablished() {
        ProtectedDefaults.standard.set(10_000, forKey: key)
        XCTAssertEqual(EngagementTier.current(), .established)
    }

    // MARK: - raw value labels

    // @verifies REQ-SW03-07
    func testRawValuesMatchTelemetrySpec() {
        // NP-APP-TELEMETRY-001 Rev B §3.2 specifies exact string labels.
        XCTAssertEqual(EngagementTier.new.rawValue,         "new")
        XCTAssertEqual(EngagementTier.active.rawValue,      "active")
        XCTAssertEqual(EngagementTier.established.rawValue, "established")
    }

    // MARK: - incrementLaunchCount round-trip

    // @verifies REQ-SW03-07
    func testIncrementFromZeroYieldsOne() {
        // key absent → integer(forKey:) returns 0; after increment should be 1
        EngagementTier.incrementLaunchCount()
        XCTAssertEqual(ProtectedDefaults.standard.integer(forKey: key), 1)
    }

    // @verifies REQ-SW03-07
    func testIncrementIsAdditive() {
        ProtectedDefaults.standard.set(4, forKey: key)
        EngagementTier.incrementLaunchCount()
        XCTAssertEqual(ProtectedDefaults.standard.integer(forKey: key), 5)
    }

    // @verifies REQ-SW03-07
    func testIncrementThenCurrentCrossesBoundaryFromNewToActive() {
        ProtectedDefaults.standard.set(5, forKey: key)
        EngagementTier.incrementLaunchCount()   // becomes 6

        XCTAssertEqual(EngagementTier.current(), .active,
                       "After 6th launch, tier must transition from .new to .active")
    }

    // @verifies REQ-SW03-07
    func testIncrementThenCurrentCrossesBoundaryFromActiveToEstablished() {
        ProtectedDefaults.standard.set(50, forKey: key)
        EngagementTier.incrementLaunchCount()   // becomes 51

        XCTAssertEqual(EngagementTier.current(), .established,
                       "After 51st launch, tier must transition from .active to .established")
    }

    // @verifies REQ-SW03-07
    func testMultipleIncrementsAccumulate() {
        for _ in 0..<10 {
            EngagementTier.incrementLaunchCount()
        }
        XCTAssertEqual(ProtectedDefaults.standard.integer(forKey: key), 10)
        XCTAssertEqual(EngagementTier.current(), .active)
    }
}
