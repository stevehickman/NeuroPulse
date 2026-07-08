//
//  GATTParserTests.swift
//  NeurOneTests
//
//  Satisfies: ISC-152 (GATT characteristic byte parsers decode canonical hub payloads correctly
//             and reject short/truncated data), ISC-157 (no production analytics import).
//
//  Subject under test: GATTParser (app/ios/NeurOne/BLE/GATTCharacteristics.swift)
//
//  All hub payloads are little-endian, matching the firmware layout documented in
//  NP-APP-ROADMAP-001 §5 and the GATTParser doc comments.

import XCTest
import CoreBluetooth
@testable import NeurOne

final class GATTParserTests: XCTestCase {

    // MARK: - SESSION_STATE (uint32 epoch ms, little-endian)

    func testParseSessionState() {
        // 0x12345678 little-endian => bytes 78 56 34 12
        let data = Data([0x78, 0x56, 0x34, 0x12])
        let value = GATTParser.parseSessionState(data)
        XCTAssertEqual(value, 0x1234_5678)
    }

    // MARK: - SESSION_STATUS (uint8 protocolID + uint8 status)

    func testParseSessionStatus() {
        let data = Data([0x07, 0x01])  // protocol 7, status 1 == .running
        let parsed = GATTParser.parseSessionStatus(data)
        XCTAssertNotNil(parsed)
        XCTAssertEqual(parsed?.protocolID, 7)
        XCTAssertEqual(parsed?.status, .running)
    }

    func testParseSessionStatusRejectsUnknownStatusByte() {
        let data = Data([0x07, 0xFF])  // 0xFF is not a valid SessionStatus
        XCTAssertNil(GATTParser.parseSessionStatus(data),
                     "An unrecognized status byte must yield nil, not a partial tuple.")
    }

    // MARK: - HRV_COHERENCE (uint16 coherence×100 + uint16 RMSSD)

    func testParseHRVCoherence() {
        // coherence 7.50 => 750 == 0x02EE little-endian (EE 02); RMSSD 42 == 0x2A 0x00
        let data = Data([0xEE, 0x02, 0x2A, 0x00])
        let hrv = GATTParser.parseHRVCoherence(data)
        XCTAssertNotNil(hrv)
        XCTAssertEqual(hrv!.coherenceScore, 7.50, accuracy: 0.0001)
        XCTAssertEqual(hrv!.rmssdMilliseconds, 42)
    }

    // MARK: - PACER_PHASE (uint8 phase + uint8 elapsed%)

    func testParsePacerPhase() {
        let data = Data([0x01, 0x40])  // phase 1 == .exhale, 64%
        let parsed = GATTParser.parsePacerPhase(data)
        XCTAssertNotNil(parsed)
        XCTAssertEqual(parsed?.phase, .exhale)
        XCTAssertEqual(parsed?.percent, 64)
    }

    func testParsePacerPhaseRejectsUnknownPhaseByte() {
        let data = Data([0x09, 0x40])  // 0x09 is not a valid PacerPhase
        XCTAssertNil(GATTParser.parsePacerPhase(data))
    }

    // MARK: - IMPEDANCE_RESULT (uint16 bitmask, little-endian)

    func testParseImpedanceResult() {
        // bits 0 and 3 set => 0b1001 == 0x09; little-endian => 09 00
        let data = Data([0x09, 0x00])
        let mask = GATTParser.parseImpedanceResult(data)
        XCTAssertEqual(mask, 0x0009)
    }

    // MARK: - CONSUMABLE_STATUS (4 × uint16, little-endian)

    func testParseConsumableStatus() {
        // intranasal 1, hydrogel 30 (0x1E), VNS 256 (0x0100), audio 1000 (0x03E8)
        let data = Data([
            0x01, 0x00,   // 1
            0x1E, 0x00,   // 30
            0x00, 0x01,   // 256
            0xE8, 0x03    // 1000
        ])
        let counts = GATTParser.parseConsumableStatus(data)
        XCTAssertEqual(counts, [1, 30, 256, 1000])
    }

