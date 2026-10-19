import Combine
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

    // MARK: - Typed values (flags, counters, lists): the calls the flag stores used on UserDefaults (SWR-MG-16)

    // @verifies REQ-SW03-18#a
    func testFlagsNumbersTextAndListsRoundTripAndSurviveARelaunch() {
        store.set(true, forKey: "np.onboarding.bipa-accepted")
        store.set(7, forKey: "np.analytics.launch-count")
        store.set(1_760_000_000.5, forKey: "np.backup.last-epoch")
        store.set("Encrypted Cloud (User Key)", forKey: "np.backup.destination")
        store.set(["fam-a", "fam-b"], forKey: "np.used-families")
        store.set(["fam-a": "fam-b"], forKey: "np.clone-sources")

        let again = ProtectedDefaults(directory: dir, legacy: legacy)
        XCTAssertTrue(again.bool(forKey: "np.onboarding.bipa-accepted"))
        XCTAssertEqual(again.integer(forKey: "np.analytics.launch-count"), 7)
        XCTAssertEqual(again.double(forKey: "np.backup.last-epoch"), 1_760_000_000.5)
        XCTAssertEqual(again.string(forKey: "np.backup.destination"), "Encrypted Cloud (User Key)")
        XCTAssertEqual(again.array(forKey: "np.used-families") as? [String], ["fam-a", "fam-b"])
        XCTAssertEqual(again.dictionary(forKey: "np.clone-sources") as? [String: String], ["fam-a": "fam-b"])
        XCTAssertFalse(again.bool(forKey: "np.nothing"), "an absent flag is false, as UserDefaults gave")
        XCTAssertEqual(again.integer(forKey: "np.nothing"), 0)
        XCTAssertNil(again.string(forKey: "np.nothing"))
    }

    // @verifies REQ-SW03-18#a
    func testTypedValuesAreExcludedFromBackupAndNeverWrittenToTheLegacyPlist() {
        store.set(true, forKey: "np.warranty.consent-granted")
        XCTAssertTrue(store.isExcludedFromBackup(forKey: "np.warranty.consent-granted"))
        XCTAssertNil(legacy.object(forKey: "np.warranty.consent-granted"))
    }

    // @verifies REQ-SW03-18#a
    func testATypedValueFromTheLegacyPlistIsMovedOnceAndTheCopyRemoved() {
        legacy.set(true, forKey: "np.onboarding.age-confirmed")
        legacy.set(12, forKey: "np.analytics.launch-count")
        legacy.set(["fam-a"], forKey: "np.used-families")

        XCTAssertTrue(store.bool(forKey: "np.onboarding.age-confirmed"))
        XCTAssertEqual(store.integer(forKey: "np.analytics.launch-count"), 12)
        XCTAssertEqual(store.array(forKey: "np.used-families") as? [String], ["fam-a"])
        for key in ["np.onboarding.age-confirmed", "np.analytics.launch-count", "np.used-families"] {
            XCTAssertNil(legacy.object(forKey: key), "the plaintext copy of \(key) is gone once moved")
        }
        // And it is still there from the protected copy.
        XCTAssertEqual(store.integer(forKey: "np.analytics.launch-count"), 12)
    }

    // @verifies REQ-SW03-18#a
    func testSettingNilOrRemovingClearsATypedValueFromBothStores() {
        legacy.set(3, forKey: "np.k")
        store.set(4, forKey: "np.k")
        store.set(nil, forKey: "np.k")
        XCTAssertEqual(store.integer(forKey: "np.k"), 0)
        XCTAssertNil(legacy.object(forKey: "np.k"))
        store.set("x", forKey: "np.k2")
        store.removeObject(forKey: "np.k2")
        XCTAssertNil(store.string(forKey: "np.k2"))
    }

    // @verifies REQ-SW03-18#a
    func testAWriteAnnouncesAChangeSoAViewReadingAFlagRedraws() {
        var announced = 0
        let cancellable = store.objectWillChange.sink { announced += 1 }
        store.set(true, forKey: "np.onboarding.bipa-shown")
        store.removeObject(forKey: "np.onboarding.bipa-shown")
        XCTAssertEqual(announced, 2)
        cancellable.cancel()
    }

    // @verifies REQ-SW03-18#a
    func testAProtectedAppStorageValueIsRebuiltFromWhatTheFileHolds() {
        XCTAssertEqual(Bool(protectedValue: NSNumber(value: true)), true)
        XCTAssertEqual(Int(protectedValue: NSNumber(value: 5)), 5)
        XCTAssertEqual(Double(protectedValue: NSNumber(value: 1.5)), 1.5)
        XCTAssertEqual(String(protectedValue: "x"), "x")
        XCTAssertNil(Bool(protectedValue: "yes"), "a value of another type is not a flag")
    }
}

