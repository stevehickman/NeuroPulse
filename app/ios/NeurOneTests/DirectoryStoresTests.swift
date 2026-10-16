//
//  DirectoryStoresTests.swift
//  NeurOneTests
//
//  The folder-backed stores on a real disk: the route for a local computer or a USB drive, and the app's files for the old
//  phone's clear (SWR-MG-06, SWR-MG-07, SWR-MG-10, SWR-MG-12). The same cases run on Kotlin and C#.
//
//  Subject under test: DirectoryObjectStore, DirectoryFileStore (app/ios/NeurOne/Session/DirectoryStores.swift)

import XCTest
@testable import NeurOne

private final class NoRoute: MigrationHost {
    private var counter: Int
    init(seed: Int) { counter = seed }
    func perform(_ e: [String: Any]) async throws -> [String: Any] {
        guard e["type"] as? String == "random" else { throw NppsCore.Refusal(message: "unknown effect \(e["type"] ?? "")") }
        let count = e["bytes"] as? Int ?? 0
        let bytes = (0..<count).map { _ -> UInt8 in counter = (counter * 31 + 7) & 255; return UInt8(counter) }
        return ["bytes": bytes.map { String(format: "%02x", $0) }.joined()]
    }
}

final class DirectoryStoresTests: XCTestCase {