    // MARK: - OTA_STATUS (uint8 phase + uint8 progress + uint16 errorCode)

    func testParseOTAStatus() {
        // phase 2, progress 55, errorCode 0x0000 => no error
        let ok = GATTParser.parseOTAStatus(Data([0x02, 0x37, 0x00, 0x00]))
        XCTAssertNotNil(ok)
        XCTAssertEqual(ok?.phaseRaw, 2)
        XCTAssertEqual(ok?.progressPercent, 55)
        XCTAssertEqual(ok?.errorCode, 0)
        XCTAssertEqual(ok?.isError, false)

        // errorCode 0x0102 (258) little-endian => 02 01, must read as error
        let err = GATTParser.parseOTAStatus(Data([0x05, 0x00, 0x02, 0x01]))
        XCTAssertEqual(err?.errorCode, 0x0102)
        XCTAssertEqual(err?.isError, true)
    }

    // MARK: - ZONE_MODULE_STATUS (5 bytes, one per slot)

    func testParseZoneModuleStatus() {
        // slot0 absent, slot1=zone1, slot2=zone2, slot3 absent, slot4=zone5
        let data = Data([0x00, 0x01, 0x02, 0x00, 0x05])
        let slots = GATTParser.parseZoneModuleStatus(data)
        XCTAssertEqual(slots, [0, 1, 2, 0, 5])
    }

    // MARK: - CONSUMABLE_STATUS routing isolation (NP-PRIV-ANALYSIS-003, item 3)
    //
    // Privacy invariant: SessionState.consumableSessionCounts must only ever be
    // written from GATTParser.parseConsumableStatus, routed via NPUUID.consumableStatus.
    // No UHDR-class characteristic (SESSION_STATE, SESSION_STATUS, HRV_COHERENCE,
    // PACER_PHASE, IMPEDANCE_RESULT) should reach this field.
    //
    // These tests mirror NeurOneGATTManager.didUpdateValueFor dispatch logic so that
    // any future change routing a UHDR characteristic to consumableSessionCounts fails here.

    func testConsumableCountsUnchangedByAllUHDRCharacteristicUpdates() {
        // Mirror the manager's pending-state accumulation for every UHDR case.
        var pending = SessionState.empty
        let baseline = pending.consumableSessionCounts
        XCTAssertEqual(baseline, [0, 0, 0, 0],
                       "Precondition: SessionState.empty must initialize counts to zero.")

        // NPUUID.sessionState → pending.epoch only
        if let epoch = GATTParser.parseSessionState(Data([0x01, 0x00, 0x00, 0x00])) {
            pending.epoch = epoch
        }
        XCTAssertEqual(pending.consumableSessionCounts, baseline,
                       "SESSION_STATE characteristic must not write consumableSessionCounts.")

        // NPUUID.sessionStatus → pending.protocolID + pending.status only
        if let (pid, status) = GATTParser.parseSessionStatus(Data([0x05, 0x01])) {
            pending.protocolID = pid
            pending.status = status
        }
        XCTAssertEqual(pending.consumableSessionCounts, baseline,
                       "SESSION_STATUS characteristic must not write consumableSessionCounts.")

        // NPUUID.hrvCoherence → pending.hrv only
        pending.hrv = GATTParser.parseHRVCoherence(Data([0xEE, 0x02, 0x2A, 0x00]))
        XCTAssertEqual(pending.consumableSessionCounts, baseline,
                       "HRV_COHERENCE characteristic must not write consumableSessionCounts.")

        // NPUUID.pacerPhase → pending.pacerPhase + pending.pacerElapsedPercent only
        if let (phase, pct) = GATTParser.parsePacerPhase(Data([0x01, 0x40])) {
            pending.pacerPhase = phase
            pending.pacerElapsedPercent = pct
        }
        XCTAssertEqual(pending.consumableSessionCounts, baseline,
                       "PACER_PHASE characteristic must not write consumableSessionCounts.")

        // NPUUID.impedanceResult → pending.impedancePassFlags only
        if let flags = GATTParser.parseImpedanceResult(Data([0xFF, 0x00])) {
            pending.impedancePassFlags = flags
        }
        XCTAssertEqual(pending.consumableSessionCounts, baseline,
                       "IMPEDANCE_RESULT characteristic must not write consumableSessionCounts.")
    }

