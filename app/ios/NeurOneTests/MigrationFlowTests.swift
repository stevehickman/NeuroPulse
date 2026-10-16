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

private final class KeyStore: KeyValueStore {
    var values: [String: String] = [:]
    func getString(_ key: String) -> String? { values[key] }
    func putString(_ key: String, _ value: String) { values[key] = value }
    func remove(_ key: String) { values.removeValue(forKey: key) }
}

/// The app's files in memory, with every call recorded in order and one call that can be made to throw.
private final class MemoryFiles: FileStore {
    struct Broken: Error {}
    var files: [String: [UInt8]] = [:]
    var events: [String] = []
    var failOn: String?
    private func check(_ call: String, _ path: String) throws { if failOn == "\(call):\(path)" { throw Broken() } }
    func list() async throws -> [StoredFile] { files.sorted { $0.key < $1.key }.map { StoredFile(path: $0.key, size: Int64($0.value.count)) } }
    func write(_ path: String, offset: Int64, bytes: Data) async throws {
        try check("write", path)
        files[path]?.replaceSubrange(Int(offset)..<Int(offset) + bytes.count, with: bytes)
        events.append("write:\(path):\(offset):\(bytes.count)")
    }
    func sync(_ path: String) async throws { try check("sync", path); events.append("sync:\(path)") }
    func delete(_ path: String) async throws { try check("delete", path); files.removeValue(forKey: path); events.append("delete:\(path)") }
}

/// A provider with named objects and a create-if-absent the store itself arbitrates.
private final class MemoryStore: ObjectStore {
    struct Broken: Error {}
    var objects: [String: String] = [:]
    var conditional = true
    var space: Int64? = 1 << 30
    var failOn: String?
    private func check(_ call: String) throws { if failOn == call { throw Broken() } }
    func get(_ name: String) async throws -> String? { try check("get"); return objects[name] }
    func put(_ name: String, _ data: String) async throws { try check("put"); objects[name] = data }
    func createIfAbsent(_ name: String, _ data: String) async throws -> Bool {
        try check("create")
        if conditional && objects[name] != nil { return false }
        objects[name] = data
        return true
    }
    func delete(_ name: String) async throws { try check("delete"); objects.removeValue(forKey: name) }
    func list(_ prefix: String) async throws -> [String] { try check("list"); return objects.keys.filter { $0.hasPrefix(prefix) } }
    func freeSpace() async throws -> Int64? { try check("space"); return space }
}

/// A route and a phone's randomness in memory: dumb storage that performs whatever effect the driver does not answer.
private final class Host: MigrationHost {
    var seen: [String] = []
    var ran: [String] = []
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
        case "runClearStep":
            ran.append(e["step"] as? String ?? "")
            return ["ok": true]
        case "listFiles":
            return ["files": [[String: Any]]()]
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
        for key in carried.values.flatMap({ $0 }) { store.putString(key, "value-of-\(key)") }
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

    // @verifies SWR-MG-08
    // @verifies SWR-MG-09
    func testTheOldPhoneClearsThroughTheSharedFlowAndTheHostDoesOnlyItsOwnSteps() async throws {
        let route = Route()
        let oldStore = try filledStore()
        oldStore.putString("ble.last-hub-id", "hub-1")
        let done = try await MigrationFlow.run(
            "backup", params: params(["kind": "migrate", "method": "passphrase"]),
            host: Host(route: route, seed: 1), secret: try secret("a long enough passphrase"), keys: oldStore)
        let clearParams: [String: Any] = ["bundleHash": done["bundleHash"] as? String ?? "", "canRemoveOsBond": false]

        let before = try await MigrationFlow.run("clear", params: clearParams, host: Host(route: route, seed: 1), keys: oldStore)
        XCTAssertEqual(before["reason"] as? String, "no_confirmation")

        _ = try await MigrationFlow.run(
            "restore", params: params(), host: Host(route: route, seed: 50),
            secret: try secret("a long enough passphrase"), keys: KeyStore())
        let host = Host(route: route, seed: 1)
        let after = try await MigrationFlow.run("clear", params: clearParams, host: host, keys: oldStore)
        XCTAssertEqual(after["ok"] as? Bool, true)
        XCTAssertEqual(after["removeOsBond"] as? Bool, true)
        XCTAssertEqual(host.ran, ["destroyEncryptionKey", "forgetPairingRecord"])
        XCTAssertEqual(Array(oldStore.values.keys), ["np.migration.cleared"])
    }

    // @verifies SWR-MG-10
    // @verifies SWR-MG-11
    // @verifies SWR-MG-07
    func testAMigrationRunsOverAnObjectStoreTheAppOnlyHasToProvideAndASecondRestoreIsRefused() async throws {
        let store = MemoryStore()
        let oldStore = try filledStore()
        let done = try await MigrationFlow.run(
            "backup", params: params(["kind": "migrate", "method": "passphrase"]),
            host: Host(route: Route(), seed: 1), secret: try secret("a long enough passphrase"), keys: oldStore, store: store)
        XCTAssertEqual(done["ok"] as? Bool, true)
        XCTAssertEqual(Array(store.objects.keys), [try MigrationFlow.objectNames()["bundle"] as? String ?? ""])

        let freshStore = KeyStore()
        let host = Host(route: Route(), seed: 50)
        let r = try await MigrationFlow.run(
            "restore", params: params(), host: host, secret: try secret("a long enough passphrase"), keys: freshStore, store: store)
        XCTAssertEqual(r["ok"] as? Bool, true)
        XCTAssertEqual(r["confirmed"] as? Bool, true)
        XCTAssertEqual(freshStore.values, oldStore.values)
        XCTAssertTrue(host.seen.allSatisfy { !["getBundle", "putBundle", "readClaim", "createClaim"].contains($0) })

        let second = try await MigrationFlow.run(
            "restore", params: params(), host: Host(route: Route(), seed: 90),
            secret: try secret("a long enough passphrase"), keys: KeyStore(), store: store)
        XCTAssertEqual(second["reason"] as? String, "claimed")
    }

