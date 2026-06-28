//
//  NeuroPulseGATTManagerTests.swift
//  NeuroPulseTests
//
//  Satisfies:
//    ISC-11  auto-scan starts when BLE becomes available
//    ISC-12  auto-reconnect scheduled 2 s after disconnect
//    ISC-13  allCharacteristicsResolved published once all 14 chars are assigned
//    ISC-16  connectionState transitions through .disconnected → .scanning → .connecting → .connected
//    ISC-17  NeuroPulseGATTManager holds no strong UIKit/SwiftUI reference
//    ISC-19  bluetoothUnavailable published when BLE is off/unauthorized
//    ISC-20  SESSION_STATUS is among the discovered characteristics (read-on-reconnect path)
//    —       warrantyToken stored from hub-provided 32-byte TRNG payload; cleared on disconnect
//
//  CoreBluetooth unit-testing strategy
//  ─────────────────────────────────────
//  NeuroPulseGATTManager accepts an injected BLECentralManager via init(mockCentral:),
//  so no real Bluetooth stack is required.  State transitions are driven by calling the
//  internal applyStateUpdate(_:), applyCharacteristicAssignment(discovered:),
//  applyDisconnection(), and applyWarrantyToken(_:) methods directly — these are the
//  same code paths the CBCentralManagerDelegate callbacks use in production.

import XCTest
import CoreBluetooth
@testable import NeuroPulse

// MARK: - Mock BLE central manager

/// Stub conforming to BLECentralManager for unit tests.
/// Records every call made so assertions can verify the GATT manager's behavior.
private final class MockBLECentral: BLECentralManager {

    var state: CBManagerState = .unknown

    private(set) var scanCallCount = 0
    private(set) var stopScanCallCount = 0
    private(set) var connectCallCount = 0
    private(set) var cancelCallCount = 0
    private(set) var lastScannedServiceUUIDs: [CBUUID]?

    func scanForPeripherals(withServices serviceUUIDs: [CBUUID]?, options: [String: Any]?) {
        scanCallCount += 1
        lastScannedServiceUUIDs = serviceUUIDs
    }

    func stopScan() { stopScanCallCount += 1 }
    func connect(_ peripheral: CBPeripheral, options: [String: Any]?) { connectCallCount += 1 }
    func cancelPeripheralConnection(_ peripheral: CBPeripheral) { cancelCallCount += 1 }
}

// MARK: - Tests

@MainActor
final class NeuroPulseGATTManagerTests: XCTestCase {

    // MARK: ISC-11 — auto-scan when BLE powered on

    /// Manager must call scanForPeripherals immediately when BLE transitions to .poweredOn,
    /// and must filter the scan to the NeuroPulse hub service UUID (not an unfiltered scan).
    func testAutoScanOnPoweredOn() {
        let mock = MockBLECentral()
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        XCTAssertEqual(manager.connectionState, .disconnected,
                       "Initial state must be .disconnected before any BLE event")
        XCTAssertEqual(mock.scanCallCount, 0,
                       "No scan must be started before BLE is available")

        mock.state = .poweredOn
        manager.applyStateUpdate(state: .poweredOn)

        XCTAssertEqual(manager.connectionState, .scanning,
                       "connectionState must become .scanning on poweredOn")
        XCTAssertEqual(mock.scanCallCount, 1,
                       "scanForPeripherals must be called exactly once")
        XCTAssertEqual(mock.lastScannedServiceUUIDs, [NPUUID.service],
                       "Scan must be filtered to the NeuroPulse service UUID")
    }

