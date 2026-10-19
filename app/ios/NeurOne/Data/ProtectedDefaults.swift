import Combine
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
// API mirrors the `UserDefaults` calls the stores already used, so the swap is mechanical. That includes the typed calls for the
// small values the app keeps (flags, counters, lists: `bool`, `integer`, `double`, `string`, `array`, `dictionary`, `object` and
// `set(_:forKey:)`): the age and biometric-privacy answers, the research and warranty switches, the launch count, the consumable
// counts, the backup folder and so on. Those say something about the person by inference or name an install, and `CLAUDE.md` §5.1
// says that without a showing to the contrary a field is UHDR, so they live here too and not in the backup-eligible plist
// (desktop audit of the privacy review, `SWR-MG-16`: every key the core lists is sealed at rest on the hosts that can, and kept
// out of backup on this one). A value is one property-list file per key, `<key>.plist`, protected exactly like the data files.
//
// It is an `ObservableObject` so a view that reads a flag through `ProtectedAppStorage` redraws when it changes, as `@AppStorage` did.
final class ProtectedDefaults: ObservableObject, @unchecked Sendable {

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
        lock.lock()
        try? FileManager.default.removeItem(at: fileURL(for: key))
        try? FileManager.default.removeItem(at: valueURL(for: key))
        legacy.removeObject(forKey: key)
        lock.unlock()
        announceChange()
    }

    /// True if the key's file is excluded from backup. Exposed for tests and diagnostics.
    func isExcludedFromBackup(forKey key: String) -> Bool {
        let url = FileManager.default.fileExists(atPath: valueURL(for: key).path) ? valueURL(for: key) : fileURL(for: key)
        let values = try? url.resourceValues(forKeys: [.isExcludedFromBackupKey])
        return values?.isExcludedFromBackup ?? false
    }

    // MARK: - Typed values (the `UserDefaults` calls the flag and counter stores used)

    /// The value stored under `key`: a number, a flag, a string, a list or a dictionary of those. Like `data(forKey:)`, a value
    /// found only in the legacy `UserDefaults` is moved here once and the plaintext copy removed.
    func object(forKey key: String) -> Any? {
        lock.lock(); defer { lock.unlock() }
        if let value = readValue(forKey: key) { return value }
        guard let old = legacy.object(forKey: key) else { return nil }
        if writeValue(old, forKey: key) {
            legacy.removeObject(forKey: key)
        }
        return old
    }

    /// Stores `value`, or removes the key when it is nil. A value a property list cannot hold is not stored.
    func set(_ value: Any?, forKey key: String) {
        guard let value else {
            removeObject(forKey: key)
            return
        }
        lock.lock()
        if writeValue(value, forKey: key) {
            // A stale legacy copy must not outlive a newer protected one.
            legacy.removeObject(forKey: key)
        }
        lock.unlock()
        announceChange()
    }

    func bool(forKey key: String) -> Bool { (object(forKey: key) as? NSNumber)?.boolValue ?? false }
    func integer(forKey key: String) -> Int { (object(forKey: key) as? NSNumber)?.intValue ?? 0 }
    func double(forKey key: String) -> Double { (object(forKey: key) as? NSNumber)?.doubleValue ?? 0 }
    func string(forKey key: String) -> String? { object(forKey: key) as? String }
    func array(forKey key: String) -> [Any]? { object(forKey: key) as? [Any] }
    func dictionary(forKey key: String) -> [String: Any]? { object(forKey: key) as? [String: Any] }

    // MARK: - Internals

    private func fileURL(for key: String) -> URL {
        // Keys are dotted identifiers ("np.consent.research"); keep only filename-safe characters.
        let safe = key.map { $0.isLetter || $0.isNumber || $0 == "." || $0 == "-" || $0 == "_" ? $0 : "_" }
        return directory.appendingPathComponent(String(safe) + ".json")
    }

    private func valueURL(for key: String) -> URL {
        fileURL(for: key).deletingPathExtension().appendingPathExtension("plist")
    }

    /// Views that read a value through `ProtectedAppStorage` redraw on this. SwiftUI wants it on the main thread.
    private func announceChange() {
        if Thread.isMainThread {
            objectWillChange.send()
        } else {
            DispatchQueue.main.async { [weak self] in self?.objectWillChange.send() }
        }
    }

    private func readValue(forKey key: String) -> Any? {
        guard let data = try? Data(contentsOf: valueURL(for: key)),
              let wrapper = try? PropertyListSerialization.propertyList(from: data, options: [], format: nil) as? [String: Any]
        else { return nil }
        return wrapper["v"]
    }

    private func writeValue(_ value: Any, forKey key: String) -> Bool {
        // Wrapped in a dictionary so a bare number or string is a valid property list at the top level.
        let wrapper: [String: Any] = ["v": value]
        guard PropertyListSerialization.propertyList(wrapper, isValidFor: .binary),
              let data = try? PropertyListSerialization.data(fromPropertyList: wrapper, format: .binary, options: 0)
        else {
            Self.log.error("ProtectedDefaults: a value for a key cannot be stored as a property list")
            return false
        }
        return writeFile(data, to: valueURL(for: key))
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
        writeFile(data, to: fileURL(for: key))
    }

    private func writeFile(_ data: Data, to target: URL) -> Bool {
        var url = target
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
