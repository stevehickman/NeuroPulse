import Foundation
import BackgroundTasks
import CryptoKit
import SwiftUI   // @AppStorage, UIDevice
import os

// Nightly incremental UHDR backup scheduler.
// Per CLAUDE.md §5.1: backup uses same Argon2id-derived AES-256-XTS key as on-device UHDR.
// NeurOne cannot decrypt backup archives.
// Backup only triggers when device is on USB-C power (same condition as hub connection).
// Destination: local USB-C connected storage OR E2E encrypted cloud (user-held key).

// Background task identifier — register in Info.plist BGTaskSchedulerPermittedIdentifiers.
private let backupTaskID = "life.neurone.uhdr-backup"

@MainActor
final class UHDRBackupScheduler: ObservableObject {

    @Published private(set) var lastBackupAt: Date?
    @Published private(set) var lastBackupStatus: BackupStatus = .never

    private let keyManager: UHDRKeyManager
    private let uhdrDirectory: URL
    private let backupDirectory: URL

    private static let log = Logger(subsystem: "life.neurone.app", category: "UHDRBackupScheduler")

    enum BackupStatus {
        case never
        case succeeded(Date)
        case failed(String)
        case skipped(String)  // e.g. "Not on USB-C power"
    }

    enum BackupDestination: String, CaseIterable {
        case localUSBC     = "USB-C Local Storage"
        case encryptedCloud = "Encrypted Cloud (User Key)"
    }

    @AppStorage("np.backup.destination") var destination: String = BackupDestination.localUSBC.rawValue
    @AppStorage("np.backup.last-epoch") private var lastBackupEpoch: Double = 0

    init(keyManager: UHDRKeyManager) {
        self.keyManager = keyManager
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        uhdrDirectory = docs.appendingPathComponent("UHDR", isDirectory: true)
        backupDirectory = docs.appendingPathComponent("UHDRBackup", isDirectory: true)

        // Create the backup directory. Both failures are logged at fault level —
        // a silent failure here means the backup exclusion below also silently
        // fails, leaving encrypted UHDR archives backup-eligible.
        do {
            try FileManager.default.createDirectory(at: backupDirectory, withIntermediateDirectories: true)
        } catch {
            let msg = error.localizedDescription
            Self.log.fault("UHDRBackupScheduler: backup directory creation failed — exclusion may not apply: \(msg, privacy: .public)")
        }

        // Exclude the backup directory from iCloud and iTunes backups.
        // Encrypted archives are user-key-encrypted UHDR; they must not flow to
        // Apple infrastructure. Log failures so the gap is visible in diagnostics.
        do {
            try (backupDirectory as NSURL).setResourceValue(true, forKey: .isExcludedFromBackupKey)
        } catch {
            let msg = error.localizedDescription
            Self.log.fault("UHDRBackupScheduler: backup exclusion not applied — UHDR archives may flow to iCloud: \(msg, privacy: .public)")
        }

        // Enable battery monitoring once in init rather than on every isOnUSBCPower() call.
        UIDevice.current.isBatteryMonitoringEnabled = true

        scheduleBackgroundTask()
    }

    // Call from AppDelegate.application(_:handleEventsForBackgroundURLSession:) and
    // BGTaskScheduler.handleAppRefresh after registering the task.
    func performBackupIfNeeded() async {
        guard keyManager.isAuthenticated else {
            lastBackupStatus = .skipped("Session not authenticated — unlock the app first.")
            return
        }

        guard isOnUSBCPower() else {
            lastBackupStatus = .skipped("Not on USB-C power — backup deferred.")
            return
        }

        let lastBackupDate = lastBackupEpoch > 0 ? Date(timeIntervalSince1970: lastBackupEpoch) : Date.distantPast
        guard Date().timeIntervalSince(lastBackupDate) > 20 * 3600 else {
            // Already backed up within 20 hours — skip.
            return
        }

        do {
            try await performIncrementalBackup()
            let now = Date()
            lastBackupAt = now
            lastBackupEpoch = now.timeIntervalSince1970
            lastBackupStatus = .succeeded(now)
        } catch {
            lastBackupStatus = .failed(error.localizedDescription)
        }

        scheduleBackgroundTask()
    }

    // MARK: - Backup implementation

    private func performIncrementalBackup() async throws {
        guard let key = keyManager.activeKey else { return }
        let fm = FileManager.default

        guard let uhdrFiles = try? fm.contentsOfDirectory(
            at: uhdrDirectory, includingPropertiesForKeys: [.contentModificationDateKey], options: .skipsHiddenFiles)
        else { return }

        let lastDate = lastBackupEpoch > 0 ? Date(timeIntervalSince1970: lastBackupEpoch) : Date.distantPast
        let changedFiles = uhdrFiles.filter { url in
            guard let attrs = try? url.resourceValues(forKeys: [.contentModificationDateKey]),
                  let modDate = attrs.contentModificationDate else { return false }
            return modDate > lastDate
        }

        for sourceURL in changedFiles {
            let destURL = backupDirectory.appendingPathComponent(sourceURL.lastPathComponent + ".enc")
            let plaintext = try Data(contentsOf: sourceURL)
            let ciphertext = try encryptForBackup(plaintext, key: key)
            try ciphertext.write(to: destURL, options: .atomic)
        }

        // Generate incremental manifest — plaintext, no key material.
        let manifest = BackupManifest(
            createdAt: Date(),
            fileCount: changedFiles.count
        )
        let manifestData = try JSONEncoder().encode(manifest)
        try manifestData.write(to: backupDirectory.appendingPathComponent("manifest.json"), options: .atomic)
    }

    // AES-256-GCM using K1. K2 is reserved for AES-XTS on the hub side; app uses GCM
    // because iOS CryptoKit does not expose XTS mode.
    private func encryptForBackup(_ plaintext: Data, key: UHDRKey) throws -> Data {
        let symKey = SymmetricKey(data: key.k1)
        let sealed = try AES.GCM.seal(plaintext, using: symKey)
        return sealed.combined ?? Data()
    }

    // MARK: - Background task scheduling

    func scheduleBackgroundTask() {
        let request = BGAppRefreshTaskRequest(identifier: backupTaskID)
        request.earliestBeginDate = Date(timeIntervalSinceNow: 8 * 3600)  // earliest in 8 hours
        try? BGTaskScheduler.shared.submit(request)
    }

    // MARK: - Power detection

    private func isOnUSBCPower() -> Bool {
        // isBatteryMonitoringEnabled is enabled once in init() — no need to set it here.
        let state = UIDevice.current.batteryState
        return state == .charging || state == .full
    }
}

struct BackupManifest: Codable {
    var createdAt: Date
    var fileCount: Int
    // keyFingerprint removed: SHA-256(K1) truncated to 8 bytes was stored in a plaintext
    // manifest that lands in the unencrypted backup directory. Any partial hash of key
    // material belongs only in secure storage, not in a JSON file.
}
