# OI-PRES-07 — run the presence gate on an iPhone (with OI-UI-KMP-06's first device run)

**Status:** DRAFT, not sent. **From:** Software (Claude Code session, 2026-10-22). **To:** whoever holds the device; unnamed.

**What is asked.** Run the iOS presence-gate binding on an iPhone, and the first device run of the shared iOS host (`OI-UI-KMP-06`), and report each result. A pass or a fail with the device, OS version and log line for each box. Nothing here is a decision, and nothing in the repo may act as if it has passed: `OFFER_PRESENCE_GATE` stays `false` until it has (`docs/np_app_presence_001.md` §7).

**What is known and what is assumed.** The test points come from `docs/np_app_presence_001.md` Rev 9 §6–§7 and `REQ-PRES-01` to `-09`. The steps were written without a device: the Xcode background-task command, what the Files app shows, the behaviour at the end of the 15-second window and the notification behaviour while locked are general knowledge, not checked against the code. Where the result differs, that is a finding.

**What to send back.** Pass or fail per box; device model, OS version and (iOS) Xcode version; the exact prompt type seen; and any log line for a failure. If all of it passes, say so and name who ran it and when, so the open item can be closed in the owning file and logged in `docs/status/completed-decisions.md`. A failure is an open finding: do not flip the flag. Do not include any health data from the device (CLAUDE.md §5).

Section 0 below is shared with `oi-pres-06-android-device-run.md`.

## 0. Common set-up (both platforms)

- [ ] Branch from `main`. In the **test build only**, flip the flag to `true`:
  - Android: `OFFER_PRESENCE_GATE` in `app/android/app/src/main/kotlin/life/neurone/app/NeurOneApplication.kt:126`
  - iOS: `OFFER_PRESENCE_GATE` in `app/NeurOneUI/shared/src/iosMain/kotlin/life/neurone/shared/IosPlatformServices.kt:128`
  - Never commit it as `true` until the boxes below pass (the open items say `OFFER_PRESENCE_GATE` stays false until then).
- [ ] You need a device **with a screen lock set** (PIN/passcode), one **biometric enrolled**, and a second biometric you can add/remove.
- [ ] Use a fresh install, then a second pass over an **existing install with data** (for the migration, REQ-PRES-07).
- [ ] Before turning the gate on, create some UHDR-class data: finish onboarding, tick a research consent category,
      and (if you have no hub) note that session history will be empty — that is fine; consent state is enough to prove the migration.
- [ ] Have a **backup made under a recovery code or passphrase you wrote down** (REQ-PRES-08): the app will insist on it.

## 1. iOS (OI-PRES-07, and the first device run of the shared iOS host, OI-UI-KMP-06)

Requires a Mac with Xcode (the project compiles on `macos-15` CI with Xcode 16.4), an iPhone on iOS 17+ with a passcode and Face ID/Touch ID, and an Apple developer team for signing.

### 1a. Build and install
- [ ] `brew install xcodegen`; `cd app/NeurOneUI/iosApp && xcodegen generate --spec project.yml`.
- [ ] Open `NeurOneUI.xcodeproj`; set your **Team** and a unique bundle id for the test (the real id `life.neurone.app` is fine if your team owns it).
- [ ] Capabilities: **HealthKit is not needed** (the HealthKit read is retired). Background modes: *processing* (the backup task `life.neurone.ui.backup`).
- [ ] Build to the **iPhone** (not the simulator: the simulator has no Secure Enclave / background scheduler).
- [ ] First launch: trust the developer certificate on the phone if asked (Settings → General → VPN & Device Management).
- [ ] App launches; the hub link has never touched a radio (OI-UI-KMP-06) — a failure to connect without a hub is expected; note any crash.

### 1b. Turning it on and the prompt asking ONCE (REQ-PRES-01..03)
- [ ] Settings → **Lock my health data**: **Off**. **Turn on** without a backup → **Make a backup first**.
- [ ] Make the backup (recovery code/passphrase), turn on → **Face ID prompt appears once** (not twice), and the **Face ID usage string** reads "NeurOne asks for Face ID to confirm it is you before it opens your health data, if you turn that on."
- [ ] Kill the app (swipe up), reopen → app lock screen, one prompt, then content.
- [ ] Background <5 min → returns unlocked; >5 min → asks again.
- [ ] Pull down Control Centre / take an incoming call/system alert while open → **does not re-lock** (the foreground signal ignores will-resign-active).

