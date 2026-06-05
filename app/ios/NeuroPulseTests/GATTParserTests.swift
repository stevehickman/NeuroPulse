//
//  GATTParserTests.swift
//  NeuroPulseTests
//
//  Satisfies: ISC-152 (GATT characteristic byte parsers decode canonical hub payloads correctly
//             and reject short/truncated data), ISC-157 (no production analytics import).
//
//  Subject under test: GATTParser (app/ios/NeuroPulse/BLE/GATTCharacteristics.swift)
//
//  All hub payloads are little-endian, matching the firmware layout documented in
//  NP-APP-ROADMAP-001 §5 and the GATTParser doc comments.

import XCTest
@testable import NeuroPulse

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
                     "An unrecognised status byte must yield nil, not a partial tuple.")
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