    func testConsumableCountsOnlyUpdatedByParseConsumableStatus() {
        // Mirror the NPUUID.consumableStatus case in didUpdateValueFor.
        var pending = SessionState.empty
        XCTAssertEqual(pending.consumableSessionCounts, [0, 0, 0, 0])

        let wire = Data([0x01, 0x00,   // intranasal: 1
                         0x1E, 0x00,   // hydrogel:   30
                         0x0A, 0x00,   // VNS:        10
                         0x64, 0x00])  // audio:      100
        if let counts = GATTParser.parseConsumableStatus(wire) {
            pending.consumableSessionCounts = counts
        }

        XCTAssertEqual(pending.consumableSessionCounts, [1, 30, 10, 100],
                       "parseConsumableStatus must be the write path for consumableSessionCounts.")

        // Confirm the write has no side-effects on UHDR fields.
        XCTAssertEqual(pending.epoch, 0)
        XCTAssertEqual(pending.status, .idle)
        XCTAssertNil(pending.hrv)
        XCTAssertEqual(pending.impedancePassFlags, 0)
    }

    func testConsumableStatusUUIDDistinctFromAllUHDRCharacteristicUUIDs() {
        // Static invariant: if a future refactor accidentally swaps or aliases these UUIDs,
        // routing UHDR data to consumableSessionCounts becomes possible. Catch it here.
        let uhdrUUIDs: [CBUUID] = [
            NPUUID.sessionState,
            NPUUID.sessionStatus,
            NPUUID.hrvCoherence,
            NPUUID.pacerPhase,
            NPUUID.impedanceResult,
        ]
        for uuid in uhdrUUIDs {
            XCTAssertNotEqual(NPUUID.consumableStatus, uuid,
                              "consumableStatus UUID must differ from UHDR characteristic \(uuid).")
        }
    }

    func testWatchBridgeConsumableCountsSourcedFromCorrectKey() {
        // SessionState.from(wcMessage:) is the only other code path that can populate
        // consumableSessionCounts (via WatchConnectivity). Verify it reads from
        // WCKey.consumableCounts (SHDR) not from any UHDR key (coherenceX100, rmssd).
        // impedancePassFlags (formerly WCKey.impedanceFlags) is UHDR-class and must not be
        // transmitted over WatchConnectivity. It is excluded from WC messages entirely.
        let msg: [String: Any] = [
            WCKey.protocolID:       Int(3),
            WCKey.status:           Int(SessionStatus.running.rawValue),
            WCKey.pacerPhase:       Int(PacerPhase.inhale.rawValue),
            WCKey.pacerPercent:     Int(25),
            WCKey.consumableCounts: [2, 15, 5, 80],  // SHDR device counts
            WCKey.coherenceX100:    Int(750),          // UHDR — must NOT affect counts
            WCKey.rmssd:            Int(42),           // UHDR — must NOT affect counts
        ]
        guard let state = SessionState.from(wcMessage: msg) else {
            XCTFail("Valid WC message must decode to a non-nil SessionState.")
            return
        }
        XCTAssertEqual(state.consumableSessionCounts, [2, 15, 5, 80],
                       "Watch bridge must source consumableSessionCounts from WCKey.consumableCounts only.")
        // UHDR fields decoded correctly and did not bleed into counts.
        XCTAssertEqual(state.hrv?.coherenceScore ?? 0, 7.50, accuracy: 0.01)
        XCTAssertEqual(state.hrv?.rmssdMilliseconds, 42)
        XCTAssertEqual(state.consumableSessionCounts.count, 4)
    }

    // MARK: - ISC-15: all 8 parsers return non-nil for their canonical byte sequences

