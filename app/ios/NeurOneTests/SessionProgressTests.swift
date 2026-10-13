//
//  SessionProgressTests.swift
//  NeurOneTests
//
//  SESSION_PROGRESS as the app decodes it (SWR-SS-07) and the session controls' state (SWR-SS-05). The same cases run
//  on the Kotlin app, so the two decoders cannot drift apart.
//
//  Subject under test: SessionProgress, NeurOneGATTManager.applySessionProgress
//  (app/ios/NeurOne/Models/SessionProgress.swift, app/ios/NeurOne/BLE/NeurOneGATTManager.swift)

import XCTest
import CoreBluetooth
@testable import NeurOne

/// A central that does nothing: the manager is driven directly through `applySessionProgress`.
private final class InertBLECentral: BLECentralManager {
    var state: CBManagerState = .poweredOn
    func scanForPeripherals(withServices: [CBUUID]?, options: [String: Any]?) {}
    func stopScan() {}
    func connect(_ peripheral: CBPeripheral, options: [String: Any]?) {}
    func cancelPeripheralConnection(_ peripheral: CBPeripheral) {}
}

final class SessionProgressTests: XCTestCase {

    private func le32(_ v: UInt32) -> [UInt8] { (0..<4).map { UInt8((v >> UInt32(8 * $0)) & 0xFF) } }

    private func frame(state: UInt8, part: UInt8, mask: UInt16, elapsed: UInt32, remaining: UInt32, total: UInt32,
                       ramp: UInt8, pct: UInt8, rampLeft: UInt32) -> Data {
        Data([state, part, UInt8(mask & 0xFF), UInt8(mask >> 8)] + le32(elapsed) + le32(remaining) + le32(total)
             + [ramp, pct] + le32(rampLeft))
    }

    // @verifies SWR-SS-07
    func testDecodesTheHelmetsFigures() throws {
        let p = try XCTUnwrap(SessionProgress(wire: frame(state: 3, part: 2, mask: 0x0105, elapsed: 61_000,
                                                          remaining: 539_000, total: 600_000, ramp: 0, pct: 0, rampLeft: 0)))
        XCTAssertEqual(p.state, .running)
        XCTAssertEqual(p.activePart, 2)
        XCTAssertEqual(p.activeModalityMask, 0x0105)
        XCTAssertEqual(p.elapsedMs, 61_000)
        XCTAssertEqual(p.remainingMs, 539_000)
        XCTAssertEqual(p.totalMs, 600_000)
        XCTAssertEqual(p.rampPhase, RampPhase.none)
    }

    // @verifies SWR-SS-07
    func testNoRunningPartAndAnUnknownRampTimeAreNilNotNumbers() throws {
        let p = try XCTUnwrap(SessionProgress(wire: frame(state: 4, part: 0xFF, mask: 0, elapsed: 10_000,
                                                          remaining: 90_000, total: 100_000, ramp: 3, pct: 40,
                                                          rampLeft: 0xFFFF_FFFF)))
        XCTAssertNil(p.activePart)
        XCTAssertNil(p.rampRemainingMs)
        XCTAssertEqual(p.rampPhase, .precheck)
        XCTAssertTrue(p.isPaused)
    }

    // @verifies SWR-SS-07
    func testAMalformedFrameIsDroppedWhole() {
        XCTAssertNil(SessionProgress(wire: Data(count: 21)))
        XCTAssertNil(SessionProgress(wire: Data(count: 23)))
        XCTAssertNil(SessionProgress(wire: frame(state: 99, part: 0, mask: 0, elapsed: 0, remaining: 0, total: 0,
                                                 ramp: 0, pct: 0, rampLeft: 0)), "an unknown state")
        XCTAssertNil(SessionProgress(wire: frame(state: 3, part: 0, mask: 0, elapsed: 0, remaining: 0, total: 0,
                                                 ramp: 9, pct: 0, rampLeft: 0)), "an unknown ramp phase")
    }

    // @verifies SWR-SS-07
    func testElapsedIsShownAsAClock() {
        XCTAssertEqual(SessionProgress.formatClock(0), "0:00")
        XCTAssertEqual(SessionProgress.formatClock(61_000), "1:01")
        XCTAssertEqual(SessionProgress.formatClock(3_600_000), "1:00:00")
        XCTAssertEqual(SessionProgress.formatClock(32_399_999), "8:59:59")
    }

    // @verifies SWR-SS-05
    func testASessionIsInProgressWhileRunningPausedOrStoppingAndNotOtherwise() throws {
        func active(_ state: UInt8) throws -> Bool {
            try XCTUnwrap(SessionProgress(wire: frame(state: state, part: 0, mask: 0, elapsed: 0, remaining: 0, total: 0,
                                                      ramp: 0, pct: 0, rampLeft: 0))).isActive
        }
        XCTAssertTrue(try active(3))
        XCTAssertTrue(try active(4))
        XCTAssertTrue(try active(5))
        XCTAssertFalse(try active(0))
        XCTAssertFalse(try active(6))
        XCTAssertFalse(try active(7))
    }

    // @verifies SWR-SS-07
    @MainActor
    func testTheManagerShowsTheFiguresAndAMalformedFrameOrADroppedLinkClearsThem() {
        let manager = NeurOneGATTManager(mockCentral: InertBLECentral())
        let good = frame(state: 4, part: 1, mask: 0, elapsed: 10_000, remaining: 20_000, total: 30_000,
                         ramp: 2, pct: 0, rampLeft: 0)
        manager.applySessionProgress(good)
        XCTAssertEqual(manager.progress?.elapsedMs, 10_000)
        XCTAssertEqual(manager.progress?.state, .paused)

        manager.applySessionProgress(Data(count: 5)) // nothing is partly believed
        XCTAssertNil(manager.progress)

        manager.applySessionProgress(good)
        manager.applyDisconnection()
        XCTAssertNil(manager.progress)
    }

    // @verifies SWR-HV-07
    @MainActor
    func testAHelmetThatStatesNoVersionIsVersionOne() {
        XCTAssertEqual(NeurOneGATTManager(mockCentral: InertBLECentral()).hardwareVersion, 1)
    }

    // @verifies SWR-HV-06
    @MainActor
    func testTheVersionTheHelmetReportsIsUsedAndABadByteIsNotBelieved() {
        let manager = NeurOneGATTManager(mockCentral: InertBLECentral())
        manager.applyHardwareVersion(Data([2]))
        XCTAssertEqual(manager.hardwareVersion, 2)
        manager.applyHardwareVersion(Data([0]))
        XCTAssertEqual(manager.hardwareVersion, 2, "an out-of-range byte changes nothing")
        manager.applyHardwareVersion(Data([16]))
        XCTAssertEqual(manager.hardwareVersion, 2)
        manager.applyHardwareVersion(Data([3, 3]))
        XCTAssertEqual(manager.hardwareVersion, 2, "a frame of the wrong length changes nothing")
        manager.applyHardwareVersion(Data([15]))
        XCTAssertEqual(manager.hardwareVersion, 15)
    }

    // @verifies SWR-HV-06
    @MainActor
    func testTheNextLinkMayBeAnotherHelmetSoADroppedLinkForgetsTheVersion() {
        let manager = NeurOneGATTManager(mockCentral: InertBLECentral())
        manager.applyHardwareVersion(Data([5]))
        manager.applyDisconnection()
        XCTAssertEqual(manager.hardwareVersion, 1)
    }
}
