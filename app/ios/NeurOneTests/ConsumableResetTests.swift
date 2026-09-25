//
//  ConsumableResetTests.swift
//  NeurOneTests
//
//  OI-ACC-08 (GitHub #381): the CONSUMABLE_STATUS reset write and its offline queue. The wire
//  value is the one the hub's np_cons_on_reset_write() accepts (np_consumables_tests); the
//  Android twin is ConsumableResetTests.kt.

import XCTest
@testable import NeurOne

final class ConsumableResetTests: XCTestCase {

    private var defaults: UserDefaults!

    override func setUp() {
        super.setUp()
        defaults = UserDefaults(suiteName: "ConsumableResetTests")
        defaults.removePersistentDomain(forName: "ConsumableResetTests")
    }

    func testWireIsTheKindIndexForTheFourCountedKinds() {
        for kind in ConsumableKind.allCases {
            XCTAssertEqual(ConsumableResetWire.encode(kind.rawValue), Data([UInt8(kind.rawValue)]))
        }
        XCTAssertNil(ConsumableResetWire.encode(-1))
        XCTAssertNil(ConsumableResetWire.encode(ConsumableKind.allCases.count))
    }

    func testQueueSurvivesARestartAndDrainsOnce() {
        let q = ConsumableResetQueue(defaults: defaults)
        q.add(3); q.add(0); q.add(3); q.add(7)

        let restarted = ConsumableResetQueue(defaults: defaults)
        XCTAssertEqual(restarted.pending, [0, 3], "Deduplicated, the unknown kind dropped.")

        var written: [UInt8] = []
        restarted.drain { written.append($0[0]) }
        XCTAssertEqual(written, [0, 3])
        XCTAssertTrue(restarted.pending.isEmpty)

        restarted.drain { _ in written.append(0xFF) }
        XCTAssertEqual(written, [0, 3], "Nothing left: no second write.")
    }
}
