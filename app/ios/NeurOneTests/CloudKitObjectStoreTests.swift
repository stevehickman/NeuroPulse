//
//  CloudKitObjectStoreTests.swift
//  NeurOneTests
//
//  The iCloud route's rules over an in-memory stand-in for CloudKit (SWR-MG-07, SWR-MG-11): record names, hex, the claim's two
//  outcomes and listing. The cases are those of Kotlin's ObjectStoreContract. It is not CloudKit, and it serialises racing creates.
//
//  Subject under test: CloudKitObjectStore, CKDatabaseRecords (app/ios/NeurOne/Session/CloudKitObjectStore.swift)

import CloudKit
import XCTest
@testable import NeurOne

private final class FakeRecords: CloudKitRecords, @unchecked Sendable {
    private let lock = NSLock()
    private var stored: [String: Data] = [:]

    var keys: Set<String> { lock.lock(); defer { lock.unlock() }; return Set(stored.keys) }

    func create(_ recordName: String, _ data: Data) async throws -> Bool {
        lock.lock(); defer { lock.unlock() }
        if stored[recordName] != nil { return false }
        stored[recordName] = data
        return true
    }

    func replace(_ recordName: String, _ data: Data) async throws {
        lock.lock(); defer { lock.unlock() }
        stored[recordName] = data
    }

    func remove(_ recordName: String) async throws {
        lock.lock(); defer { lock.unlock() }
        stored[recordName] = nil
    }

    func fetch(_ recordName: String) async throws -> Data? {
        lock.lock(); defer { lock.unlock() }
        return stored[recordName]
    }

    func names() async throws -> [String] {
        lock.lock(); defer { lock.unlock() }
        return Array(stored.keys)
    }
}

final class CloudKitObjectStoreTests: XCTestCase {

    private func makeStore() -> (CloudKitObjectStore, FakeRecords) {
        let fake = FakeRecords()
        return (CloudKitObjectStore(records: fake), fake)
    }

    // @verifies SWR-MG-11#calls
    func testAMissingObjectReadsAsNilAndDeletingItIsNotAnError() async throws {
        let (store, _) = makeStore()
        let missing = try await store.get("np-migration-bundle.npbk")
        XCTAssertNil(missing)
        try await store.delete("np-migration-bundle.npbk")
    }

    // @verifies SWR-MG-11#calls
    func testPutThenGetReturnsTheSameBytesAndAPutReplaces() async throws {
        let (store, _) = makeStore()
        try await store.put("np-migration-bundle.npbk", "00ff10a5")
        let first = try await store.get("np-migration-bundle.npbk")
        XCTAssertEqual(first, "00ff10a5")
        try await store.put("np-migration-bundle.npbk", "010203")
        let second = try await store.get("np-migration-bundle.npbk")
        XCTAssertEqual(second, "010203")
    }

    // @verifies SWR-MG-11#calls
    func testASecondCreateIsRefusedAndLeavesTheFirstWritersBytes() async throws {
        let (store, _) = makeStore()
        let first = try await store.createIfAbsent("np-migration-claim.json", "41")
        let second = try await store.createIfAbsent("np-migration-claim.json", "42")
        let kept = try await store.get("np-migration-claim.json")
        XCTAssertTrue(first)
        XCTAssertFalse(second)
        XCTAssertEqual(kept, "41")
    }

    // @verifies SWR-MG-11#calls
    func testAReleasedClaimCanBeClaimedAgain() async throws {
        let (store, _) = makeStore()
        let first = try await store.createIfAbsent("np-migration-claim.json", "41")
        try await store.delete("np-migration-claim.json")
        let gone = try await store.get("np-migration-claim.json")
        let again = try await store.createIfAbsent("np-migration-claim.json", "42")
        XCTAssertTrue(first)
        XCTAssertNil(gone)
        XCTAssertTrue(again)
    }

    // @verifies SWR-MG-07
    func testOfManySimultaneousCreatesExactlyOneWinsOverTheStandIn() async throws {
        let (store, _) = makeStore()
        let results = try await withThrowingTaskGroup(of: Bool.self) { group -> [Bool] in
            for index in 1...24 {
                group.addTask { try await store.createIfAbsent("np-migration-claim.json", String(format: "%02x", index)) }
            }
            var all: [Bool] = []
            for try await won in group { all.append(won) }
            return all
        }
        XCTAssertEqual(results.filter { $0 }.count, 1)
    }

    // @verifies SWR-MG-11#calls
    func testListReturnsOnlyTheNamesUnderThePrefix() async throws {
        let (store, _) = makeStore()
        for name in ["np-1", "np-2", "other", "np-3"] { try await store.put(name, "01") }
        let listed = try await store.list("np-")
        XCTAssertEqual(listed, ["np-1", "np-2", "np-3"])
    }

    // @verifies SWR-MG-11#calls
    func testANameWithSpacesSlashesPlusSignsAndALeadingUnderscoreRoundTrips() async throws {
        let (store, fake) = makeStore()
        try await store.put("dir/a b+c", "01")
        try await store.put("_hidden", "02")
        XCTAssertEqual(fake.keys, ["dir%2Fa%20b%2Bc", "%5Fhidden"])
        let slash = try await store.list("dir/")
        let underscore = try await store.list("_")
        let read = try await store.get("dir/a b+c")
        XCTAssertEqual(slash, ["dir/a b+c"])
        XCTAssertEqual(underscore, ["_hidden"])
        XCTAssertEqual(read, "01")
    }

    // @verifies SWR-MG-11#calls
    func testAnEmptyObjectAndATooLongNameAreRefusedBeforeAnythingIsWritten() async throws {
        let (store, fake) = makeStore()
        do {
            try await store.put("x", "")
            XCTFail("an empty object must be refused")
        } catch let error as CloudKitStoreError {
            XCTAssertEqual(error, .emptyObject)
        }
        do {
            try await store.put(String(repeating: "x", count: 256), "01")
            XCTFail("a name over the limit must be refused")
        } catch let error as CloudKitStoreError {
            XCTAssertEqual(error, .nameTooLong(String(repeating: "x", count: 256)))
        }
        XCTAssertTrue(fake.keys.isEmpty)
    }

    // @verifies SWR-MG-11#calls
    func testFreeSpaceIsUnknown() async throws {
        let (store, _) = makeStore()
        let space = try await store.freeSpace()
        XCTAssertNil(space)
    }

    // @verifies SWR-MG-11#calls
    func testOnlyServerRecordChangedIsReadAsExistsAndOnlyUnknownItemAsMissing() {
        XCTAssertTrue(CKDatabaseRecords.isExists(CKError(.serverRecordChanged)))
        XCTAssertFalse(CKDatabaseRecords.isExists(CKError(.networkUnavailable)))
        XCTAssertFalse(CKDatabaseRecords.isExists(CKError(.quotaExceeded)))
        XCTAssertFalse(CKDatabaseRecords.isExists(CloudKitStoreError.emptyObject))
        XCTAssertTrue(CKDatabaseRecords.isUnknownItem(CKError(.unknownItem)))
        XCTAssertFalse(CKDatabaseRecords.isUnknownItem(CKError(.serverRecordChanged)))
    }
}
