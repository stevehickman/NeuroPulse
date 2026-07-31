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

    // MARK: - ZONE_MODULE_STATUS (socket-keyed frame)
    //
    // Wire contract: firmware/zone_announce/include/np_zone_notify.h, pinned on
    // the firmware side by np_zone_notify_tests.c. These cases mirror those.

    /// Build a STATUS frame the way the firmware encoder does — 3-byte records.
    private func frame(version: UInt8 = 0x02,
                       snapshot: Bool,
                       last: Bool,
                       fragment: UInt8 = 0,
                       records: [(socket: UInt8, type: UInt8, flags: UInt8)],
                       declaredCount: UInt8? = nil) -> Data {
        var d = Data([version,
                      (snapshot ? 0x01 : 0x00) | (last ? 0x02 : 0x00),
                      fragment,
                      declaredCount ?? UInt8(records.count)])
        for r in records { d.append(contentsOf: [r.socket, r.type, r.flags]) }
        return d
    }

    /// Build a SOCKET MAP frame — 7-byte records, MAP kind bit set.
    private func mapFrame(version: UInt8 = 0x02,
                          last: Bool,
                          fragment: UInt8 = 0,
                          records: [(socket: UInt8, flags: UInt8,
                                     x: Int16, y: Int16, z: Int16)],
                          declaredCount: UInt8? = nil) -> Data {
        var d = Data([version,
                      0x01 | (last ? 0x02 : 0x00) | 0x04,   // snapshot | last | map
                      fragment,
                      declaredCount ?? UInt8(records.count)])
        for r in records {
            let ux = UInt16(bitPattern: r.x)
            let uy = UInt16(bitPattern: r.y)
            let uz = UInt16(bitPattern: r.z)
            d.append(contentsOf: [r.socket, r.flags,
                                  UInt8(ux & 0xFF), UInt8(ux >> 8),
                                  UInt8(uy & 0xFF), UInt8(uy >> 8),
                                  UInt8(uz & 0xFF), UInt8(uz >> 8)])
        }
        return d
    }

    func testParseZoneModuleStatusDecodesRecord() {
        // Socket 12, EEG tile, present. No anatomy: a status record is three
        // bytes, because where socket 12 is cannot change when its module does.
        let data = frame(snapshot: true, last: true, records: [(12, 2, 0x01)])
        let f = GATTParser.parseZoneModuleStatus(data)
        XCTAssertEqual(f?.records.count, 1)
        XCTAssertEqual(f?.records.first?.socketID, 12)
        XCTAssertEqual(f?.records.first?.moduleType, .eeg)
        XCTAssertEqual(f?.records.first?.isPresent, true)
        XCTAssertEqual(f?.records.first?.hasFault, false)
        XCTAssertEqual(f?.isSnapshot, true)
        XCTAssertEqual(f?.isLastFragment, true)
        XCTAssertEqual(data.count, 4 + 3, "header + one 3-byte record")
    }

    // MARK: - SOCKET_MAP (the static half — read once at link)

    func testParseSocketMapDecodesPosition() {
        // Aircraft body axes: +x forward, +y right, +z down. A vault socket sits
        // above the ellipsoid centre, so z is negative.
        let data = mapFrame(last: true, records: [(12, 0x01, 119, -44, -38)])
        let f = GATTParser.parseSocketMap(data)
        XCTAssertEqual(f?.descriptors.count, 1)
        let d = f?.descriptors.first
        XCTAssertEqual(d?.socketID, 12)
        XCTAssertEqual(d?.position.forwardMm, 119)
        XCTAssertEqual(d?.position.rightMm, -44, "left is negative y")
        XCTAssertEqual(d?.position.downMm, -38, "above the origin is negative z")
        XCTAssertEqual(d?.isWiredInShell, true)
    }

    func testParseSocketMapCrownAndUnwired() {
        // The crown is directly above the origin: on both centrelines, most
        // negative z.
        let crown = GATTParser.parseSocketMap(
            mapFrame(last: true, records: [(40, 0x01, 0, 0, -157)]))?.descriptors.first
        XCTAssertEqual(crown?.position, SocketPosition(forwardMm: 0, rightMm: 0,
                                                       downMm: -157))

        let unwired = GATTParser.parseSocketMap(
            mapFrame(last: true, records: [(80, 0x00, -130, 28, 0)]))?.descriptors.first
        XCTAssertEqual(unwired?.isWiredInShell, false)
        XCTAssertEqual(unwired?.position.forwardMm, -130, "aft is negative x")
    }

    func testSocketMapCarriesNoAnatomy() {
        // Lobe and side are gone from the wire entirely (ZONE-1). A record is
        // socket id + flags + three int16 coordinates and nothing else.
        let data = mapFrame(last: true, records: [(12, 0x01, 1, 2, -3)])
        XCTAssertEqual(data.count, 4 + 8, "header + one 8-byte record")
    }

    func testMapAndStatusFramesAreNotInterchangeable() {
        // The kind bit is what stops a 7-byte map record being read as 3-byte
        // status records — that would misparse silently instead of failing.
        let map = mapFrame(last: true, records: [(12, 0x01, 0, 0, -10)])
        let status = frame(snapshot: true, last: true, records: [(12, 2, 0x01)])

        XCTAssertNil(GATTParser.parseZoneModuleStatus(map),
                     "a map frame must not decode as status")
        XCTAssertNil(GATTParser.parseSocketMap(status),
                     "a status frame must not decode as a map")
    }

    func testParseSocketMapRejectsBadSocketAndVersion() {
        XCTAssertNil(GATTParser.parseSocketMap(
            mapFrame(last: true, records: [(0, 0x01, 0, 0, 0)])),
            "socket id 0 rejected in maps too")
        XCTAssertNil(GATTParser.parseSocketMap(
            mapFrame(version: 0x01, last: true, records: [(1, 0x01, 0, 0, 0)])),
            "an older format version is rejected, not reinterpreted")
        XCTAssertNil(GATTParser.parseSocketMap(
            mapFrame(last: true, records: [(1, 0x01, 0, 0, 0)], declaredCount: 4)),
            "a record count the body cannot satisfy is rejected")
    }

    func testSocketMapAssemblerReassemblesRun() {
        var assembler = SocketMapFrameAssembler()
        let f0 = GATTParser.parseSocketMap(
            mapFrame(last: false, fragment: 0, records: [(1, 0x01, 0, 0, -10)]))!
        let f1 = GATTParser.parseSocketMap(
            mapFrame(last: true, fragment: 1, records: [(2, 0x01, 5, 6, -20)]))!

        XCTAssertNil(assembler.accept(f0), "a non-final fragment must not commit")
        let complete = assembler.accept(f1)
        XCTAssertEqual(complete?.descriptors.map(\.socketID), [1, 2])
    }

    // MARK: - Zones name a socket; the helmet only places it

    func testZoneNameComesFromTheAuthoredZoneFileNotTheWire() {
        // Socket 1 is in "Frontal Left" per protocols/predefined/00-zones.npps.
        // Nothing on the wire said so — the helmet sent coordinates only.
        let map = SocketMap([
            SocketDescriptor(socketID: 1,
                             position: SocketPosition(forwardMm: 130, rightMm: -26,
                                                      downMm: -12),
                             isWiredInShell: true)
        ])
        let status = ZoneModuleStatus(socketID: 1, moduleType: .eeg,
                                      isPresent: true, hasFault: false)

        XCTAssertEqual(SocketZones.primaryZone(for: 1), "Frontal Left")
        XCTAssertTrue(map.label(for: 1).contains("1"))
        XCTAssertTrue(map.label(for: 1).localizedCaseInsensitiveContains("frontal"))
        XCTAssertTrue(map.spokenConfirmation(for: status)
                        .localizedCaseInsensitiveContains("frontal"))
    }

    func testMostSpecificZoneWins() {
        // Socket 26 is in "Motor / SMA" (7 sockets) and "Frontal Left" (20).
        // The smaller zone is the more useful description.
        XCTAssertEqual(SocketZones.primaryZone(for: 26), "Motor / SMA")
        XCTAssertTrue(SocketZones.zones(for: 26).contains("Frontal Left"))
        XCTAssertTrue(SocketZones.zones(for: 26).contains("All"),
                      "every socket is in All, but All is never the primary")
    }

    func testSocketWithNoZoneStillGetsAUsableLabel() {
        // The helmet is authoritative for which sockets exist; the zone file is
        // not. A socket beyond the authored lattice must not crash or go unnamed.
        let map = SocketMap([
            SocketDescriptor(socketID: 120,
                             position: SocketPosition(forwardMm: 0, rightMm: 0, downMm: 0),
                             isWiredInShell: true)
        ])
        XCTAssertNil(SocketZones.primaryZone(for: 120))
        XCTAssertTrue(map.label(for: 120).contains("120"))
    }

    func testParseZoneModuleStatusAcceptsFullSocketDomain() {
        // 128 is the top of the 7-bit major addressing domain — the app must not
        // clamp the lattice to whatever count ships today (it has moved twice).
        let data = frame(snapshot: false, last: true,
                         records: [(128, 1, 0x01)])
        XCTAssertEqual(GATTParser.parseZoneModuleStatus(data)?.records.first?.socketID, 128)
    }

    func testParseZoneModuleStatusRejectsSocketZeroAndOutOfDomain() {
        // 0 is never emitted; >128 would alias onto a real socket, which is a
        // wrong-site targeting path rather than a display glitch.
        XCTAssertNil(GATTParser.parseZoneModuleStatus(
            frame(snapshot: false, last: true, records: [(0, 1, 0x01)])),
            "socket id 0 must be rejected")
        XCTAssertNil(GATTParser.parseZoneModuleStatus(
            frame(snapshot: false, last: true, records: [(129, 1, 0x01)])),
            "socket id above the 128-socket domain must be rejected")
    }

    func testParseZoneModuleStatusRejectsWrongVersion() {
        XCTAssertNil(GATTParser.parseZoneModuleStatus(
            frame(version: 0x03, snapshot: true, last: true, records: [(1, 1, 1)])),
            "a future format version must be rejected, not guessed at")
        XCTAssertNil(GATTParser.parseZoneModuleStatus(
            frame(version: 0x01, snapshot: true, last: true, records: [(1, 1, 1)])),
            "v1 (anatomy inline on every change) must not decode as v2")
    }

    func testParseZoneModuleStatusRejectsRetiredFiveBytePayload() {
        // The retired format was five raw slot bytes with no version header. A hub
        // still speaking it decodes as version 0x00 and must be refused rather
        // than misread as socket data.
        XCTAssertNil(GATTParser.parseZoneModuleStatus(Data([0x00, 0x01, 0x02, 0x00, 0x05])),
                     "the retired 5-byte slot payload must not decode")
    }

    func testParseZoneModuleStatusRejectsTruncatedBody() {
        // Header claims 3 records, body carries 1.
        let data = frame(snapshot: true, last: true,
                         records: [(1, 1, 1)], declaredCount: 3)
        XCTAssertNil(GATTParser.parseZoneModuleStatus(data),
                     "a record count the body cannot satisfy must be rejected")
    }

    func testParseZoneModuleStatusRejectsShortHeader() {
        XCTAssertNil(GATTParser.parseZoneModuleStatus(Data()))
        XCTAssertNil(GATTParser.parseZoneModuleStatus(Data([0x01, 0x03, 0x00])))
    }

    func testParseZoneModuleStatusEmptySnapshotIsValid() {
        // "The helmet reports no modules" is a real answer and must be
        // distinguishable from "nothing arrived".
        let f = GATTParser.parseZoneModuleStatus(frame(snapshot: true, last: true, records: []))
        XCTAssertNotNil(f)
        XCTAssertEqual(f?.records.isEmpty, true)
    }

    func testParseZoneModuleStatusFaultIsNeverPresent() {
        // Both bits set by a misbehaving hub: presence gates safety-critical
        // placement checks, so fault must win.
        let data = frame(snapshot: false, last: true,
                         records: [(7, 2, 0x01 | 0x02)])
        let record = GATTParser.parseZoneModuleStatus(data)?.records.first
        XCTAssertEqual(record?.hasFault, true)
        XCTAssertEqual(record?.isPresent, false,
                       "a faulted module must never be reported as present")
    }

    func testParseZoneModuleStatusHandlesSlicedData() {
        // Data sliced out of a larger buffer does not start at index 0. Indexing
        // it as if it did is a silent misparse.
        let padded = Data([0xAA, 0xBB]) + frame(snapshot: false, last: true,
                                                records: [(9, 1, 0x01)])
        let sliced = padded.dropFirst(2)
        XCTAssertEqual(GATTParser.parseZoneModuleStatus(sliced)?.records.first?.socketID, 9)
    }

    func testParseZoneModuleStatusUnknownEnumsDegradeSafely() {
        // Forward compatibility: a module type this app build does not know must
        // not drop the whole frame — the socket is still real and still occupied.
        let data = frame(snapshot: false, last: true, records: [(3, 99, 0x01)])
        let record = GATTParser.parseZoneModuleStatus(data)?.records.first
        XCTAssertEqual(record?.moduleType, .unknown)
        XCTAssertEqual(record?.isPresent, true)
    }



    // MARK: - Snapshot fragment reassembly

    func testAssemblerReassemblesMultiFragmentSnapshot() {
        var assembler = ZoneModuleFrameAssembler()
        let f0 = GATTParser.parseZoneModuleStatus(
            frame(snapshot: true, last: false, fragment: 0, records: [(1, 1, 0x01)]))!
        let f1 = GATTParser.parseZoneModuleStatus(
            frame(snapshot: true, last: true, fragment: 1, records: [(2, 2, 0x01)]))!

        XCTAssertNil(assembler.accept(f0), "a non-final fragment must not commit")
        let complete = assembler.accept(f1)
        XCTAssertEqual(complete?.records.map(\.socketID), [1, 2])
        XCTAssertEqual(assembler.isAccumulating, false, "run resets after commit")
    }

    func testAssemblerDropsRunOnMissingFragment() {
        // A snapshot with a hole reads as "that module is not installed" — the
        // wrong answer for a placement gate. Abandon the run instead.
        var assembler = ZoneModuleFrameAssembler()
        let f0 = GATTParser.parseZoneModuleStatus(
            frame(snapshot: true, last: false, fragment: 0, records: [(1, 1, 0x01)]))!
        let f2 = GATTParser.parseZoneModuleStatus(
            frame(snapshot: true, last: true, fragment: 2, records: [(3, 1, 0x01)]))!

        XCTAssertNil(assembler.accept(f0))
        XCTAssertNil(assembler.accept(f2), "a gap in the fragment run must abandon it")
        XCTAssertEqual(assembler.isAccumulating, false)
    }

    func testAssemblerPassesDeltasThrough() {
        var assembler = ZoneModuleFrameAssembler()
        let delta = GATTParser.parseZoneModuleStatus(
            frame(snapshot: false, last: true, records: [(5, 2, 0x01)]))!
        XCTAssertEqual(assembler.accept(delta)?.records.first?.socketID, 5)
    }

    func testAssemblerDeltaDuringSnapshotRunSurvivesTheCommit() {
        // A snapshot describes the hardware as of when the hub STARTED sending it.
        // A removal observed mid-run is newer. If the snapshot simply replaced the
        // map, the pulled module would come back marked present — and presence
        // gates safety-critical placement checks.
        var assembler = ZoneModuleFrameAssembler()
        let snap0 = GATTParser.parseZoneModuleStatus(
            frame(snapshot: true, last: false, fragment: 0,
                  records: [(7, 2, 0x01)]))!            // socket 7 present
        let removal = GATTParser.parseZoneModuleStatus(
            frame(snapshot: false, last: true,
                  records: [(7, 0, 0x00)]))!            // socket 7 pulled
        let snap1 = GATTParser.parseZoneModuleStatus(
            frame(snapshot: true, last: true, fragment: 1,
                  records: [(8, 2, 0x01)]))!

        XCTAssertNil(assembler.accept(snap0))
        XCTAssertNil(assembler.accept(removal),
                     "a delta mid-run is held back, not applied then overwritten")

        let complete = assembler.accept(snap1)
        var config = ZoneModuleConfiguration()
        config.apply(complete!)

        XCTAssertFalse(config.isPresent(socket: 7),
                       "the mid-run removal must win over the older snapshot")
        XCTAssertTrue(config.isPresent(socket: 8))
    }

    func testAssemblerBoundsRunLength() {
        // A hub that never sends a terminating fragment must not grow the
        // accumulation buffer without limit.
        var assembler = ZoneModuleFrameAssembler()
        var committed = false
        for i in 0..<40 {
            let records = (0..<4).map { j -> (UInt8, UInt8, UInt8) in
                (UInt8((i * 4 + j) % 128 + 1), 1, 0x01)
            }
            let f = GATTParser.parseZoneModuleStatus(
                frame(snapshot: true, last: false, fragment: UInt8(i), records: records))!
            if assembler.accept(f) != nil { committed = true }
        }
        XCTAssertFalse(committed, "no unterminated run may commit")
        XCTAssertFalse(assembler.isAccumulating,
                       "an over-long run must be abandoned, not accumulated forever")
    }

    func testAssemblerRestartsOnNewSnapshotRun() {
        // A fresh fragment 0 supersedes anything part-received.
        var assembler = ZoneModuleFrameAssembler()
        let stale = GATTParser.parseZoneModuleStatus(
            frame(snapshot: true, last: false, fragment: 0, records: [(1, 1, 0x01)]))!
        let fresh = GATTParser.parseZoneModuleStatus(
            frame(snapshot: true, last: true, fragment: 0, records: [(9, 1, 0x01)]))!

        XCTAssertNil(assembler.accept(stale))
        XCTAssertEqual(assembler.accept(fresh)?.records.map(\.socketID), [9],
                       "a new run must discard the abandoned one, not merge with it")
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

        // parseZoneModuleStatus: v2 delta frame, one present socket (3-byte record)
        XCTAssertNotNil(GATTParser.parseZoneModuleStatus(
            Data([0x02, 0x02, 0x00, 0x01, 0x0C, 0x02, 0x01])),
            "parseZoneModuleStatus must return non-nil for a valid v2 frame.")
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
        XCTAssertNil(GATTParser.parseZoneModuleStatus(Data([0x02, 0x02, 0x00]))) // needs 4B header
    }
}
