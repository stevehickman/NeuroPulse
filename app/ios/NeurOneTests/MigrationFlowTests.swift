//
//  MigrationFlowTests.swift
//  NeurOneTests
//
//  The backup and device-replacement flows through the iOS driver and the C ABI binding. The Rust tests hold the ORDER of
//  the flow once for every app (migration_flow.rs); this holds the iOS path: the driver loop, the effects it answers itself
//  (the core's Argon2id key, the key-value store) and the effect shapes the host sees. The same cases run on Kotlin,
//  Windows and the web.
//
//  Subject under test: MigrationFlow (app/ios/NeurOne/Session/MigrationFlow.swift)

import XCTest
@testable import NeurOne

private let routeJSON: [String: Any] = ["id": "memory", "atomicClaim": true, "direct": false, "authenticatedPairing": true]

private final class Route {
    var bundle: String?
    var claim: [String: Any]?
    var confirmation: [String: Any]?
}

private final class KeyStore: MigrationKeyStore {
    var values: [String: String] = [:]
    func get(_ key: String) -> String? { values[key] }
    func put(_ key: String, _ value: String) { values[key] = value }
    func remove(_ key: String) { values.removeValue(forKey: key) }
}

/// A route and a phone's randomness in memory: dumb storage that performs whatever effect the driver does not answer.
private final class Host: MigrationHost {
    var seen: [String] = []
    private let route: Route
    private var counter: Int

    init(route: Route, seed: Int) {
        self.route = route
        self.counter = seed
    }

    private func hex(_ bytes: [UInt8]) -> String { bytes.map { String(format: "%02x", $0) }.joined() }

    func perform(_ e: [String: Any]) async throws -> [String: Any] {
        let type = e["type"] as? String ?? ""
        seen.append(type)
        switch type {
        case "random":
            let n = e["bytes"] as? Int ?? 0
            return ["bytes": hex((0..<n).map { _ in counter = (counter * 31 + 7) & 255; return UInt8(counter) })]
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

    private func secret(_ text: String) throws -> BundleSecret {
        guard case .ok(let s) = try BundleKeyDerivation.passphraseSecret(text) else { throw NppsCore.Refusal(message: "passphrase refused") }
        return s
    }

    private func filledStore() throws -> KeyStore {
        let store = KeyStore()
        let carried = try NppsCore.migration(["op": "itemKeys"])["carried"] as? [String: [String]] ?? [:]
        for key in carried.values.flatMap({ $0 }) { store.put(key, "value-of-\(key)") }
        return store
    }

    private func params(_ extra: [String: Any] = [:]) -> [String: Any] {
        ["route": routeJSON].merging(extra) { _, new in new }
    }

    // @verifies SWR-MG-02
    // @verifies SWR-MG-08
    func testAMigrationMovesAPhoneThroughTheSharedFlowAndEndsWithAConfirmationOfTheBundleHash() async throws {
        let route = Route()
        let oldStore = try filledStore()
        let done = try await MigrationFlow.run(
            "backup", params: params(["kind": "migrate", "method": "passphrase"]),
            host: Host(route: route, seed: 1), secret: try secret("a long enough passphrase"), keys: oldStore)
        XCTAssertEqual(done["ok"] as? Bool, true)

        let freshStore = KeyStore()
        let r = try await MigrationFlow.run(
            "restore", params: params(), host: Host(route: route, seed: 50),
            secret: try secret("a long enough passphrase"), keys: freshStore)
        XCTAssertEqual(r["ok"] as? Bool, true)
        XCTAssertEqual(r["confirmed"] as? Bool, true)
        XCTAssertEqual(freshStore.values, oldStore.values)
        XCTAssertEqual(route.confirmation?["bundleHash"] as? String, done["bundleHash"] as? String)
    }

    // @verifies SWR-MG-01
    func testKeysThatBelongToTheInstallNeverMove() async throws {
        let route = Route()
        let oldStore = try filledStore()
        let installKeys = ["np.research.device-token", "np.warranty.consent-granted", "ble.lastHubId", "np.consumable.snooze-counts"]
        for k in installKeys { oldStore.put(k, "belongs-to-this-phone") }
        _ = try await MigrationFlow.run(
            "backup", params: params(["kind": "migrate", "method": "passphrase"]),
            host: Host(route: route, seed: 1), secret: try secret("a long enough passphrase"), keys: oldStore)
        let freshStore = KeyStore()
        _ = try await MigrationFlow.run(
            "restore", params: params(), host: Host(route: route, seed: 50),
            secret: try secret("a long enough passphrase"), keys: freshStore)
        for k in installKeys { XCTAssertNil(freshStore.get(k), "\(k) must not move") }
        XCTAssertEqual(freshStore.get("np.session.history"), "value-of-np.session.history")
    }

    // @verifies SWR-MG-07
    func testAWrongSecretOpensNothingAndLeavesNoClaimBehind() async throws {
        let route = Route()
        _ = try await MigrationFlow.run(
            "backup", params: params(["kind": "migrate", "method": "passphrase"]),
            host: Host(route: route, seed: 1), secret: try secret("a long enough passphrase"), keys: try filledStore())
        let wrong = Host(route: route, seed: 50)
        let r = try await MigrationFlow.run(
            "restore", params: params(), host: wrong, secret: try secret("a different passphrase"), keys: KeyStore())
        XCTAssertEqual(r["ok"] as? Bool, false)
        XCTAssertEqual(r["reason"] as? String, "authentication_failed")
        XCTAssertNil(route.claim)
        XCTAssertFalse(wrong.seen.contains("createClaim"))
    }
}
