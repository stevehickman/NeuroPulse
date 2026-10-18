import Foundation
import CryptoKit
import os

// The on-disk form of the nightly UHDR backup (NP-PRIV privacy review Finding 7, REQ-UHDR-BK-01).
//
// WHY THIS EXISTS. The backup directory held, in the clear, a `manifest.json` with the exact time of the last backup and the
// number of files it wrote, and archives named after the sessions they hold (`session_<id>.edf.enc`, the id a counter the hub
// hands out). CLAUDE.md §5.1 classes session timestamps and counts as UHDR (they say something about the person, when and how
// often they use the device). The directory is excluded from backup, but it is a place a file manager, a forensic image or
// the person's own cloud-sync of a copied folder can reach.
//
// What it is now:
//   • an archive is named by a random UUID, kept stable for its source file across nights (it is overwritten, not duplicated);
//   • the map from source file to archive name lives in ONE sealed file, `index.enc`, sealed the way the archives are;
//   • there is no plaintext manifest, no time of backup and no count anywhere in the directory's contents;
//   • every file's creation and modification time is set to the epoch, so the directory listing does not give the backup times.
//
// RESIDUAL, stated and not removed: the NUMBER of archive files and their SIZES are visible (a size is roughly a session's
// length). Hiding them means padding and a fixed-size container, which is a different feature. And the AES-GCM here is Swift
// code over CryptoKit, one of those `CLAUDE.md` §22 lists for moving to the Rust core (OI-SOUPKT-01); this file adds no new
// primitive, it uses the same seal and its inverse.
struct UHDRBackupIndex: Codable, Equatable {
    /// Source file name (in Documents/UHDR) → archive file name (in Documents/UHDRBackup). Sealed on disk; names here are not
    /// visible in the directory, which is the point.
    var archives: [String: String] = [:]
}

enum UHDRBackupFormat {

    static let indexName = "index.enc"

    private static let log = Logger(subsystem: "life.neurone.app", category: "UHDRBackupFormat")

    /// Whether this directory already holds the sealed index. Without one the directory is in the old form (or empty), and the
    /// caller must back up EVERY source file, not only the changed ones: the old archives are removed after a write.
    static func hasIndex(in directory: URL) -> Bool {
        FileManager.default.fileExists(atPath: directory.appendingPathComponent(indexName).path)
    }

    /// The index in `directory`, or an empty one when there is none or it cannot be opened with `key` (a changed key cannot open
    /// the old archives either; the backup carries on and writes under the new key).
    static func readIndex(in directory: URL, key: SymmetricKey) -> UHDRBackupIndex {
        let url = directory.appendingPathComponent(indexName)
        guard let sealed = try? Data(contentsOf: url) else { return UHDRBackupIndex() }
        guard let plain = try? open(sealed, key: key), let index = try? JSONDecoder().decode(UHDRBackupIndex.self, from: plain)
        else {
            log.error("UHDRBackupFormat: the backup index could not be opened with this key; starting a new one.")
            return UHDRBackupIndex()
        }
        return index
    }

    /// Seals each of `sources` into `directory` under a random, stable name, rewrites the sealed index to cover them all,
    /// then removes every other file in the directory (the plaintext manifest and archives named for their sessions that
    /// earlier builds left, and archives no index names).
    static func write(_ sources: [URL], to directory: URL, key: SymmetricKey) throws {
        let fm = FileManager.default
        var index = readIndex(in: directory, key: key)

        for source in sources {
            let sourceName = source.lastPathComponent
            let archiveName = index.archives[sourceName] ?? UUID().uuidString.lowercased() + ".enc"
            let archiveURL = directory.appendingPathComponent(archiveName)
            try seal(try Data(contentsOf: source), key: key).write(to: archiveURL, options: .atomic)
            flattenTimes(of: archiveURL)
            index.archives[sourceName] = archiveName
        }

        let indexURL = directory.appendingPathComponent(indexName)
        try seal(try JSONEncoder().encode(index), key: key).write(to: indexURL, options: .atomic)
        flattenTimes(of: indexURL)

        let keep = Set(index.archives.values).union([indexName])
        for name in (try? fm.contentsOfDirectory(atPath: directory.path)) ?? [] where !keep.contains(name) {
            try? fm.removeItem(at: directory.appendingPathComponent(name))
        }
    }

    /// Opens one archive of the index back to the file it holds (the restore direction; the app has no restore screen yet).
    static func read(sourceName: String, from directory: URL, key: SymmetricKey) throws -> Data? {
        guard let archiveName = readIndex(in: directory, key: key).archives[sourceName] else { return nil }
        return try open(Data(contentsOf: directory.appendingPathComponent(archiveName)), key: key)
    }

    // AES-256-GCM using K1. K2 is reserved for AES-XTS on the hub side; the app uses GCM because CryptoKit does not expose XTS.
    static func seal(_ plaintext: Data, key: SymmetricKey) throws -> Data {
        guard let combined = try AES.GCM.seal(plaintext, using: key).combined else { throw CocoaError(.coderInvalidValue) }
        return combined
    }

    static func open(_ sealed: Data, key: SymmetricKey) throws -> Data {
        try AES.GCM.open(AES.GCM.SealedBox(combined: sealed), using: key)
    }

    /// Sets the creation and modification times to the epoch, so the directory does not say when each file was written.
    private static func flattenTimes(of url: URL) {
        let epoch = Date(timeIntervalSince1970: 0)
        do {
            try FileManager.default.setAttributes([.creationDate: epoch, .modificationDate: epoch], ofItemAtPath: url.path)
        } catch {
            let msg = error.localizedDescription
            log.error("UHDRBackupFormat: could not flatten a file's times: \(msg, privacy: .public)")
        }
    }
}
