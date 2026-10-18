import XCTest
import CryptoKit
@testable import NeurOne

// The on-disk form of the nightly UHDR backup (privacy review Finding 7, REQ-UHDRBK-01). Real files in a temporary directory,
// a fixed key; nothing here touches the keychain or the real UHDR directory.
final class UHDRBackupFormatTests: XCTestCase {

    private var root: URL!
    private var sourceDir: URL!
    private var backupDir: URL!
    private let key = SymmetricKey(data: Data(repeating: 0x42, count: 32))

    override func setUpWithError() throws {
        root = FileManager.default.temporaryDirectory.appendingPathComponent("uhdr-backup-\(UUID().uuidString)", isDirectory: true)
        sourceDir = root.appendingPathComponent("UHDR", isDirectory: true)
        backupDir = root.appendingPathComponent("UHDRBackup", isDirectory: true)
        try FileManager.default.createDirectory(at: sourceDir, withIntermediateDirectories: true)
        try FileManager.default.createDirectory(at: backupDir, withIntermediateDirectories: true)
    }

    override func tearDownWithError() throws {
        try? FileManager.default.removeItem(at: root)
    }

    private func source(_ name: String, _ text: String) throws -> URL {
        let url = sourceDir.appendingPathComponent(name)
        try Data(text.utf8).write(to: url)
        return url
    }

    private func names() throws -> [String] {
        try FileManager.default.contentsOfDirectory(atPath: backupDir.path).sorted()
    }

    // @verifies REQ-UHDRBK-01#a
    func testNothingInTheDirectoryIsPlaintext() throws {
        let a = try source("session_17.edf", "waveform-a")
        let b = try source("session_18.edf", "waveform-b")
        try UHDRBackupFormat.write([a, b], to: backupDir, key: key)

        XCTAssertFalse(try names().contains("manifest.json"), "there must be no plaintext manifest")
        for name in try names() {
            let bytes = try Data(contentsOf: backupDir.appendingPathComponent(name))
            let text = String(decoding: bytes, as: UTF8.self)
            XCTAssertFalse(text.contains("createdAt") || text.contains("fileCount"), "\(name) must not hold a time or a count")
            XCTAssertFalse(text.contains("session_17") || text.contains("waveform-a"), "\(name) must not hold a name or a waveform in the clear")
        }
    }

    // @verifies REQ-UHDRBK-01#b
    func testArchivesAreNamedAtRandomAndKeepTheirNameAcrossNights() throws {
        let a = try source("session_17.edf", "one")
        let b = try source("session_18.edf", "two")
        try UHDRBackupFormat.write([a, b], to: backupDir, key: key)
        let first = try names()

        XCTAssertFalse(first.contains { $0.contains("session") }, "no archive may be named for its session: \(first)")
        XCTAssertEqual(first.count, 3, "two archives and the index: \(first)")

        try Data("one, changed".utf8).write(to: a)
        try UHDRBackupFormat.write([a], to: backupDir, key: key)
        XCTAssertEqual(try names(), first, "a rewritten source overwrites its own archive and adds no new file")
    }

    // @verifies REQ-UHDRBK-01#c
    func testTheIndexIsSealedAndOpensOnlyWithTheKey() throws {
        let a = try source("session_17.edf", "one")
        try UHDRBackupFormat.write([a], to: backupDir, key: key)

        let raw = try Data(contentsOf: backupDir.appendingPathComponent(UHDRBackupFormat.indexName))
        XCTAssertFalse(String(decoding: raw, as: UTF8.self).contains("session_17"), "the index must not hold a source name in the clear")
        XCTAssertEqual(UHDRBackupFormat.readIndex(in: backupDir, key: key).archives.keys.sorted(), ["session_17.edf"])
        XCTAssertEqual(try UHDRBackupFormat.read(sourceName: "session_17.edf", from: backupDir, key: key), Data("one".utf8))

        let other = SymmetricKey(data: Data(repeating: 0x01, count: 32))
        XCTAssertTrue(UHDRBackupFormat.readIndex(in: backupDir, key: other).archives.isEmpty, "a wrong key reads as no index")
        // A changed key starts a new index and the backup carries on.
        XCTAssertNoThrow(try UHDRBackupFormat.write([a], to: backupDir, key: other))
        XCTAssertEqual(UHDRBackupFormat.readIndex(in: backupDir, key: other).archives.keys.sorted(), ["session_17.edf"])
    }

    // @verifies REQ-UHDRBK-01#d
    func testEveryFilesTimesAreTheEpoch() throws {
        let a = try source("session_17.edf", "one")
        try UHDRBackupFormat.write([a], to: backupDir, key: key)

        for name in try names() {
            let attrs = try FileManager.default.attributesOfItem(atPath: backupDir.appendingPathComponent(name).path)
            XCTAssertEqual((attrs[.modificationDate] as? Date)?.timeIntervalSince1970, 0, "\(name) must not give its write time")
            XCTAssertEqual((attrs[.creationDate] as? Date)?.timeIntervalSince1970, 0, "\(name) must not give its creation time")
        }
    }

    // @verifies REQ-UHDRBK-01#e
    func testTheEarlierFormIsRemovedAfterAFullWrite() throws {
        try Data("{\"createdAt\":1,\"fileCount\":1}".utf8).write(to: backupDir.appendingPathComponent("manifest.json"))
        try Data("old".utf8).write(to: backupDir.appendingPathComponent("session_17.edf.enc"))
        XCTAssertFalse(UHDRBackupFormat.hasIndex(in: backupDir), "a directory in the earlier form has no index")

        let a = try source("session_17.edf", "one")
        try UHDRBackupFormat.write([a], to: backupDir, key: key)

        XCTAssertTrue(UHDRBackupFormat.hasIndex(in: backupDir))
        XCTAssertFalse(try names().contains("manifest.json"))
        XCTAssertFalse(try names().contains("session_17.edf.enc"), "an archive named for its session must be gone")
    }
}
