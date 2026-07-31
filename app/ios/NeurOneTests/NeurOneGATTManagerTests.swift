//
//  NeurOneGATTManagerTests.swift
//  NeurOneTests
//
//  Satisfies:
//    ISC-11  auto-scan starts when BLE becomes available
//    ISC-12  auto-reconnect scheduled 2 s after disconnect
//    ISC-13  allCharacteristicsResolved published once all 14 chars are assigned
//    ISC-16  connectionState transitions through .disconnected → .scanning → .connecting → .connected
//    ISC-17  NeurOneGATTManager holds no strong UIKit/SwiftUI reference
//    ISC-19  bluetoothUnavailable published when BLE is off/unauthorized
//    ISC-20  SESSION_STATUS is among the discovered characteristics (read-on-reconnect path)
//    —       warrantyToken stored from hub-provided 32-byte TRNG payload; cleared on disconnect
//
//  CoreBluetooth unit-testing strategy
//  ─────────────────────────────────────
//  NeurOneGATTManager accepts an injected BLECentralManager via init(mockCentral:),
//  so no real Bluetooth stack is required.  State transitions are driven by calling the
//  internal applyStateUpdate(_:), applyCharacteristicAssignment(discovered:),
//  applyDisconnection(), and applyWarrantyToken(_:) methods directly — these are the
//  same code paths the CBCentralManagerDelegate callbacks use in production.

import XCTest
import CoreBluetooth
@testable import NeurOne

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
final class NeurOneGATTManagerTests: XCTestCase {

    // MARK: ISC-11 — auto-scan when BLE powered on