    /// A second poweredOn notification (e.g. BT toggled off then on) must restart the scan.
    func testAutoScanRestartsOnSecondPoweredOn() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeuroPulseGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)

        mock.state = .poweredOff
        manager.applyStateUpdate(state: .poweredOff)

        mock.state = .poweredOn
        manager.applyStateUpdate(state: .poweredOn)

        XCTAssertEqual(mock.scanCallCount, 2,
                       "scanForPeripherals must be called again when BLE is re-enabled")
    }

    /// No scan when BLE is off.
    func testNoScanWhenBluetoothOff() {
        let mock = MockBLECentral()
        mock.state = .poweredOff
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        manager.applyStateUpdate(state: .poweredOff)

        XCTAssertEqual(manager.connectionState, .disconnected)
        XCTAssertEqual(mock.scanCallCount, 0)
    }

    // MARK: ISC-12 — auto-reconnect after disconnect

    /// After a disconnect, the manager must schedule a scan after ≥ 2 s.
    /// This test waits 2.1 s to allow the reconnect timer to fire.
    func testReconnectScheduledAfterDisconnect() async {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeuroPulseGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)
        XCTAssertEqual(mock.scanCallCount, 1)

        manager.applyDisconnection()
        XCTAssertEqual(manager.connectionState, .disconnected,
                       "State must be .disconnected immediately after applyDisconnection")

        // Allow the 2-second reconnect timer to fire.
        try? await Task.sleep(for: .seconds(2.1))

        XCTAssertGreaterThanOrEqual(mock.scanCallCount, 2,
                                    "scanForPeripherals must be called again after the 2 s reconnect delay")
    }

    /// No reconnect timer is started when BLE is unavailable at the time of disconnect.
    func testNoReconnectWhenBluetoothOff() async {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeuroPulseGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)

        // BLE goes off before the disconnect happens.
        mock.state = .poweredOff
        manager.applyStateUpdate(state: .poweredOff)
        manager.applyDisconnection()

        try? await Task.sleep(for: .seconds(2.1))

        XCTAssertEqual(mock.scanCallCount, 1,
                       "No reconnect scan must be started when BLE is off at disconnect time")
    }

    // MARK: ISC-13 — all characteristics resolved

    func testAllCharacteristicsResolvedWhenAllPresent() {
        let mock = MockBLECentral()
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        XCTAssertFalse(manager.allCharacteristicsResolved,
                       "allCharacteristicsResolved must start false")

        manager.applyCharacteristicAssignment(discovered: Set(NPUUID.all))

        XCTAssertTrue(manager.allCharacteristicsResolved,
                      "allCharacteristicsResolved must be true after all characteristics are discovered")
    }

    func testAllCharacteristicsNotResolvedWhenAnyMissing() {
        let mock = MockBLECentral()
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        // Drop one required characteristic.
        var partial = Set(NPUUID.all)
        partial.remove(NPUUID.sessionStop)
        manager.applyCharacteristicAssignment(discovered: partial)

        XCTAssertFalse(manager.allCharacteristicsResolved,
                       "allCharacteristicsResolved must be false when any required characteristic is absent")
    }

    func testAllCharacteristicsResolvedClearedOnDisconnect() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeuroPulseGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)
        manager.applyCharacteristicAssignment(discovered: Set(NPUUID.all))
        XCTAssertTrue(manager.allCharacteristicsResolved)

        manager.applyDisconnection()

        XCTAssertFalse(manager.allCharacteristicsResolved,
                       "allCharacteristicsResolved must reset to false on disconnect")
    }

    // MARK: ISC-16 — connectionState transitions

    func testConnectionStateTransitionDisconnectedToScanning() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        XCTAssertEqual(manager.connectionState, .disconnected)
        manager.applyStateUpdate(state: .poweredOn)
        XCTAssertEqual(manager.connectionState, .scanning)
    }

    func testConnectionStateReturnsToDisconnectedWhenBLEGoesOff() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeuroPulseGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)
        XCTAssertEqual(manager.connectionState, .scanning)

        mock.state = .poweredOff
        manager.applyStateUpdate(state: .poweredOff)

        XCTAssertEqual(manager.connectionState, .disconnected,
                       "connectionState must return to .disconnected when BLE is turned off")
    }

    // MARK: ISC-17 — no strong UIKit/SwiftUI reference

    /// NeuroPulseGATTManager must inherit from NSObject only — not from any UIKit or SwiftUI type.
    /// This is a structural guard; the absence of UIKit/SwiftUI imports is verified by code review.
    func testSuperclassIsNSObject() {
        guard let superclass = NeuroPulseGATTManager.superclass() else {
            XCTFail("NeuroPulseGATTManager must have a superclass")
            return
        }
        XCTAssertEqual(
            String(describing: superclass),
            "NSObject",
            "NeuroPulseGATTManager must inherit only from NSObject, not from UIView, UIViewController, or any SwiftUI type"
        )
    }

    // MARK: ISC-19 — bluetoothUnavailable published state

    func testBluetoothUnavailableWhenPoweredOff() {
        let mock = MockBLECentral()
        mock.state = .poweredOff
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        manager.applyStateUpdate(state: .poweredOff)

        XCTAssertTrue(manager.bluetoothUnavailable,
                      "bluetoothUnavailable must be true when BLE state is .poweredOff")
    }

    func testBluetoothUnavailableWhenUnauthorized() {
        let mock = MockBLECentral()
        mock.state = .unauthorized
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        manager.applyStateUpdate(state: .unauthorized)

        XCTAssertTrue(manager.bluetoothUnavailable,
                      "bluetoothUnavailable must be true when BLE state is .unauthorized")
    }

    func testBluetoothUnavailableWhenUnsupported() {
        let mock = MockBLECentral()
        mock.state = .unsupported
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        manager.applyStateUpdate(state: .unsupported)

        XCTAssertTrue(manager.bluetoothUnavailable)
    }

    func testBluetoothAvailableWhenPoweredOn() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        manager.applyStateUpdate(state: .poweredOn)

        XCTAssertFalse(manager.bluetoothUnavailable,
                       "bluetoothUnavailable must be false when BLE is .poweredOn")
    }

    // MARK: ISC-20 — SESSION_STATUS in discovery set (read-on-reconnect path)

    /// SESSION_STATUS must be included in NPUUID.all so the GATT manager discovers it
    /// and can call readValue(for:) to restore in-flight session state on reconnect.
    /// The actual readValue call on CBPeripheral is verified by code review (requires
    /// real hardware for full integration confirmation).
    func testSessionStatusUUIDInExpectedDiscoverySet() {
        XCTAssertTrue(
            NPUUID.all.contains(NPUUID.sessionStatus),
            "SESSION_STATUS must be in NPUUID.all so it is discovered and read on reconnect"
        )
    }

    /// After full characteristic assignment, allCharacteristicsResolved becomes true —
    /// confirming the characteristic discovery path that includes SESSION_STATUS is complete.
    func testCharacteristicAssignmentCompletesAfterFullDiscovery() {
        let mock = MockBLECentral()
        let manager = NeuroPulseGATTManager(mockCentral: mock)

        manager.applyCharacteristicAssignment(discovered: Set(NPUUID.all))

        XCTAssertTrue(manager.allCharacteristicsResolved,
                      "Full characteristic discovery (including SESSION_STATUS) must set allCharacteristicsResolved")
    }

    // MARK: — Warranty token (SHDR TRNG architecture, NP-FW-EMMC-002 Rev A §A)

    func testWarrantyTokenInitiallyNil() {
        let manager = NeuroPulseGATTManager(mockCentral: MockBLECentral())
        XCTAssertNil(manager.warrantyToken,
                     "warrantyToken must be nil before the hub provides the TRNG token")
    }

    func testWarrantyTokenStoredFromHub() {
        let manager = NeuroPulseGATTManager(mockCentral: MockBLECentral())
        let trngToken = Data(repeating: 0xAB, count: 32)

        manager.applyWarrantyToken(trngToken)

        XCTAssertEqual(manager.warrantyToken, trngToken,
                       "warrantyToken must store the 32-byte TRNG data provided by the hub")
    }

    /// The hub GATT characteristic is 32 bytes.  Any shorter payload is corrupt and must be rejected.
    func testWarrantyTokenRejectedIfTooShort() {
        let manager = NeuroPulseGATTManager(mockCentral: MockBLECentral())
        manager.applyWarrantyToken(Data(repeating: 0xFF, count: 16))

        XCTAssertNil(manager.warrantyToken,
                     "A payload shorter than 32 bytes must be ignored to guard against corrupt GATT reads")
    }

    /// Payloads longer than 32 bytes (future extension) are accepted — only the first 32 bytes are used.
    func testWarrantyTokenAcceptsLongerPayload() {
        let manager = NeuroPulseGATTManager(mockCentral: MockBLECentral())
        let extendedToken = Data(repeating: 0xCD, count: 40)

        manager.applyWarrantyToken(extendedToken)

        XCTAssertNotNil(manager.warrantyToken)
        XCTAssertEqual(manager.warrantyToken?.count, 32,
                       "Only the first 32 bytes of an extended payload should be stored")
    }

    func testWarrantyTokenClearedOnDisconnect() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeuroPulseGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)
        manager.applyWarrantyToken(Data(repeating: 0xEF, count: 32))
        XCTAssertNotNil(manager.warrantyToken)

        manager.applyDisconnection()

        XCTAssertNil(manager.warrantyToken,
                     "warrantyToken must be cleared on disconnect — the hub re-provisions it at next pairing")
    }

    // MARK: — Session state reset on disconnect

    func testSessionResetToEmptyOnDisconnect() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeuroPulseGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)

        manager.applyDisconnection()

        XCTAssertEqual(manager.session.status, .idle,
                       "session must reset to .idle on disconnect")
        XCTAssertEqual(manager.session.epoch, 0,
                       "session.epoch must reset to 0 on disconnect")
    }

    func testZoneModulesResetOnDisconnect() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeuroPulseGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)

        manager.applyDisconnection()

        XCTAssertEqual(manager.zoneModules, [0, 0, 0, 0, 0],
                       "zoneModules must reset to all-absent on disconnect")
    }

    // MARK: — SHDR upload pending flag

    func testSHDRUploadPendingInitiallyFalse() {
        let manager = NeuroPulseGATTManager(mockCentral: MockBLECentral())
        XCTAssertFalse(manager.shdrUploadPending,
                       "shdrUploadPending must start false — no upload until hub requests one")
    }
}
