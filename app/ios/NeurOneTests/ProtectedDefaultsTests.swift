import XCTest
@testable import NeurOne

// ProtectedDefaults holds consent state and session history outside the backup-eligible
// UserDefaults plist. These tests pin the properties that make it safe to rely on:
// round-trip, backup exclusion, one-time migration that removes the plaintext legacy copy,
// and removal from both stores.
final class ProtectedDefaultsTests: XCTestCase {

    private var dir: URL!
    private var legacy: UserDefaults!
    private var legacyName: String!
    private var store: ProtectedDefaults!

    override func setUp() {
        super.setUp()
        dir = FileManager.default.temporaryDirectory
            .appendingPathComponent("ProtectedDefaultsTests-\(UUID().uuidString)", isDirectory: true)
        legacyName = "ProtectedDefaultsTests.\(UUID().uuidString)"
        legacy = UserDefaults(suiteName: legacyName)!
        store = ProtectedDefaults(directory: dir, legacy: legacy)
    }

    override func tearDown() {
        legacy.removePersistentDomain(forName: legacyName)
        try? FileManager.default.removeItem(at: dir)
        super.tearDown()
    }

    // @verifies REQ-SW03-18#a
    func testRoundTrip() {
        let blob = Data("hello".utf8)
        store.set(blob, forKey: "np.consent.research")
        XCTAssertEqual(store.data(forKey: "np.consent.research"), blob)
        // A second instance over the same directory sees it (survives a "relaunch").
        XCTAssertEqual(ProtectedDefaults(directory: dir, legacy: legacy).data(forKey: "np.consent.research"), blob)
    }

    // @verifies REQ-SW03-18#a
    func testMissingKeyIsNil() {
        XCTAssertNil(store.data(forKey: "np.nothing"))
    }

    // @verifies REQ-SW03-18#a
    func testFilesAreExcludedFromBackup() {
        store.set(Data("x".utf8), forKey: "np.session.history")
        XCTAssertTrue(store.isExcludedFromBackup(forKey: "np.session.history"))
        let dirValues = try? dir.resourceValues(forKeys: [.isExcludedFromBackupKey])
        XCTAssertEqual(dirValues?.isExcludedFromBackup, true)
    }

    // @verifies REQ-SW03-18#a
    func testWriteDoesNotLeaveALegacyCopy() {
        legacy.set(Data("old".utf8), forKey: "np.consent.research")
        store.set(Data("new".utf8), forKey: "np.consent.research")
        XCTAssertNil(legacy.data(forKey: "np.consent.research"),
                     "a stale plaintext copy must not outlive a newer protected one")
        XCTAssertEqual(store.data(forKey: "np.consent.research"), Data("new".utf8))
    }

    // @verifies REQ-SW03-18#a
    func testMigratesLegacyValueOnceAndRemovesIt() {
        let old = Data("legacy-consent".utf8)
        legacy.set(old, forKey: "np.consent.clinician-grants")

        XCTAssertEqual(store.data(forKey: "np.consent.clinician-grants"), old,
                       "the first read returns the legacy value")
        XCTAssertNil(legacy.data(forKey: "np.consent.clinician-grants"),
                     "migration must remove the backup-eligible copy")
        XCTAssertTrue(store.isExcludedFromBackup(forKey: "np.consent.clinician-grants"))
        // Second read is served from the protected store.
        XCTAssertEqual(store.data(forKey: "np.consent.clinician-grants"), old)
    }

    // @verifies REQ-SW03-18#a
    func testRemoveClearsBothStores() {
        legacy.set(Data("a".utf8), forKey: "np.k")
        store.set(Data("b".utf8), forKey: "np.k2")
        store.removeObject(forKey: "np.k")
        store.removeObject(forKey: "np.k2")
        XCTAssertNil(store.data(forKey: "np.k"))
        XCTAssertNil(store.data(forKey: "np.k2"))
        XCTAssertNil(legacy.data(forKey: "np.k"))
    }

    // @verifies REQ-SW03-18#a
    func testUnsafeKeyCharactersStayInsideTheDirectory() {
        store.set(Data("x".utf8), forKey: "../escape/attempt")
        let contents = (try? FileManager.default.contentsOfDirectory(atPath: dir.path)) ?? []
        XCTAssertEqual(contents.count, 1)
        XCTAssertFalse(FileManager.default.fileExists(atPath: dir.deletingLastPathComponent().appendingPathComponent("escape").path))
    }
}
