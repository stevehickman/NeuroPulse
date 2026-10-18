import Foundation
import os

// Backup-excluded, file-protected key/data store for health-adjacent app state.
//
// WHY THIS EXISTS (NP-PRIV finding: iOS UserDefaults in backup). `UserDefaults` is written to a
// plist that iCloud and iTunes/Finder backups include and that MDM can read. Consent state
// (clinician grants, the research categories a person ticked, study participation) and session
// history (HRV RMSSD, coherence, protocol name, day) reveal health conditions by inference
// (GDPR Art. 9, WA My Health My Data). Android already excludes all of it from cloud backup and
// device transfer (`data_extraction_rules.xml`); this brings iOS to the same position.
//
// What it does differently from `UserDefaults`:
//   • one file per key under Application Support/Protected/, marked `isExcludedFromBackup`;
//   • `.completeUntilFirstUserAuthentication` — the strongest class that still lets a background
//     launch (BLE reconnect, BGTask) read the file while the phone is locked. `.completeFileProtection`
//     would make a locked-device read fail, and a failed read of consent state looks like "never
//     asked" (CLAUDE.md §6.0) — the wrong direction to fail;
//   • consent is deliberately re-established on a new device (as on Android), never restored.
//
// Not for secrets or keys: those belong in the Keychain (`…ThisDeviceOnly`). Plain feature flags
// that carry no health inference may stay in `UserDefaults`.
//
// MIGRATION. Builds before this one wrote these keys to `UserDefaults`. `data(forKey:)` falls back
// to that legacy value once, copies it into the protected store and removes it from
// `UserDefaults`, so the plaintext copy does not linger. Only after the protected write succeeds is
// the legacy value removed. Note the legacy value may already sit in an existing backup; that
// cannot be recalled from here.
//
// API mirrors the `UserDefaults` calls the stores already used, so the swap is mechanical.
final class ProtectedDefaults: @unchecked Sendable {

    /// Shared instance used by the app. Tests build their own with a temporary directory.
    static let standard = ProtectedDefaults()

    private static let log = Logger(subsystem: "life.neurone.app", category: "ProtectedDefaults")

    private let directory: URL
    private let legacy: UserDefaults
    private let lock = NSLock()

    /// - Parameters:
    ///   - directory: where the per-key files live. Defaults to Application Support/Protected.
    ///   - legacy: the `UserDefaults` that earlier builds wrote to; read once for migration.
    init(directory: URL? = nil, legacy: UserDefaults = .standard) {
        let base = directory ?? FileManager.default
            .urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Protected", isDirectory: true)
        self.directory = base
        self.legacy = legacy
        prepareDirectory()
    }

    // MARK: - API

    func data(forKey key: String) -> Data? {
        lock.lock(); defer { lock.unlock() }
        if let data = try? Data(contentsOf: fileURL(for: key)) { return data }
        // One-time migration from the backup-eligible store.
        guard let old = legacy.data(forKey: key) else { return nil }
        if write(old, forKey: key) {
            legacy.removeObject(forKey: key)
        }
        return old
    }

    func set(_ data: Data, forKey key: String) {
        lock.lock(); defer { lock.unlock() }
        if write(data, forKey: key) {
            // A stale legacy copy must not outlive a newer protected one.
            legacy.removeObject(forKey: key)
        }
    }

    /// Removes the key from both the protected store and the legacy `UserDefaults`.
    func removeObject(forKey key: String) {
        lock.lock(); defer { lock.unlock() }
        try? FileManager.default.removeItem(at: fileURL(for: key))
        legacy.removeObject(forKey: key)
    }

    /// True if the key's file is excluded from backup. Exposed for tests and diagnostics.
    func isExcludedFromBackup(forKey key: String) -> Bool {
        let values = try? fileURL(for: key).resourceValues(forKeys: [.isExcludedFromBackupKey])
        return values?.isExcludedFromBackup ?? false
    }

    // MARK: - Internals

    private func fileURL(for key: String) -> URL {
        // Keys are dotted identifiers ("np.consent.research"); keep only filename-safe characters.
        let safe = key.map { $0.isLetter || $0.isNumber || $0 == "." || $0 == "-" || $0 == "_" ? $0 : "_" }
        return directory.appendingPathComponent(String(safe) + ".json")
    }

    private func prepareDirectory() {
        var dir = directory
        do {
            try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
            var values = URLResourceValues()
            values.isExcludedFromBackup = true
            try dir.setResourceValues(values)
        } catch {
            // Logged at fault level: a silent failure here leaves health-adjacent state backup-eligible.
            let msg = error.localizedDescription
            Self.log.fault("ProtectedDefaults: directory setup failed — backup exclusion may not apply: \(msg, privacy: .public)")
        }
    }

    @discardableResult
    private func write(_ data: Data, forKey key: String) -> Bool {
        var url = fileURL(for: key)
        do {
            try data.write(to: url, options: [.atomic, .completeFileProtectionUntilFirstUserAuthentication])
            // Exclude each file as well as the directory: atomic writes replace the inode.
            var values = URLResourceValues()
            values.isExcludedFromBackup = true
            try url.setResourceValues(values)
            return true
        } catch {
            let msg = error.localizedDescription
            Self.log.error("ProtectedDefaults: write failed for a key: \(msg, privacy: .public)")
            return false
        }
    }
}