    private func tempDirectory() throws -> URL {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("np-dir-store-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        addTeardownBlock { try? FileManager.default.removeItem(at: url) }
        return url
    }

    private func secret() throws -> BundleSecret {
        guard case .ok(let s) = try BundleKeyDerivation.passphraseSecret("a long enough passphrase") else {
            throw NppsCore.Refusal(message: "passphrase refused")
        }
        return s
    }

    // @verifies SWR-MG-07
    func testOfManySimultaneousCreatesExactlyOneWinsAndItsContentIsWhole() async throws {
        let store = try DirectoryObjectStore(root: try tempDirectory())
        let results = try await withThrowingTaskGroup(of: Bool.self) { group -> [Bool] in
            for i in 1...24 {
                group.addTask { try await store.createIfAbsent("claim.json", String(repeating: String(format: "%02x", i), count: 4096)) }
            }
            var all: [Bool] = []
            for try await won in group { all.append(won) }
            return all
        }
        XCTAssertEqual(results.filter { $0 }.count, 1, "the operating system arbitrates: one create succeeds")
        let content = try await store.get("claim.json") ?? ""
        XCTAssertEqual(content.count, 8192)
        XCTAssertEqual(Set(content.chunked(2)).count, 1, "the claim is one writer's, whole")
        let listed = try await store.list("")
        XCTAssertEqual(listed, ["claim.json"], "no temporary file is left")
    }

    // @verifies SWR-MG-11
    func testAnObjectIsWrittenReplacedReadListedAndDeletedAndANameCannotLeaveTheFolder() async throws {
        let store = try DirectoryObjectStore(root: try tempDirectory())
        let none = try await store.get("a.bin")
        XCTAssertNil(none)
        try await store.put("a.bin", "010203")
        try await store.put("a.bin", "09")
        let replaced = try await store.get("a.bin")
        XCTAssertEqual(replaced, "09")
        try await store.put("b.bin", "")
        let all = try await store.list("")
        XCTAssertEqual(all, ["a.bin", "b.bin"])
        let only = try await store.list("b")
        XCTAssertEqual(only, ["b.bin"])
        try await store.delete("a.bin")
        try await store.delete("a.bin")
        let gone = try await store.get("a.bin")
        XCTAssertNil(gone)
        let space = try await store.freeSpace() ?? 0
        XCTAssertGreaterThan(space, 0)
        for bad in ["../x", "a/b", ".hidden", "", "a\\b", "é"] {
            do {
                _ = try await store.get(bad)
                XCTFail("'\(bad)' must be refused")
            } catch {}
        }
    }

    // @verifies SWR-MG-12
    func testAFolderOnDiskPassesTheAccountCheckAndTheProbeLeavesItEmpty() async throws {
        let dir = try tempDirectory()
        let result = try await MigrationFlow.probe(
            store: try DirectoryObjectStore(root: dir), bundleBytes: 1_000_000, host: NoRoute(seed: 7))
        XCTAssertEqual(result["result"] as? String, "sufficient")
        XCTAssertEqual(try FileManager.default.contentsOfDirectory(atPath: dir.path), [], "the probe leaves nothing behind")
    }

    // @verifies SWR-MG-06
    func testTheFileStoreOverwritesInPlaceAndWillNotLeaveItsFolder() async throws {
        let dir = try tempDirectory()
        let files = DirectoryFileStore(root: dir)
        try Data(repeating: 1, count: 10).write(to: dir.appendingPathComponent("x.db"))
        let listed = try await files.list()
        XCTAssertEqual(listed.map { $0.path }, ["x.db"])
        XCTAssertEqual(listed.map { $0.size }, [10])
        try await files.write("x.db", offset: 4, bytes: Data([7, 7]))
        try await files.sync("x.db")
        XCTAssertEqual([UInt8](try Data(contentsOf: dir.appendingPathComponent("x.db"))), [1, 1, 1, 1, 7, 7, 1, 1, 1, 1])
        try await files.delete("x.db")
        try await files.delete("x.db")
        XCTAssertFalse(FileManager.default.fileExists(atPath: dir.appendingPathComponent("x.db").path))
        for bad in ["../x.db", "/etc/passwd", ""] {
            do {
                try await files.delete(bad)
                XCTFail("'\(bad)' must be refused")
            } catch {}
        }
    }

    // @verifies SWR-MG-10
    // @verifies SWR-MG-07
    // @verifies SWR-MG-06
    func testAMigrationOverAFolderOnDiskMovesThePhoneAndTheClearOverwritesAndDeletesRealFiles() async throws {
        let folder = try DirectoryObjectStore(root: try tempDirectory())
        let route: [String: Any] = ["id": "folder", "atomicClaim": true, "direct": false, "authenticatedPairing": true]
        let oldKv = MemoryKeys()
        oldKv.putString("np.session.history", "history of the old phone")
        let backup = try await MigrationFlow.run(
            "backup", params: ["kind": "migrate", "method": "passphrase", "route": route, "hostItems": [String]()],
            host: NoRoute(seed: 1), secret: try secret(), keys: oldKv, store: folder)
        XCTAssertEqual(backup["ok"] as? Bool, true)

        let newKv = MemoryKeys()
        let restore = try await MigrationFlow.run(
            "restore", params: ["route": route, "hostItems": [String]()], host: NoRoute(seed: 50),
            secret: try secret(), keys: newKv, store: folder)
        XCTAssertEqual(restore["ok"] as? Bool, true)
        XCTAssertEqual(newKv.getString("np.session.history"), "history of the old phone")
        let second = try await MigrationFlow.run(
            "restore", params: ["route": route, "hostItems": [String]()], host: NoRoute(seed: 90),
            secret: try secret(), keys: MemoryKeys(), store: folder)
        XCTAssertEqual(second["reason"] as? String, "claimed")

        let dataDir = try tempDirectory()
        try FileManager.default.createDirectory(at: dataDir.appendingPathComponent("uhdr"), withIntermediateDirectories: true)
        try Data(repeating: 0x5A, count: 40_000).write(to: dataDir.appendingPathComponent("uhdr/sessions.db"))
        try Data().write(to: dataDir.appendingPathComponent("settings.db"))
        let clear = try await MigrationFlow.run(
            "clear", params: ["bundleHash": backup["bundleHash"] as? String ?? "", "canRemoveOsBond": true],
            host: ClearHost(), keys: oldKv, store: folder, files: DirectoryFileStore(root: dataDir))
        XCTAssertEqual(clear["ok"] as? Bool, true, "\(clear)")
        let left = try await DirectoryFileStore(root: dataDir).list()
        XCTAssertTrue(left.isEmpty, "every file is gone")
    }
}

private final class MemoryKeys: KeyValueStore {
    var values: [String: String] = [:]
    func getString(_ key: String) -> String? { values[key] }
    func putString(_ key: String, _ value: String) { values[key] = value }
    func remove(_ key: String) { values.removeValue(forKey: key) }
}

/// The host of a clear: the key and the pairing are the platform's, and here they just succeed.
private final class ClearHost: MigrationHost {
    func perform(_ e: [String: Any]) async throws -> [String: Any] {
        switch e["type"] as? String {
        case "runClearStep": return ["ok": true]
        default: throw NppsCore.Refusal(message: "unknown effect \(e["type"] ?? "")")
        }
    }
}

private extension String {
    func chunked(_ size: Int) -> [String] {
        stride(from: 0, to: count, by: size).map { String(dropFirst($0).prefix(size)) }
    }
}
