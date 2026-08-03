import XCTest
@testable import NeurOne

/// Socket-lattice targeting: zone resolution, the retired forms, and byte-level
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

    func testBitOrderIsLSBFirstAndZeroBased() throws {
        // App socket id 1 is firmware socket_id 0, which is bit 0 of byte 0.
        let mask = try NPSocketMask(sockets: [1], source: "test")
        XCTAssertEqual(mask.bytes[0], 0x01)
        XCTAssertEqual(mask.hexString, "01000000000000000000000000000000")

        let ninth = try NPSocketMask(sockets: [9], source: "test")
        XCTAssertEqual(ninth.bytes[0], 0x00)
        XCTAssertEqual(ninth.bytes[1], 0x01)
    }

    // MARK: - Retired forms parse but never resolve

    func testRetiredSelectorsFailWithTheirMigrationTarget() {
        let expected: [(NPRetiredZoneSelector, String)] = [
            (.all,   "All"),
            (.front, "Frontal"),
            (.rear,  "Posterior"),
        ]
        for (selector, replacement) in expected {
            XCTAssertThrowsError(try NPPBMTarget.retiredSelector(selector).resolve()) { error in
                let message = (error as? NPSocketTargetError)?.errorDescription ?? ""
                XCTAssertTrue(message.contains("'\(selector.rawValue)'"),
                              "message must quote the retired selector: \(message)")
                XCTAssertTrue(message.contains("\"\(replacement)\""),
                              "message must name the replacement zone: \(message)")
            }
        }
    }

    func testRetiredSelectorsAreNotSilentlyMigrated() {
        // `front` was slot mask 0x07 — frontal L/R PLUS parietal L — so mapping
        // it onto the "Frontal" zone would change which sockets light. Refusing
        // is the point; this test fails if someone "helpfully" makes it resolve.
        XCTAssertThrowsError(try NPPBMTarget.retiredSelector(.front).resolve())
        XCTAssertThrowsError(try NPPBMTarget.retiredSelector(.all).resolve())
    }

    func testRetiredNumericFailsAndQuotesTheIndices() {
        XCTAssertThrowsError(try NPPBMTarget.retiredNumeric([1, 2]).resolve()) { error in
            let message = (error as? NPSocketTargetError)?.errorDescription ?? ""
            XCTAssertTrue(message.contains("custom_zones: [1, 2]"), message)
            XCTAssertTrue(message.contains("ambiguous"),
                          "the message must say WHY it cannot be migrated: \(message)")
        }
        XCTAssertThrowsError(try NPPBMTarget.retiredNumeric([]).resolve())
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
