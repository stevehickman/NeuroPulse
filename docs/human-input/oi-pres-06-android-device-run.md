# OI-PRES-06 — run the presence gate on Android devices

**Status:** DRAFT, not sent. **From:** Software (Claude Code session, 2026-10-22). **To:** whoever holds the device; unnamed.

**What is asked.** Run the Android presence-gate binding on a device at API 29, 30 and 36 and report each result. A pass or a fail with the device, OS version and log line for each box. Nothing here is a decision, and nothing in the repo may act as if it has passed: `OFFER_PRESENCE_GATE` stays `false` until it has (`docs/np_app_presence_001.md` §7).

**What is known and what is assumed.** The test points come from `docs/np_app_presence_001.md` Rev 9 §6–§7 and `REQ-PRES-01` to `-09`. The steps were written without a device: the Xcode background-task command, what the Files app shows, the behaviour at the end of the 15-second window and the notification behaviour while locked are general knowledge, not checked against the code. Where the result differs, that is a finding.

**What to send back.** Pass or fail per box; device model, OS version and (iOS) Xcode version; the exact prompt type seen; and any log line for a failure. If all of it passes, say so and name who ran it and when, so the open item can be closed in the owning file and logged in `docs/status/completed-decisions.md`. A failure is an open finding: do not flip the flag. Do not include any health data from the device (CLAUDE.md §5).

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

## 1. Android (OI-PRES-06) — run on API 29, API 30 and API 36

Build: `cd app/android && gradle :app:assembleDebug` (needs SDK + NDK), install with `adb install -r app/build/outputs/apk/debug/*.apk`.
Use three devices or emulators (API 29, 30, 36). Emulators are fine for API behaviour, but run at least one **physical** device for the biometric sensor.

For **each** API level:

### 1a. Turning it on
- [ ] Settings → **Lock my health data** shows state **Off**.
- [ ] Press **Turn on** with no backup made → it shows **Make a backup first** and does not turn on (REQ-PRES-08).
- [ ] Make a backup under a recovery code/passphrase, return, **Turn on** → a system prompt appears.
- [ ] A line says it **takes effect the next time the app opens**; state reads **On**.

### 1b. The prompt (REQ-PRES-03, -04)
- [ ] Force-stop the app and reopen (cold start) → the **lock screen of the app** appears, no content composed behind it.
- [ ] **Biometric** prompt: fingerprint/face unlocks.
- [ ] **PIN-only**: cancel biometric, use the device PIN/pattern/password → unlocks. The prompt must **never be biometrics-only** (REQ-PRES-04).
- [ ] Wrong PIN/biometric → stays locked, no crash, no data shown.
- [ ] Background the app for **less than 5 minutes**, return → still unlocked. Background for **more than 5 minutes**, return → asks again (REQ-PRES-03, UC-105).
- [ ] Unlock, put the screen to sleep, wake → behaves as foreground/background as above.

### 1c. The 15-second window (UC-107)
- [ ] After a successful prompt, open the data-reading screens repeatedly for ~10 s → no second prompt.
- [ ] Wait **>15 s** after a prompt, then do something that needs the key (e.g. cold start of a screen reading consent) → a new prompt is shown, or the app stays locked until you unlock. Record exactly which, and the observed window length.

### 1d. Migration (REQ-PRES-07) — existing install with data
- [ ] Before the update/turn-on: note your consent ticks and onboarding state.
- [ ] Turn on, close the app, reopen, unlock → **every earlier value is still there** (consent ticks, withdrawal state, history if any). Nothing reset.
- [ ] Withdraw a consent **while locked** is not possible (content is not composed) — instead: unlock, change a consent, lock, unlock → the change survived.

### 1e. A new fingerprint (key must stay usable)
- [ ] Add a **second fingerprint/face** in system settings. Reopen app → still unlocks with the **old** one, and the key is still usable (no data lost).
- [ ] Unlock with the **new** one too.
- [ ] Remove the new one again → old one still works.

### 1f. Removing the screen lock (key must be lost; backup is the way back)
- [ ] Remove the screen lock in system settings (set to None). Reopen the app.
- [ ] Expected: the gate key is **gone**; UHDR data cannot be read; the app says so and does **not** open ungated or unsealed, and writes nothing in the clear (REQ-PRES-09 b).
- [ ] Set a lock again and **restore from the backup** you made under the recovery code → data returns.

### 1g. The gate cannot be turned off (REQ-PRES-09 a)
- [ ] Look in Settings → there is **no switch to turn it off**.
- [ ] **Erase** (Settings → erase) → device returns to factory state; after that the gate is **Off** again (OI-PRES-10), and a new run starts clean.

### 1h. What must still work while locked (device tier, REQ-PRES-05)
- [ ] With the app locked: a consumable reminder notification still fires; the fleet/warranty upload still runs (the device-tier list in NP-APP-PRESENCE-001 §4).
- [ ] Nothing about the person (consent, history, profiles) is shown while locked.

### 1i. Record
- [ ] For each API: Pass/Fail per box, device model, Android version, exact prompt type, and any logcat error (`adb logcat | grep -i "keystore\|biometric\|presence"`).

## 2. After the runs

- [ ] If every box in section 1 passes: close this item in `docs/np_app_presence_001.md` §7 and `docs/status/pending-decisions.md`, then flip `OFFER_PRESENCE_GATE` to `true` for that platform in a **separate** change (keep the flag change reviewable on its own).
- [ ] Any failure: do **not** flip the flag; open an item with the exact box, device and log line.
- [ ] Rule to keep in mind throughout (REQ-PRES-09): a fix must never weaken UHDR protection — no turn-off, no fallback that opens ungated, no shorter/easier passphrase.
