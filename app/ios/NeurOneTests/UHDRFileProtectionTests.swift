import XCTest
@testable import NeurOne

// The protection class on the UHDR waveform files (privacy review Finding 8, REQ-UHDRFP-01). A recording FileManager stands in for
// the real one, because the simulator does not enforce Data Protection and so cannot show what was asked of it.
final class UHDRFileProtectionTests: XCTestCase {

    private final class RecordingFileManager: FileManager {
        var calls: [(path: String, protection: FileProtectionType?)] = []
        var failing = false

        override func setAttributes(_ attributes: [FileAttributeKey: Any], ofItemAtPath path: String) throws {
            if failing { throw CocoaError(.fileWriteNoPermission) }
            calls.append((path, attributes[.protectionKey] as? FileProtectionType))
        }
    }

    private var folder: URL!

    override func setUpWithError() throws {
        folder = FileManager.default.temporaryDirectory.appendingPathComponent("uhdr-fp-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        try Data("a".utf8).write(to: folder.appendingPathComponent("session_1.edf"))
        try Data("b".utf8).write(to: folder.appendingPathComponent("session_2.edf"))
    }

    override func tearDownWithError() throws {
        try? FileManager.default.removeItem(at: folder)
    }

    // @verifies REQ-UHDRFP-01#b
    func testTheClassIsTheStrongestOneABackgroundBackupCanStillRead() {
        XCTAssertEqual(UHDRFileProtection.level, .completeUntilFirstUserAuthentication,
                       "`complete` would make the nightly backup fail while the phone is locked")
    }

    // @verifies REQ-UHDRFP-01#a
    func testTheFolderAndEveryFileInItGetTheClass() {
        let fm = RecordingFileManager()
        UHDRFileProtection.applyToFolderAndContents(folder, fileManager: fm)

        let paths = Set(fm.calls.map { $0.path })
        XCTAssertEqual(paths, [folder.path, folder.appendingPathComponent("session_1.edf").path,
                               folder.appendingPathComponent("session_2.edf").path])
        XCTAssertTrue(fm.calls.allSatisfy { $0.protection == UHDRFileProtection.level }, "every call asks for the one class")
    }

    // @verifies REQ-UHDRFP-01#a
    func testOneLandedFileGetsTheClass() {
        let fm = RecordingFileManager()
        let url = folder.appendingPathComponent("session_1.edf")

        XCTAssertTrue(UHDRFileProtection.apply(to: url, fileManager: fm))
        XCTAssertEqual(fm.calls.count, 1)
        XCTAssertEqual(fm.calls.first?.protection, UHDRFileProtection.level)
    }

    // @verifies REQ-UHDRFP-01#c
    func testAFailureIsReportedAndDoesNotThrowOrStopTheRest() {
        let fm = RecordingFileManager()
        fm.failing = true
        XCTAssertFalse(UHDRFileProtection.apply(to: folder.appendingPathComponent("session_1.edf"), fileManager: fm))
        // The folder pass carries on past a failure rather than raising.
        UHDRFileProtection.applyToFolderAndContents(folder, fileManager: fm)
    }
}
