import Foundation
import os

// File protection for the raw EDF+ waveform files in Documents/UHDR (privacy review Finding 8, REQ-UHDRFP-01).
//
// WHY. The files hold raw EEG and HRV waveforms (UHDR, CLAUDE.md §5.1). The app never set a protection class on them or on their
// folder: the hub's USB-C transfer lands them, and they relied on whatever class the system gave a new file. That is
// `completeUntilFirstUserAuthentication` today, but a default is not a decision, and a project setting (the Data Protection
// capability's default class) can change it without anyone reading this code.
//
// WHICH CLASS. `completeUntilFirstUserAuthentication`, and not `complete`, for the reason `ProtectedDefaults` gives: the nightly
// backup (`UHDRBackupScheduler`) runs as a background task, usually while the phone is charging and locked, and reads these files.
// Under `complete` the read fails while locked and the backup silently never runs. The strongest class that lets that task work
// is this one. The files are readable once the person has unlocked the phone after a restart, and not before.
//
// WHAT IT DOES NOT DO. It does not encrypt the file with the person's key (the hub may or may not, `OI-EDF-01`), and it does not
// hide the folder: `UIFileSharingEnabled` shows the whole Documents folder in the Files app and to a paired computer, which is the
// export route the person is meant to have (ISA ISC-54) and also shows `UHDRBackup` and the SHDR staging file (`OI-EDF-01`).
enum UHDRFileProtection {

    /// The class every UHDR waveform file and its folder is given.
    static let level = FileProtectionType.completeUntilFirstUserAuthentication

    private static let log = Logger(subsystem: "life.neurone.app", category: "UHDRFileProtection")

    /// Sets the class on one file or folder. `false` (and a fault in the log) if it could not be set: the caller carries on, because
    /// refusing to show a downloaded session helps no one, but the gap is visible in diagnostics.
    @discardableResult
    static func apply(to url: URL, fileManager: FileManager = .default) -> Bool {
        do {
            try fileManager.setAttributes([.protectionKey: level], ofItemAtPath: url.path)
            return true
        } catch {
            let msg = error.localizedDescription
            log.fault("UHDRFileProtection: protection class not applied to a UHDR file: \(msg, privacy: .public)")
            return false
        }
    }

    /// Sets the class on the folder and on every file already in it (files an earlier build left, and any the hub landed since).
    static func applyToFolderAndContents(_ directory: URL, fileManager: FileManager = .default) {
        apply(to: directory, fileManager: fileManager)
        let names = (try? fileManager.contentsOfDirectory(atPath: directory.path)) ?? []
        for name in names {
            apply(to: directory.appendingPathComponent(name), fileManager: fileManager)
        }
    }
}