    // @verifies SWR-MG-11
    func testAnObjectCallThatThrowsEndsTheFlowWithTheRouteUnavailable() async throws {
        let store = MemoryStore()
        store.failOn = "put"
        let r = try await MigrationFlow.run(
            "backup", params: params(["kind": "migrate", "method": "passphrase"]),
            host: Host(route: Route(), seed: 1), secret: try secret("a long enough passphrase"), keys: try filledStore(), store: store)
        XCTAssertEqual(r["reason"] as? String, "route_unavailable")
    }

    // @verifies SWR-MG-12
    func testTheProbeSaysWhatAnAccountCanDoAndLeavesNothingBehind() async throws {
        let good = MemoryStore()
        good.objects["users-file"] = "01"
        var r = try await MigrationFlow.probe(store: good, bundleBytes: 1000, host: Host(route: Route(), seed: 7))
        XCTAssertEqual(r["result"] as? String, "sufficient")
        XCTAssertEqual(Array(good.objects.keys), ["users-file"])

        let noConditional = MemoryStore()
        noConditional.conditional = false
        r = try await MigrationFlow.probe(store: noConditional, bundleBytes: 1000, host: Host(route: Route(), seed: 7))
        XCTAssertEqual(r["result"] as? String, "backup_only")
        XCTAssertEqual(r["failedOperation"] as? String, "createIfAbsentSecondRefused")
        XCTAssertTrue(noConditional.objects.isEmpty)

        let small = MemoryStore()
        small.space = 10
        r = try await MigrationFlow.probe(store: small, bundleBytes: 1000, host: Host(route: Route(), seed: 7))
        XCTAssertEqual(r["failedOperation"] as? String, "space")
    }

    // @verifies SWR-MG-06
    // @verifies SWR-MG-08
    func testTheFilesAreOverwrittenInFullSyncedAndDeletedAfterTheKeyAndAFailedCallResumesAtTheNextLaunch() async throws {
        let route = Route()
        let oldStore = try filledStore()
        let done = try await MigrationFlow.run(
            "backup", params: params(["kind": "migrate", "method": "passphrase"]),
            host: Host(route: route, seed: 1), secret: try secret("a long enough passphrase"), keys: oldStore)
        _ = try await MigrationFlow.run(
            "restore", params: params(), host: Host(route: route, seed: 50),
            secret: try secret("a long enough passphrase"), keys: KeyStore())
        let clearParams: [String: Any] = ["bundleHash": done["bundleHash"] as? String ?? "", "canRemoveOsBond": true]

        let files = MemoryFiles()
        files.files = ["a-big.db": [UInt8](repeating: 0x11, count: 40_000), "b-empty.db": [], "c-small.db": [UInt8](repeating: 0x22, count: 100)]
        files.failOn = "delete:c-small.db"
        let host = Host(route: route, seed: 1)
        let r1 = try await MigrationFlow.run("clear", params: clearParams, host: host, keys: oldStore, files: files)
        XCTAssertEqual(r1["ok"] as? Bool, false)
        XCTAssertEqual(r1["step"] as? String, "overwriteAndDeleteFiles")
        XCTAssertEqual(Array(files.files.keys), ["c-small.db"], "what was not reached is left")
        XCTAssertEqual(host.ran, ["destroyEncryptionKey"])

        let writes = files.events.filter { $0.hasPrefix("write:a-big.db:") }.map { $0.split(separator: ":").map { Int($0) ?? 0 } }
        XCTAssertEqual(writes.reduce(0) { $0 + $1[3] }, 40_000, "every byte of the big file was written")
        XCTAssertTrue(writes.allSatisfy { $0[3] <= 16 * 1024 }, "in chunks")
        XCTAssertLessThan(try XCTUnwrap(files.events.firstIndex(of: "sync:a-big.db")), try XCTUnwrap(files.events.firstIndex(of: "delete:a-big.db")))
        XCTAssertEqual(files.events.filter { $0.contains("b-empty.db") }, ["delete:b-empty.db"], "an empty file is only deleted")

        files.failOn = nil
        let second = Host(route: Route(), seed: 1)
        let r2 = try await MigrationFlow.run("clear", params: clearParams, host: second, keys: oldStore, files: files)
        XCTAssertEqual(r2["ok"] as? Bool, true)
        XCTAssertTrue(files.files.isEmpty)
        XCTAssertEqual(second.ran, ["forgetPairingRecord"], "the key is not destroyed twice")
    }

    // @verifies SWR-MG-01
    func testKeysThatBelongToTheInstallNeverMove() async throws {
        let route = Route()
        let oldStore = try filledStore()
        let installKeys = ["np.research.device-token", "np.warranty.consent-granted", "ble.last-hub-id", "np.consumable.snooze-counts"]
        for k in installKeys { oldStore.putString(k, "belongs-to-this-phone") }
        _ = try await MigrationFlow.run(
            "backup", params: params(["kind": "migrate", "method": "passphrase"]),
            host: Host(route: route, seed: 1), secret: try secret("a long enough passphrase"), keys: oldStore)
        let freshStore = KeyStore()
        _ = try await MigrationFlow.run(
            "restore", params: params(), host: Host(route: route, seed: 50),
            secret: try secret("a long enough passphrase"), keys: freshStore)
        for k in installKeys { XCTAssertNil(freshStore.getString(k), "\(k) must not move") }
        XCTAssertEqual(freshStore.getString("np.session.history"), "value-of-np.session.history")
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
