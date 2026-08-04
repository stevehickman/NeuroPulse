import XCTest
@testable import NeurOne

/// Socket-lattice targeting: zone resolution, socket numbering, and byte-level
/// agreement with the web encoder.
///
/// iOS is the component that signs and uploads the session protocol
/// (CLAUDE.md §4.2), so "the app produced a mask" is not the property that
/// matters — "the app produced the SAME mask the other producer would" is.
final class NPSocketMaskTests: XCTestCase {

    // MARK: - Named zone resolution

    func testNamedZoneResolvesToItsAuthoredSockets() throws {
        let mask = try NPPBMTarget.named(["Occipital Left"]).resolve()
        XCTAssertEqual(mask.socketIDs, [72, 73, 74, 77, 78],
                       "must be exactly the socket list 00-zones.npps authors for the zone")
    }

    func testAllZoneCoversEveryFittedSocket() throws {
        let mask = try NPPBMTarget.named(["All"]).resolve()
        XCTAssertEqual(mask.socketCount, SocketZones.socketCount)
    }

    func testDisjointZonesUnionToTheSumOfTheirSockets() throws {
        let left  = try NPPBMTarget.named(["Occipital Left"]).resolve()
        let right = try NPPBMTarget.named(["Occipital Right"]).resolve()
        // 74 is the midline socket, in both — so this is NOT 5 + 5.
        let both  = try NPPBMTarget.named(["Occipital Left", "Occipital Right"]).resolve()
        XCTAssertEqual(both, left.union(right))
        XCTAssertEqual(both.socketIDs, [72, 73, 74, 75, 76, 77, 78, 79, 80])
    }

    // MARK: - The midline, which is the whole reason this is a bitmap

    func testMidlineSocketInTwoZonesResolvesToOneBit() throws {
        // Socket 2 sits on the centre column, so 00-zones.npps lists it in BOTH
        // "Frontal Left" and "Frontal Right" by design. A list would carry it
        // twice and drive the same module twice: double J/cm² on one patch of
        // scalp. A bit cannot be set twice.
        XCTAssertTrue(SocketZones.zones(for: 2).contains("Frontal Left"))
        XCTAssertTrue(SocketZones.zones(for: 2).contains("Frontal Right"))

        let left  = try NPPBMTarget.named(["Frontal Left"]).resolve()
        let right = try NPPBMTarget.named(["Frontal Right"]).resolve()
        let both  = try NPPBMTarget.named(["Frontal Left", "Frontal Right"]).resolve()

        XCTAssertEqual(both.socketIDs.filter { $0 == 2 }.count, 1)
        XCTAssertLessThan(both.socketCount, left.socketCount + right.socketCount,
                          "shared midline sockets must not be counted twice")
        XCTAssertEqual(both.socketCount, 37)
    }

    func testRepeatingTheSameZoneChangesNothing() throws {
        let once  = try NPPBMTarget.named(["Frontal"]).resolve()
        let twice = try NPPBMTarget.named(["Frontal", "Frontal"]).resolve()
        XCTAssertEqual(once, twice)
    }

    // MARK: - Byte-level agreement with the web encoder

    /// Fixtures produced by running `hubCompiler.compileProtocol` on the same
    /// zone names and reading the 16-byte target block out of the emitted blob
    /// (offset 64 + 14). If iOS and web ever disagree about a zone, one of them
    /// is lighting the wrong part of a skull.
    func testMaskBytesMatchTheWebEncoder() throws {
        let fixtures: [(zones: [String], hex: String)] = [
            (["Occipital Left"],                 "00000000000000008033000000000000"),
            (["Frontal Left"],                   "3b1e0f1e1c0000000000000000000000"),
            (["Frontal Right"],                  "c6f1f0f0e00000000000000000000000"),
            (["Frontal Left", "Frontal Right"],  "fffffffefc0000000000000000000000"),
            (["All"],                            "ffffffffffffffffffff000000000000"),
        ]
        for fixture in fixtures {
            let mask = try NPPBMTarget.named(fixture.zones).resolve()
            XCTAssertEqual(mask.hexString, fixture.hex,
                           "mask for \(fixture.zones.joined(separator: " + ")) diverges from the web encoder")
        }
    }

    func testMaskIsSizedToTheFirmwareConstant() throws {
        // == NP_HUB_SOCKET_MASK_BYTES (firmware/hub_control/include/np_hub_config.h)
        // and == NP_PBM1064_SOCKET_MASK_BYTES. 128 bits, not 80: the wire format
        // covers the whole 7-bit socket domain, not what this shell wires.
        XCTAssertEqual(NPSocketMask.byteCount, 16)
        let mask = try NPPBMTarget.named(["All"]).resolve()
        XCTAssertEqual(mask.bytes.count, 16)
        XCTAssertEqual(mask.hexString.count, 32)
    }

    func testBitOrderIsLSBFirstWithBitZeroMeaningSocketOne() throws {
        // The bit position is INDEX space; the socket number is not. Socket 1 —
        // the lowest that exists — is bit 0 of byte 0.
        let mask = try NPSocketMask(sockets: [1], source: "test")
        XCTAssertEqual(mask.bytes[0], 0x01)
        XCTAssertEqual(mask.hexString, "01000000000000000000000000000000")

        let ninth = try NPSocketMask(sockets: [9], source: "test")
        XCTAssertEqual(ninth.bytes[0], 0x00)
        XCTAssertEqual(ninth.bytes[1], 0x01)
    }

    // MARK: - NUMBER-1: socket numbers are 1-based, project-wide