### 1c. Passcode fallback with Face ID declined (REQ-PRES-04)
- [ ] At the prompt choose **Enter Passcode** (or decline Face ID in Settings → NeurOne → Face ID) → the **device passcode unlocks**.
- [ ] Wrong passcode → stays locked.

### 1d. A new face or fingerprint (key must stay usable)
- [ ] Add a second Face ID appearance (or Touch ID finger). Reopen → unlocks and **no data lost**.
- [ ] Unlock using the new one as well.

### 1e. Removing the passcode (key must be deleted)
- [ ] Turn the passcode off in system settings (this erases some on-device credentials; iOS will warn you).
- [ ] Reopen the app → **gate key gone**, UHDR unreadable, the app says so, nothing opens in the clear or ungated (REQ-PRES-09 b).
- [ ] Set a passcode again, **restore from the backup** → data returns.

### 1f. Migration (REQ-PRES-07) on an install with data
- [ ] Consent ticks, onboarding, anything in history before turning on; after turn-on + reopen + unlock: **all still present**.

### 1g. Cannot be turned off; erase returns to factory
- [ ] No switch to turn the gate off. **Erase** → gate **Off** and data gone.

### 1h. While locked (device tier)
- [ ] Lock the app (background >5 min) → a **consumable reminder** local notification still arrives (the permission prompt for notifications appears at first use); nothing personal is shown.
- [ ] Allow **Notifications** when asked; confirm the notification text carries only title/subtitle detail the app intends.

### 1i. Shared-host checks (OI-UI-KMP-06, host decisions of 2026-10-20/21)
- [ ] **Bundle id** is `life.neurone.app`.
- [ ] **Landscape**: rotate the **iPhone** to both landscapes; layout holds, nothing clipped.
- [ ] **iPad** (if you have one): side rail from 600 dp, content max width ~840 dp (UC-099).
- [ ] **Files app**: the app's Documents folder is visible (file sharing on); a backup made in-app appears there. (REQ-UHDRFP-01: the UHDR folder is *inside* Documents — confirm what the Files app shows, and note it: it is a known open question, OI-EDF-01.)
- [ ] **Backup**: make a health-data backup; open the folder and confirm **file names are random** and **file dates show 1 Jan 1970** (REQ-SW03-29 f, new in #680). Note any real date you see.
- [ ] **Automatic backup**: turn on, then lock the phone on charge overnight (or use Xcode → Debug → Simulate Background Fetch... for a `BGProcessingTask` use `e -l objc -- (void)[[BGTaskScheduler sharedScheduler] _simulateLaunchForTaskWithIdentifier:@"life.neurone.ui.backup"]` in the Xcode debugger console). Expect a backup file set to appear; a locked/unreadable store must make **no** backup and ask again.
- [ ] **Release screen**: onboarding's release screen now shows "what NeurOne will never do" (REQ-ONBOARD-03, restored in #681); **decline** path works.
- [ ] **VoiceOver on**: the coherence score is announced politely during a session (needs a hub; skip without one).
- [ ] **Locale**: switch phone language to es-419 and zh-Hans; text appears translated (the Compose resource fallback `values-es`/`values-zh` is unverified at runtime).
- [ ] **Hub link**: with a hub, the first time `IosBleCentral` touches a radio: scan finds the hub, connects, disconnect clears state. Record.

### 1j. Record
- [ ] Device, iOS version, Xcode version, pass/fail per box, Console.app logs filtered on `NeurOne` for any Keychain error codes (`errSec…`).

## 2. After the runs

- [ ] If every box in section 1 passes: close this item in `docs/np_app_presence_001.md` §7 and `docs/status/pending-decisions.md`, then flip `OFFER_PRESENCE_GATE` to `true` in a **separate** change (keep the flag change reviewable on its own).
- [ ] Any failure: do **not** flip the flag; open an item with the exact box, device and log line.
- [ ] Rule to keep in mind throughout (REQ-PRES-09): a fix must never weaken UHDR protection — no turn-off, no fallback that opens ungated, no shorter/easier passphrase.