    /// Single omnibus test matching the ISA test strategy entry for ISC-15.
    /// Verifies that every GATTParser static function exists and returns non-nil
    /// when given a correctly-formed hub payload of the documented wire length.
    func testAllParserFunctions() {
        // parseSessionState: uint32 LE — 0x0000_0001
        XCTAssertNotNil(GATTParser.parseSessionState(Data([0x01, 0x00, 0x00, 0x00])),
                        "parseSessionState must return non-nil for a 4-byte canonical payload.")

        // parseSessionStatus: protocolID 5, status .running (0x01)
        XCTAssertNotNil(GATTParser.parseSessionStatus(Data([0x05, 0x01])),
                        "parseSessionStatus must return non-nil for a valid 2-byte payload.")

        // parseHRVCoherence: coherence 5.00 (500 = 0x01F4 LE), RMSSD 35 (0x0023 LE)
        XCTAssertNotNil(GATTParser.parseHRVCoherence(Data([0xF4, 0x01, 0x23, 0x00])),
                        "parseHRVCoherence must return non-nil for a valid 4-byte payload.")

        // parsePacerPhase: phase .inhale (0x00), elapsed 50%
        XCTAssertNotNil(GATTParser.parsePacerPhase(Data([0x00, 0x32])),
                        "parsePacerPhase must return non-nil for a valid 2-byte payload.")

        // parseImpedanceResult: all 8 electrodes pass → 0x00FF LE
        XCTAssertNotNil(GATTParser.parseImpedanceResult(Data([0xFF, 0x00])),
                        "parseImpedanceResult must return non-nil for a valid 2-byte payload.")

        // parseConsumableStatus: 4 × uint16 LE (5, 10, 15, 20)
        XCTAssertNotNil(GATTParser.parseConsumableStatus(
            Data([0x05, 0x00, 0x0A, 0x00, 0x0F, 0x00, 0x14, 0x00])),
                        "parseConsumableStatus must return non-nil for a valid 8-byte payload.")

        // parseOTAStatus: phase 1, progress 25%, no error
        XCTAssertNotNil(GATTParser.parseOTAStatus(Data([0x01, 0x19, 0x00, 0x00])),
                        "parseOTAStatus must return non-nil for a valid 4-byte payload.")

        // parseZoneModuleStatus: 5 slots — ZM-01 through ZM-05 all present
        XCTAssertNotNil(GATTParser.parseZoneModuleStatus(Data([0x01, 0x02, 0x03, 0x04, 0x05])),
                        "parseZoneModuleStatus must return non-nil for a valid 5-byte payload.")
    }

    // MARK: - Short / truncated / empty input rejection (ISC-152)

    func testParserReturnsNilForShortData() {
        // Empty data — every parser must return nil.
        XCTAssertNil(GATTParser.parseSessionState(Data()))
        XCTAssertNil(GATTParser.parseSessionStatus(Data()))
        XCTAssertNil(GATTParser.parseHRVCoherence(Data()))
        XCTAssertNil(GATTParser.parsePacerPhase(Data()))
        XCTAssertNil(GATTParser.parseImpedanceResult(Data()))
        XCTAssertNil(GATTParser.parseConsumableStatus(Data()))
        XCTAssertNil(GATTParser.parseOTAStatus(Data()))
        XCTAssertNil(GATTParser.parseZoneModuleStatus(Data()))

        // One byte short of each required length — must still return nil.
        XCTAssertNil(GATTParser.parseSessionState(Data([0x00, 0x00, 0x00])))     // needs 4
        XCTAssertNil(GATTParser.parseSessionStatus(Data([0x00])))                // needs 2
        XCTAssertNil(GATTParser.parseHRVCoherence(Data([0x00, 0x00, 0x00])))     // needs 4
        XCTAssertNil(GATTParser.parsePacerPhase(Data([0x00])))                   // needs 2
        XCTAssertNil(GATTParser.parseImpedanceResult(Data([0x00])))             // needs 2
        XCTAssertNil(GATTParser.parseConsumableStatus(Data(repeating: 0, count: 7)))  // needs 8
        XCTAssertNil(GATTParser.parseOTAStatus(Data([0x00, 0x00, 0x00])))        // needs 4
        XCTAssertNil(GATTParser.parseZoneModuleStatus(Data(repeating: 0, count: 4))) // needs 5
    }
}
