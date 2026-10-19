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
private let BACKUP_TASK_ID = "life.neurone.uhdr-backup"

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

    /// Where the backup goes, and when the last one ran. Kept in `ProtectedDefaults`, not the backup-eligible `UserDefaults` plist
    /// (`SWR-MG-16`): this is a class and not a view, so they are plain properties and a change is announced by hand.
    private static let destinationKey = "np.backup.destination"
    private static let lastEpochKey = "np.backup.last-epoch"

    var destination: String {
        get { ProtectedDefaults.standard.string(forKey: Self.destinationKey) ?? BackupDestination.localUSBC.rawValue }
        set {
            objectWillChange.send()
            ProtectedDefaults.standard.set(newValue, forKey: Self.destinationKey)
        }
    }

    private var lastBackupEpoch: Double {
        get { ProtectedDefaults.standard.double(forKey: Self.lastEpochKey) }
        set { ProtectedDefaults.standard.set(newValue, forKey: Self.lastEpochKey) }
    }

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

        // Without the sealed index the directory is in the old form (or new), and the write below removes the old archives, so
        // every file is backed up, not only the changed ones (Finding 7, UHDRBackupFormat).
        let isFirstInNewForm = !UHDRBackupFormat.hasIndex(in: backupDirectory)
        let lastDate = lastBackupEpoch > 0 && !isFirstInNewForm ? Date(timeIntervalSince1970: lastBackupEpoch) : Date.distantPast
        let changedFiles = uhdrFiles.filter { url in
            guard let attrs = try? url.resourceValues(forKeys: [.contentModificationDateKey]),
                  let modDate = attrs.contentModificationDate else { return false }
            return modDate > lastDate
        }

        // No plaintext manifest, no time of backup and no count are written: see UHDRBackupFormat.
        try UHDRBackupFormat.write(changedFiles, to: backupDirectory, key: SymmetricKey(data: key.k1))
    }

    // MARK: - Background task scheduling

    func scheduleBackgroundTask() {
        let request = BGAppRefreshTaskRequest(identifier: BACKUP_TASK_ID)
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
