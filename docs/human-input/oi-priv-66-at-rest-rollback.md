# Question for the principal: OI-PRIV-66 (5), may a device backup bring back an older sealed value?

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** none opened yet; to be opened with the pull request that carries this file.
**From:** NeurOne Software Engineering (`OI-PRIV-66` (5) in `docs/status/pending-decisions.md`; `NP-PRIV-ANALYSIS-007` Finding L20)
**To:** the principal
**Response needed:** an option (A to D); a short rationale; anything that would reopen it; your name, role and the date.

## 1. The question

A value the shared app keeps sealed at rest (`common/npps-core/src/at_rest.rs`) is AES-256-GCM under the install's data key, with
**its name as the only associated data**. Nothing in the sealed value says *when* it was written. So an older copy of the host's
key-value store, restored beside the same data key, opens as if it were current.

Plain form: **if the person restores a backup of their phone or computer, should the app accept the consent state, history and
limits that were in it, or refuse to read state older than what it last wrote?**

The case that matters most is consent. A person withdraws blanket research consent (L3) on Monday; on Wednesday they restore a
Sunday backup of the whole phone. The app reads Sunday's consent record and the withdrawal is gone. `CLAUDE.md` §6.2 says
withdrawal is permanent for future extracts, and §6.0 that "never asked" and "said stop" must stay distinguishable.

## 2. What was checked (2026-11-22, by reading the code; nothing was run on a device)

| Host | Where the sealed values are | Is that place in the OS backup? | Where the data key is | Does the key come back with it? |
|------|-----------------------------|---------------------------------|------------------------|--------------------------------|
| iOS, shared (Compose) app | `NSUserDefaults` (`UserDefaultsKeyValueStore`, `IosPlatformServices.kt`) | **Yes.** The preferences plist is in iCloud and Finder backups; `NSUserDefaults` has no per-key exclusion | Keychain, `kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly` (`IosSecretKeyStore.kt:76`) | **On the same device, yes** (Apple: a `ThisDeviceOnly` item "does not migrate to a new device", so it is absent only after restoring another device's backup) |
| iOS, Swift app (retired by #946, 2026-11-22) | Was one file per key in `Application Support/Protected/` (`ProtectedDefaults.swift`), not sealed with `at_rest.rs` | **No:** each file and the folder were marked `isExcludedFromBackup` | n/a | n/a |
| Android | App storage | **No:** `allowBackup="false"`, `fullBackupContent="false"` and data-extraction rules (`AndroidManifest.xml:47-49`) | Android Keystore (not backed up) | No |
| macOS desktop | Files in `~/Library/Application Support/NeurOne` (`FileKeyValueStore`) | **Yes** by default (Time Machine), no exclusion is set | Login keychain | Not by a folder restore: the keychain item is untouched, so a restored older folder opens under the current key |
| Windows desktop | Files in `%LOCALAPPDATA%\NeurOne` | Not by File History's defaults (AppData is not a library); other tools may | Credential Locker | Same as macOS if a tool restores the folder |
| Linux desktop | Files in the XDG data directory | Whatever the person's tool backs up | Secret Service | Same as macOS |

**So the finding is confirmed for the shared iOS app (a whole-device restore on the same phone) and for the desktop app (a
restore of the data folder, the key never having left).** It does not reach Android, nor the retired Swift iOS app; **since #946 the shared app is the only iOS app**. On iOS a whole-device
restore also rolls back the keychain, so on that host nothing the app keeps locally can tell an older state from a newer one:
only keeping the store out of the backup does.

What already limits the harm: research consent grants are never carried by a migration (`SWR-MG-04`), the withdrawal record
travels in the UHDR backup (`SWR-MG-03`), and a study still has to pass `ConsentEngine.admit()`. None of these stops a restored
local record from reading as current.

## 3. Options

**A. Keep the host stores out of OS backups (a host change, no format change).** iOS: move the shared app's key-value store from
`NSUserDefaults` to one file per key under Application Support marked `isExcludedFromBackup`, as the retired Swift app did.
macOS: set the Time Machine exclusion on the data folder (`com.apple.metadata:com_apple_backup_excludeItem`, as `tmutil
addexclusion` does). Windows and Linux: document that the folder should not be restored on its own. *Cost:* a whole-device restore
brings back no app state; the person restores the app's own UHDR backup (`SWR-MG-03`) for the history and the withdrawal record,
and is asked about consent again (grants are never carried). One migration step for installs that have data in `NSUserDefaults`.
*Does not reach:* a backup tool that ignores the exclusion.

**B. Bind a write generation into the associated data (a format change).** Each sealed value carries a counter, and the highest
counter written is kept beside the key. A value below it does not open. *Cost:* a new at-rest format and a migration of every
sealed value on every host. *Does not reach:* an iOS whole-device restore, which rolls the keychain back with the store, so the
counter rolls back too. It reaches the desktop folder restore.

**C. A and B together.** A for iOS, B as defence in depth where the key does not roll back with the data (desktop).

**D. Accept it.** A restore is the person's own act and returns the app to a state the person once had. Say so in the
instructions for use and in the restore screen. *Cost:* a withdrawal can be undone without the person deciding it, which
`CLAUDE.md` §6.2 treats as permanent.

**Recommendation:** A, because it is the only option that reaches the iOS case, it changes no format, and it matches what the
retired Swift app was built to do. B is worth having only if the desktop case is judged material.

## 4. What is assumed

- The Apple behaviour of `ThisDeviceOnly` keychain items in a same-device restore is taken from Apple's documentation and was
  not tried on a device.
- That the shared iOS app will ship; today it does not offer the presence gate (`OFFER_PRESENCE_GATE = false`).

## 5. What to send back

The option, the rationale, what would reopen it, and your name, role and the date. Nothing in the repository acts on this until
the answer is recorded in `OI-PRIV-66` and in `docs/status/completed-decisions.md`.