    /// Manager must call scanForPeripherals immediately when BLE transitions to .poweredOn,
    /// and must filter the scan to the NeurOne hub service UUID (not an unfiltered scan).
    func testAutoScanOnPoweredOn() {
        let mock = MockBLECentral()
        let manager = NeurOneGATTManager(mockCentral: mock)

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
                       "Scan must be filtered to the NeurOne service UUID")
    }

    /// A second poweredOn notification (e.g. BT toggled off then on) must restart the scan.
    func testAutoScanRestartsOnSecondPoweredOn() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeurOneGATTManager(mockCentral: mock)
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
        let manager = NeurOneGATTManager(mockCentral: mock)

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
        let manager = NeurOneGATTManager(mockCentral: mock)
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
        let manager = NeurOneGATTManager(mockCentral: mock)
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
        let manager = NeurOneGATTManager(mockCentral: mock)

        XCTAssertFalse(manager.allCharacteristicsResolved,
                       "allCharacteristicsResolved must start false")

        manager.applyCharacteristicAssignment(discovered: Set(NPUUID.all))

        XCTAssertTrue(manager.allCharacteristicsResolved,
                      "allCharacteristicsResolved must be true after all characteristics are discovered")
    }

    func testAllCharacteristicsNotResolvedWhenAnyMissing() {
        let mock = MockBLECentral()
        let manager = NeurOneGATTManager(mockCentral: mock)

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
        let manager = NeurOneGATTManager(mockCentral: mock)
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
        let manager = NeurOneGATTManager(mockCentral: mock)

        XCTAssertEqual(manager.connectionState, .disconnected)
        manager.applyStateUpdate(state: .poweredOn)
        XCTAssertEqual(manager.connectionState, .scanning)
    }

    func testConnectionStateReturnsToDisconnectedWhenBLEGoesOff() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeurOneGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)
        XCTAssertEqual(manager.connectionState, .scanning)

        mock.state = .poweredOff
        manager.applyStateUpdate(state: .poweredOff)

        XCTAssertEqual(manager.connectionState, .disconnected,
                       "connectionState must return to .disconnected when BLE is turned off")
    }

    // MARK: ISC-17 — no strong UIKit/SwiftUI reference

    /// NeurOneGATTManager must inherit from NSObject only — not from any UIKit or SwiftUI type.
    /// This is a structural guard; the absence of UIKit/SwiftUI imports is verified by code review.
    func testSuperclassIsNSObject() {
        guard let superclass = NeurOneGATTManager.superclass() else {
            XCTFail("NeurOneGATTManager must have a superclass")
            return
        }
        XCTAssertEqual(
            String(describing: superclass),
            "NSObject",
            "NeurOneGATTManager must inherit only from NSObject, not from UIView, UIViewController, or any SwiftUI type"
        )
    }

    // MARK: ISC-19 — bluetoothUnavailable published state

    func testBluetoothUnavailableWhenPoweredOff() {
        let mock = MockBLECentral()
        mock.state = .poweredOff
        let manager = NeurOneGATTManager(mockCentral: mock)

        manager.applyStateUpdate(state: .poweredOff)

        XCTAssertTrue(manager.bluetoothUnavailable,
                      "bluetoothUnavailable must be true when BLE state is .poweredOff")
    }

    func testBluetoothUnavailableWhenUnauthorized() {
        let mock = MockBLECentral()
        mock.state = .unauthorized
        let manager = NeurOneGATTManager(mockCentral: mock)

        manager.applyStateUpdate(state: .unauthorized)

        XCTAssertTrue(manager.bluetoothUnavailable,
                      "bluetoothUnavailable must be true when BLE state is .unauthorized")
    }

    func testBluetoothUnavailableWhenUnsupported() {
        let mock = MockBLECentral()
        mock.state = .unsupported
        let manager = NeurOneGATTManager(mockCentral: mock)

        manager.applyStateUpdate(state: .unsupported)

        XCTAssertTrue(manager.bluetoothUnavailable)
    }

    func testBluetoothAvailableWhenPoweredOn() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeurOneGATTManager(mockCentral: mock)

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
        let manager = NeurOneGATTManager(mockCentral: mock)

        manager.applyCharacteristicAssignment(discovered: Set(NPUUID.all))

        XCTAssertTrue(manager.allCharacteristicsResolved,
                      "Full characteristic discovery (including SESSION_STATUS) must set allCharacteristicsResolved")
    }

    // MARK: — Warranty token (SHDR TRNG architecture, NP-FW-EMMC-002 Rev A §A)

    func testWarrantyTokenInitiallyNil() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())
        XCTAssertNil(manager.warrantyToken,
                     "warrantyToken must be nil before the hub provides the TRNG token")
    }

    func testWarrantyTokenStoredFromHub() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())
        let trngToken = Data(repeating: 0xAB, count: 32)

        manager.applyWarrantyToken(trngToken)

        XCTAssertEqual(manager.warrantyToken, trngToken,
                       "warrantyToken must store the 32-byte TRNG data provided by the hub")
    }

    /// The hub GATT characteristic is 32 bytes.  Any shorter payload is corrupt and must be rejected.
    func testWarrantyTokenRejectedIfTooShort() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())
        manager.applyWarrantyToken(Data(repeating: 0xFF, count: 16))

        XCTAssertNil(manager.warrantyToken,
                     "A payload shorter than 32 bytes must be ignored to guard against corrupt GATT reads")
    }

    /// Payloads longer than 32 bytes (future extension) are accepted — only the first 32 bytes are used.
    func testWarrantyTokenAcceptsLongerPayload() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())
        let extendedToken = Data(repeating: 0xCD, count: 40)

        manager.applyWarrantyToken(extendedToken)

        XCTAssertNotNil(manager.warrantyToken)
        XCTAssertEqual(manager.warrantyToken?.count, 32,
                       "Only the first 32 bytes of an extended payload should be stored")
    }

    func testWarrantyTokenClearedOnDisconnect() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeurOneGATTManager(mockCentral: mock)
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
        let manager = NeurOneGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)

        manager.applyDisconnection()

        XCTAssertEqual(manager.session.status, .idle,
                       "session must reset to .idle on disconnect")
        XCTAssertEqual(manager.session.epoch, 0,
                       "session.epoch must reset to 0 on disconnect")
    }

    // MARK: — Zone module socket map (socket-keyed, variable length)

    /// v2 module-status frame, matching np_zone_notify.h — 3-byte records
    /// (socket id, module type, flags). Anatomy is not here; it arrives once via
    /// the socket map at link time.
    private func zoneFrame(snapshot: Bool, last: Bool, fragment: UInt8 = 0,
                           records: [(UInt8, UInt8, UInt8)]) -> Data {
        var d = Data([0x02,
                      (snapshot ? 0x01 : 0x00) | (last ? 0x02 : 0x00),
                      fragment,
                      UInt8(records.count)])
        for r in records { d.append(contentsOf: [r.0, r.1, r.2]) }
        return d
    }

    /// v2 socket-map frame — 7-byte records, MAP kind bit set.
    private func socketMapFrame(last: Bool, fragment: UInt8 = 0,
                                records: [(UInt8, UInt8, UInt8, Int16, Int16)]) -> Data {
        var d = Data([0x02, 0x01 | (last ? 0x02 : 0x00) | 0x04, fragment,
                      UInt8(records.count)])
        for r in records {
            let ux = UInt16(bitPattern: r.3), uy = UInt16(bitPattern: r.4)
            d.append(contentsOf: [r.0, r.1, r.2,
                                  UInt8(ux & 0xFF), UInt8(ux >> 8),
                                  UInt8(uy & 0xFF), UInt8(uy >> 8)])
        }
        return d
    }

    func testZoneModulesResetOnDisconnect() {
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeurOneGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)

        // Seat a module first so the reset has something to clear.
        manager.applyZoneModuleStatus(
            zoneFrame(snapshot: true, last: true, records: [(12, 2, 0x01)]))
        XCTAssertFalse(manager.zoneModules.isEmpty, "precondition: map populated")

        manager.applyDisconnection()

        XCTAssertTrue(manager.zoneModules.isEmpty,
                      "the socket map must clear entirely on disconnect — a stale "
                      + "'present' would let a placement gate pass on a pulled module")
    }

    func testZoneModuleSnapshotPopulatesSocketMap() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())

        // Sockets well beyond the retired five, including the top of the domain.
        manager.applyZoneModuleStatus(zoneFrame(snapshot: true, last: true, records: [
            (12, 2, 0x01),    // frontal left, EEG, present
            (47, 1, 0x01),    // parietal midline, base PBM, present
            (128, 1, 0x00),   // occipital midline, empty
        ]))

        XCTAssertEqual(manager.zoneModules.orderedSockets.map(\.socketID), [12, 47, 128])
        XCTAssertEqual(manager.zoneModules.presentSockets.count, 2)
        XCTAssertTrue(manager.zoneModules.isPresent(socket: 47))
        XCTAssertFalse(manager.zoneModules.isPresent(socket: 128))
    }

    func testZoneModuleDeltaUpdatesOnlyNamedSocket() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())
        manager.applyZoneModuleStatus(zoneFrame(snapshot: true, last: true, records: [
            (12, 2, 0x01),
            (47, 1, 0x01),
        ]))

        // Socket 12 removed; socket 47 untouched by the delta.
        manager.applyZoneModuleStatus(
            zoneFrame(snapshot: false, last: true, records: [(12, 0, 0x00)]))

        XCTAssertFalse(manager.zoneModules.isPresent(socket: 12))
        XCTAssertTrue(manager.zoneModules.isPresent(socket: 47),
                      "a delta must not disturb sockets it does not name")
    }

    func testZoneModuleSnapshotReplacesRatherThanMerges() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())
        manager.applyZoneModuleStatus(
            zoneFrame(snapshot: true, last: true, records: [(12, 2, 0x01)]))
        manager.applyZoneModuleStatus(
            zoneFrame(snapshot: true, last: true, records: [(47, 1, 0x01)]))

        XCTAssertEqual(manager.zoneModules.orderedSockets.map(\.socketID), [47],
                       "a snapshot is a complete inventory claim and must replace, not merge")
    }

    func testZoneModuleMalformedFrameLeavesMapUntouched() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())
        manager.applyZoneModuleStatus(
            zoneFrame(snapshot: true, last: true, records: [(12, 2, 0x01)]))

        // Wrong version, then a truncated body, then a map frame on the status
        // path (wrong kind bit).
        manager.applyZoneModuleStatus(Data([0x03, 0x03, 0x00, 0x01, 0x05, 0x01, 0x01]))
        manager.applyZoneModuleStatus(Data([0x02, 0x03, 0x00, 0x04, 0x05]))
        manager.applyZoneModuleStatus(socketMapFrame(last: true,
                                                     records: [(5, 0x09, 0x01, 0, 0)]))

        XCTAssertEqual(manager.zoneModules.orderedSockets.map(\.socketID), [12],
                       "malformed frames must be dropped whole, not partially applied")
    }

    func testZoneModuleEventFiresOncePerActualChange() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())
        var events: [ZoneModuleStatus] = []
        let sub = manager.zoneModuleEventPublisher.sink { events.append($0) }
        defer { sub.cancel() }

        manager.applyZoneModuleStatus(
            zoneFrame(snapshot: true, last: true, records: [(12, 2, 0x01)]))
        XCTAssertEqual(events.count, 1, "first insertion emits one event")

        // Same hardware re-reported — the hub re-sends snapshots freely.
        manager.applyZoneModuleStatus(
            zoneFrame(snapshot: true, last: true, records: [(12, 2, 0x01)]))
        XCTAssertEqual(events.count, 1,
                       "an unchanged republish must emit nothing — otherwise setup "
                       + "re-announces sockets that were already seated")

        // Now actually removed.
        manager.applyZoneModuleStatus(
            zoneFrame(snapshot: true, last: true, records: [(12, 0, 0x00)]))
        XCTAssertEqual(events.count, 2, "a real change emits")
        XCTAssertEqual(events.last?.isPresent, false)
    }

    func testSocketMapReadAtLinkResolvesLabelsForLaterChanges() {
        // The static/dynamic split end to end: the map arrives once, then a
        // 3-byte status change is enough to name a place.
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())

        manager.applySocketMap(socketMapFrame(last: true, records: [
            (12, 0x01 | (0x01 << 3), 0x01, -42, 137),   // frontal left
        ]))
        XCTAssertEqual(manager.socketMap.count, 1)
        XCTAssertTrue(manager.socketMap.label(for: 12).localizedCaseInsensitiveContains("frontal"))

        manager.applyZoneModuleStatus(
            zoneFrame(snapshot: false, last: true, records: [(12, 2, 0x01)]))
        XCTAssertTrue(manager.zoneModules.isPresent(socket: 12))
    }

    func testSocketMapClearedOnDisconnect() {
        // The map describes THIS helmet. The next link may be a different one, or
        // the same one after a lattice-changing firmware update.
        let mock = MockBLECentral()
        mock.state = .poweredOn
        let manager = NeurOneGATTManager(mockCentral: mock)
        manager.applyStateUpdate(state: .poweredOn)
        manager.applySocketMap(socketMapFrame(last: true, records: [(1, 0x09, 0x01, 0, 0)]))
        XCTAssertFalse(manager.socketMap.isEmpty, "precondition")

        manager.applyDisconnection()

        XCTAssertTrue(manager.socketMap.isEmpty,
                      "a cached lattice would name sockets wrongly after a re-cut")
    }

    func testZoneModuleFragmentedSnapshotCommitsOnlyWhenComplete() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())

        manager.applyZoneModuleStatus(zoneFrame(snapshot: true, last: false, fragment: 0,
                                                records: [(1, 1, 0x01)]))
        XCTAssertTrue(manager.zoneModules.isEmpty,
                      "a part-received snapshot must not be applied")

        manager.applyZoneModuleStatus(zoneFrame(snapshot: true, last: true, fragment: 1,
                                                records: [(2, 1, 0x01)]))
        XCTAssertEqual(manager.zoneModules.orderedSockets.map(\.socketID), [1, 2],
                       "the snapshot commits as one map when the last fragment lands")
    }

    // MARK: — SHDR upload pending flag

    func testSHDRUploadPendingInitiallyFalse() {
        let manager = NeurOneGATTManager(mockCentral: MockBLECentral())
        XCTAssertFalse(manager.shdrUploadPending,
                       "shdrUploadPending must start false — no upload until hub requests one")
    }
}