    func testNumberingBaseIsOne() {
        // NUMBER-1 (docs/np_hex_zm_001.md §3.3). Pinned here as well as in the
        // generator and the web suite because iOS holds its own copy of the
        // constant — a drift between the two would put every mask one tile off.
        XCTAssertEqual(NPSocketID.numberingBase, 1)
        XCTAssertEqual(NPSocketID.minimum, 1)
        XCTAssertEqual(NPSocketID.maximum, SocketZones.socketCount)
        XCTAssertEqual(NPSocketID.rangeLabel, "1–\(SocketZones.socketCount)")
    }

    func testSocketZeroDoesNotExist() {
        // Rejected, never clamped to 1 and never treated as "no socket".
        XCTAssertFalse(NPSocketID.isValid(0))
        XCTAssertEqual(NPSocketID.problem(0), .outOfRange)
        XCTAssertThrowsError(try NPSocketMask(sockets: [0], source: "test"))
    }

    func testEveryAuthoredZoneUsesOneBasedSocketNumbers() {
        // The generated table is the app's view of 00-zones.npps. If a 0-based
        // list ever reached it, every zone would resolve one tile off — silently,
        // because the ids would still all be in range.
        for name in SocketZones.zoneNames {
            for socket in SocketZones.sockets(forZone: name) ?? [] {
                XCTAssertGreaterThanOrEqual(Int(socket), NPSocketID.minimum,
                                            "zone \(name) names socket \(socket)")
                XCTAssertLessThanOrEqual(Int(socket), NPSocketID.maximum,
                                         "zone \(name) names socket \(socket)")
            }
        }
        XCTAssertEqual(SocketZones.bySocket.keys.min(), 1)
        XCTAssertEqual(Int(SocketZones.bySocket.keys.max() ?? 0), SocketZones.socketCount)
    }

    func testTheLowestAndHighestSocketsBothAddress() throws {
        // The two ends are where an off-by-one shows up first: socket 1 must not
        // fall off the bottom, and the last socket must not overflow the mask.
        let lowest = try NPSocketMask(sockets: [NPSocketID.minimum], source: "test")
        XCTAssertEqual(lowest.socketIDs, [1])
        let highest = try NPSocketMask(sockets: [NPSocketID.maximum], source: "test")
        XCTAssertEqual(highest.socketIDs, [SocketZones.socketCount])
        XCTAssertEqual(highest.bytes.count, NPSocketMask.byteCount)
    }

    // MARK: - Bad targets

    func testUnknownZoneNameFails() {
        XCTAssertThrowsError(try NPPBMTarget.named(["Left Frontal"]).resolve()) { error in
            let message = (error as? NPSocketTargetError)?.errorDescription ?? ""
            XCTAssertTrue(message.contains("\"Left Frontal\""),
                          "message must quote the zone that was not found: \(message)")
        }
    }

    func testEmptyZoneListFails() {
        XCTAssertThrowsError(try NPPBMTarget.named([]).resolve())
    }

    func testClinicianSelectedNeedsAnOperatorSelection() throws {
        XCTAssertThrowsError(try NPPBMTarget.clinicianSelected.resolve())
        XCTAssertThrowsError(try NPPBMTarget.clinicianSelected.resolve(clinicianSockets: []))

        let mask = try NPPBMTarget.clinicianSelected.resolve(clinicianSockets: [4, 6, 21])
        XCTAssertEqual(mask.socketIDs, [4, 6, 21])
    }

    func testSocketIdsOffThisLatticeAreRejected() {
        for bad in [0, -1, SocketZones.socketCount + 1, 128] {
            XCTAssertThrowsError(
                try NPSocketMask(sockets: [bad], source: "test"),
                "socket \(bad) must not be addressable"
            ) { error in
                let message = (error as? NPSocketTargetError)?.errorDescription ?? ""
                XCTAssertTrue(message.contains("\(bad)"), message)
            }
        }
    }

    // MARK: - Codable round trip

    func testMaskRoundTripsThroughJSON() throws {
        let mask = try NPPBMTarget.named(["Motor / SMA"]).resolve()
        let data = try JSONEncoder().encode(mask)
        XCTAssertEqual(String(data: data, encoding: .utf8), "\"\(mask.hexString)\"")
        XCTAssertEqual(try JSONDecoder().decode(NPSocketMask.self, from: data), mask)
    }

    func testWrongLengthMaskIsRejectedOnDecode() {
        let short = Data("\"00ff\"".utf8)
        XCTAssertThrowsError(try JSONDecoder().decode(NPSocketMask.self, from: short))
        let notHex = Data("\"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz\"".utf8)
        XCTAssertThrowsError(try JSONDecoder().decode(NPSocketMask.self, from: notHex))
    }

    // MARK: - Generated zone table

    func testZoneTableAgreesWithItselfInBothDirections() {
        for name in SocketZones.zoneNames {
            let sockets = SocketZones.sockets(forZone: name)
            XCTAssertNotNil(sockets, "\(name) is listed in zoneNames but absent from byZone")
            XCTAssertFalse(sockets!.isEmpty, "\(name) is empty")
            for socket in sockets! {
                XCTAssertTrue(SocketZones.zones(for: socket).contains(name),
                              "socket \(socket) is in byZone[\(name)] but not in bySocket[\(socket)]")
            }
        }
    }

    func testZoneTableListsNoSocketTwice() {
        for name in SocketZones.zoneNames {
            let sockets = SocketZones.sockets(forZone: name) ?? []
            XCTAssertEqual(sockets.count, Set(sockets).count, "\(name) lists a socket twice")
        }
    }
}
