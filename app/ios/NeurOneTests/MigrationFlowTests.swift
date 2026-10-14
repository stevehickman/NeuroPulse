//
//  MigrationFlowTests.swift
//  NeurOneTests
//
//  The backup and device-replacement flows through the iOS driver and the C ABI binding. The Rust tests hold the ORDER of
//  the flow once for every app (migration_flow.rs); this holds the iOS path: the driver loop and the effect shapes the
//  host sees. The same cases run on Kotlin, Windows and the web.
//
//  Subject under test: MigrationFlow (app/ios/NeurOne/Session/MigrationFlow.swift)

import XCTest
@testable import NeurOne

private let hostItems = ["protocols", "sessionHistory", "limitsProfiles", "characterisationEnrolment"]
private let routeJSON: [String: Any] = ["id": "memory", "atomicClaim": true, "direct": false, "authenticatedPairing": true]

private final class Route {
    var bundle: String?
    var claim: [String: Any]?
    var confirmation: [String: Any]?
}

/// A phone and a route in memory: dumb storage that performs whatever effect it is asked for.
private final class Host: MigrationHost {
    var items: [String: Any] = [:]
    var seen: [String] = []
    private let route: Route
    private let secret: String
    private var counter: Int

    init(route: Route, secret: String, seed: Int, filled: Bool) {
        self.route = route
        self.secret = secret
        self.counter = seed
        if filled { for n in hostItems { items[n] = ["np.\(n)": "value-of-\(n)"] } }
    }

    private func hex(_ bytes: [UInt8]) -> String { bytes.map { String(format: "%02x", $0) }.joined() }

    func perform(_ e: [String: Any]) async throws -> [String: Any] {
        let type = e["type"] as? String ?? ""
        seen.append(type)
        switch type {
        case "random":
            let n = e["bytes"] as? Int ?? 0
            return ["bytes": hex((0..<n).map { _ in counter = (counter * 31 + 7) & 255; return UInt8(counter) })]
        case "deriveKey":
            // Not Argon2: a stand-in that depends on the secret and the salt, as the real one does.
            let text = Array((secret + (((e["kdf"] as? [String: Any])?["salt"] as? String) ?? "")).utf8)
            return ["key": hex((0..<32).map { UInt8(truncatingIfNeeded: Int(text[$0 % text.count]) * 7 + $0) })]
        case "readItems":
            var found: [String: Any] = [:]
            for name in (e["names"] as? [String]) ?? [] { if let v = items[name] { found[name] = v } }
            return ["items": found]
        case "applyItem":
            items[e["name"] as? String ?? ""] = e["value"]
            return ["ok": true]
        case "getBundle":
            return ["bundle": route.bundle as Any? ?? NSNull()]
        case "putBundle":
            route.bundle = e["bundle"] as? String
            return ["ok": true]
        case "readClaim":
            return ["claim": route.claim as Any? ?? NSNull()]
        case "createClaim":
            let created = route.claim == nil
            if created { route.claim = e["claim"] as? [String: Any] }
            return ["created": created]
        case "writeConfirmation":
            route.confirmation = e["confirmation"] as? [String: Any]
            return ["ok": true]
        case "readConfirmation":
            return ["confirmation": route.confirmation as Any? ?? NSNull()]
        default:
            throw NppsCore.Refusal(message: "unknown effect \(type)")
        }
    }
}

final class MigrationFlowTests: XCTestCase {

    private func params(_ extra: [String: Any] = [:]) -> [String: Any] {
        ["route": routeJSON, "hostItems": hostItems].merging(extra) { _, new in new }
    }

    // @verifies SWR-MG-02
    // @verifies SWR-MG-08
    func testAMigrationMovesAPhoneThroughTheSharedFlowAndEndsWithAConfirmationOfTheBundleHash() async throws {
        let route = Route()
        let old = Host(route: route, secret: "a long enough passphrase", seed: 1, filled: true)
        let done = try await MigrationFlow.run("backup", params: params(["kind": "migrate", "method": "passphrase"]), host: old)
        XCTAssertEqual(done["ok"] as? Bool, true)

        let fresh = Host(route: route, secret: "a long enough passphrase", seed: 50, filled: false)
        let r = try await MigrationFlow.run("restore", params: params(), host: fresh)
        XCTAssertEqual(r["ok"] as? Bool, true)
        XCTAssertEqual(r["confirmed"] as? Bool, true)
        XCTAssertTrue((old.items as NSDictionary).isEqual(to: fresh.items))
        XCTAssertEqual(route.confirmation?["bundleHash"] as? String, done["bundleHash"] as? String)
    }

    // @verifies SWR-MG-07
    func testAWrongSecretOpensNothingAndLeavesNoClaimBehind() async throws {
        let route = Route()
        let old = Host(route: route, secret: "a long enough passphrase", seed: 1, filled: true)
        _ = try await MigrationFlow.run("backup", params: params(["kind": "migrate", "method": "passphrase"]), host: old)
        let wrong = Host(route: route, secret: "a different passphrase", seed: 50, filled: false)
        let r = try await MigrationFlow.run("restore", params: params(), host: wrong)
        XCTAssertEqual(r["ok"] as? Bool, false)
        XCTAssertEqual(r["reason"] as? String, "authentication_failed")
        XCTAssertNil(route.claim)
        XCTAssertFalse(wrong.seen.contains("createClaim"))
    }
}
