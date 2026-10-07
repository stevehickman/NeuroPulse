//
//  NPZoneCautionTests.swift
//  NeurOneTests
//
//  The caution list behind the acknowledgement screen (docs/reference/safety-zones.md): one entry per axis in its
//  caution band, carrying the id the compiler checks. The zones come from the shared core; the parse is the only
//  logic this app adds.
//
//  Subject under test: NPZoneCaution.parse (app/ios/NeurOne/Protocol/NPProtocolValidator.swift)

import XCTest
@testable import NeurOne

final class NPZoneCautionTests: XCTestCase {

    private let reply: [[String: Any]] = [[
        "index": 1, "modality": "tdcs", "zone": "caution",
        "axes": [
            ["id": "sessionChargeDensity", "nameKey": "ZONE_AXIS_SESSION_CHARGE_DENSITY", "unit": "mC/cm²",
             "value": 102.857, "caution": 100.0, "danger": 150.0, "zone": "caution",
             "ackId": "1:tdcs:sessionChargeDensity=102.857"],
            ["id": "other", "nameKey": "ZONE_AXIS_SHANNON_K", "unit": "", "value": 0.2, "caution": 1.0,
             "danger": NSNull(), "zone": "safe", "ackId": NSNull()]
        ]
    ]]

    func testOnlyAnAxisInItsCautionBandIsListed() {
        let cautions = NPZoneCaution.parse(reply)
        XCTAssertEqual(cautions.count, 1)
        XCTAssertEqual(cautions[0].ackId, "1:tdcs:sessionChargeDensity=102.857")
        XCTAssertEqual(cautions[0].modality, .tdcs)
        XCTAssertEqual(cautions[0].axisNameKey, "ZONE_AXIS_SESSION_CHARGE_DENSITY")
        XCTAssertEqual(cautions[0].value, 102.857, accuracy: 1e-9)
        XCTAssertEqual(cautions[0].caution, 100.0, accuracy: 1e-9)
    }

    func testASafeProtocolHasNoCautions() {
        XCTAssertTrue(NPZoneCaution.parse([]).isEmpty)
    }
}
