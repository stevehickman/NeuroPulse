# Privacy Analysis — Companion Apps (app/)

**Project:** NeurOne  
**Document:** NP-PRIV-ANALYSIS-006  
**Revision:** 5
**Date:** 2026-11-26  
**Status:** DRAFT  
**Effective Date:** —  
**Author:** NeurOne Software Engineering  
**Approved By:** — (new document)  
**References:** NP-PRIV-AUDIT-001, NP-PRIV-ANALYSIS-002, NP-PRIV-ANALYSIS-003, NP-PRIV-REM-001, NP-APP-PRESENCE-001, NP-APP-TELEMETRY-001, NP-SW-001, CLAUDE.md §5, §6, §23  
**Related Issues:** GitHub #727, #889, #935, #936, #1060  
**Gate:** —  
**IEC 62304 Class:** —  
**Jurisdiction Scope:** US federal (FTC Act §5, FTC HBNR, HIPAA for T2); US state (BIPA IL, MHMD WA, CCPA/CPRA CA); EU/EEA GDPR; UK GDPR + DPA 2018; Canada PIPEDA. Taken from the scope of NP-PRIV-AUDIT-001 and NP-PRIV-ANALYSIS-002; not re-confirmed with the principal.  
**Scope:** `app/ios` (SwiftUI), `app/NeurOneUI` (Compose Multiplatform: shared, desktop, web, iosApp), `app/android`, `app/web` (React), `app/windows` (C#), `app/NeurOneShared`, at commit `201b992`. `common/` is out of scope: it had its own review on 2026-11-15 (`OI-PRIV-01` to `OI-PRIV-20`).

---

## 1. Purpose and method

This is a privacy analysis of the companion apps under `app/`, and a comparison of what it found with the issues
already recorded. Three read-only passes read the code without seeing the issue record (SwiftUI; Compose Multiplatform
with Android; React, Windows and `NeurOneShared`). A fourth catalogued the existing record:
`docs/status/pending-decisions.md` (the `OI-PRIV`, `OI-ANALYTICS`, `OI-BACKUP`, `OI-EDF`, `OI-PRES`, `OI-CONSENT` and
`OI-SIMFW-62` and `-70` to `-73` rows), `docs/status/completed-decisions.md`, NP-PRIV-AUDIT-001, NP-PRIV-ANALYSIS-002 and -003,
NP-PRIV-REM-001, NP-APP-PRESENCE-001, NP-APP-TELEMETRY-001, NP-ANALYTICS-001 and the GitHub issues. Every High and Medium
finding below was re-read in the code by the session that wrote this document. Nothing was built or run.

Each finding says whether the existing record already holds it (**New**, **Partial**, or the item that holds it).
§4 lists what the passes found again that the record already holds, so that it is not raised twice.

**Open items raised:** F-03, F-04 and F-05 are `OI-PRIV-59`, `OI-PRIV-60` and `OI-PRIV-61` in
`docs/status/pending-decisions.md` §13.2, and F-13 is `OI-PRIV-78` (§8.1). F-08 is point (6) of `OI-CONSENT-08` (§8.1). The other findings are fixes or extensions of existing items (§6).

## 2. Summary

Sixteen findings are new or only partly covered: **2 High, 9 Medium, 5 Low**. Pure privacy failures predominate:
the apps hold data where they should not, or record consent they did not obtain. Most of the serious gaps are already
open items (the presence gate is off on every host, the analytics relay is not built, transfers to and from the headset
are not encrypted end to end, backup metadata). Much of the exposure is latent for now: no `track(` call exists in any app,
and the SwiftUI and Windows apps are build-only, a state `OI-UI-KMP-05` accepts only until the first external
distribution.

**The highest-leverage change:** the SwiftUI research-consent flow turns analytics on whatever the person chose (F-01),
and saves toggled consents on Skip (F-10). Both should be fixed before any TestFlight build.

## 3. Findings

| # | Severity | Category | Finding | Existing record |
|---|---|---|---|---|
| F-01 | High | Pure privacy | SwiftUI: Done on research consent turns analytics on | New |
| F-02 | High | Both | SwiftUI: profiles, limits, protocols in `Documents/`, backed up and browsable | Partial (`OI-EDF-01`, `OI-UI-KMP-05`) |
| F-03 | Medium | Pure privacy | One consent store, history and biometric flag per install, several profiles | New: `OI-PRIV-59` |
| F-04 | Medium | Pure privacy | Warranty owner's choices stored in the user's data; owner sees usage | New: `OI-PRIV-60` (related to `OI-PRIV-20`) |
| F-05 | Medium | Security | Unsealed run stores the automatic-backup secret in the clear | Partial (`SWR-MG-13` (b)): `OI-PRIV-61` |
| F-06 | Medium | Pure privacy | Recording filenames carry the session time to the millisecond | New |
| F-07 | Medium | Pure privacy | Web: helmet serial survives into the shared limits file | Gap in closed `OI-PRIV-11` |
| F-08 | Medium | Pure privacy | Clinician use cases derive tier sets, not each one's minimum | Partial: now `OI-CONSENT-08` (6) (§8.1) |
| F-09 | Medium | Pure privacy | Compose analytics switch sits outside the consent model | New |
| F-10 | Medium | Pure privacy | Skip on research consent saves what was toggled | New |
| F-11 | Medium | Security | Firmware, EDF-request and calibration writes skip the encrypted-link check | Not in `OI-PRIV-17` (4) |
| F-12 | Low | Both | Recovery code can be copied; no screen-capture protection | New |
| F-13 | Low | Pure privacy | Phones have no erase, on a premise false for iOS | Challenges `SWR-MG-17`: `OI-PRIV-78` (§8.1); related to #935, #936, `OI-ANALYTICS-01` |
| F-14 | Low | Both | React app: no CSP; plaintext backup and individual-limits exports | New |
| F-15 | Low | Pure privacy | iOS consumable notification body visible on Touch ID lock screens | Partly reopens NP-PRIV-ANALYSIS-003 LOW-1 |
| F-16 | Low | Mixed | Smaller items (pasteboard, privacy manifest, condition links, others) | Mostly new |

### F-01 [HIGH] SwiftUI: Done on research consent turns analytics on

**Where:** `app/ios/NeurOne/Views/ConsentOnboardingView.swift:366-378`; revoke reachable only from
`app/ios/NeurOne/Consent/ConsentStore.swift:247`, `:279`, `:510`.  
**Category:** Pure privacy failure.  
**Issue:** `commitAndDismiss()` defaults `grantResearchAnalytics` to `true`, and Done calls it with the default. Unless
the same commit withdraws blanket consent, it writes `RESEARCH_ANALYTICS_KEY` and calls `ResearchAnalyticsGate.configure()`.
That happens even when every layer (L1 to L4) is off. The completion screen does not mention analytics. The only revoke,
`revokeResearchAnalytics()`, runs only on a blanket (L3) true→false transition, so a person who never granted L3 has no
way to turn analytics off. A person who withdrew L3 earlier and later reopens Research Preferences to change one
category turns analytics back on with Done, because `isBlanketWithdrawal` looks only at the current commit. No `track(`
call exists, so today the exposure is the SDK start-up (network address, `distinct_id`). The consent itself is invalid
regardless. The Compose app does this correctly: a separate switch, off by default (`SettingsScreen.kt:366-380`, though
see F-09).  
**Reference:** GDPR Art. 7 and Art. 25(2) (privacy by default); *Bundled Consent* and *Opt-out Default* anti-patterns;
CLAUDE.md §6.2 (L2 is scope, L3 is posture).  
**Remediation:** Done and Skip never write `RESEARCH_ANALYTICS_KEY`. Analytics gets its own switch, off by default, with
its own disclosure, as in Compose, and a revoke reachable without L3.

### F-02 [HIGH] SwiftUI: profiles, limits and protocols are plaintext in `Documents/`

**Where:** `app/ios/NeurOne/Protocol/NPLimitsStore.swift:147-160` (`URL.documentsDirectory`) and its writes;
`app/ios/NeurOne/Protocol/NPProtocolLibrary.swift:91`; `app/ios/SupportingFiles/Info.plist` (`UIFileSharingEnabled`,
`LSSupportsOpeningDocumentsInPlace`).  
**Category:** Both.  
**Issue:** `np_profiles.json` (a name and a note of up to 100 characters), `np_individual_limits.json` (per-person
stimulation limits, from which a condition can be inferred), `np_active_profile.json`, `np_helmet_limits.json` (helmet
serials) and `np_user_protocols.json` are pretty-printed JSON in `Documents/`. They have no backup exclusion and no
explicit protection class. So they go into iCloud and iTunes backups, show in the Files app and over a paired computer's
file sharing, and put the owner's helmet data and the users' data in one backup. This conflicts with `REQ-SW03-18` (a),
which keeps every small value the iOS app holds out of iCloud, and with CLAUDE.md §6.0 (the owner's data never travels
with a user's).  
**Existing record:** `OI-EDF-01` names `Documents/` exposure but only for `UHDRBackup/` and `shdr_staging.bin`.
`OI-UI-KMP-05` accepts that the SwiftUI app seals nothing while it is build-only. It does not accept cloud backup or
the Files app.  
**Reference:** *Sensitive Data in Unencrypted Client Storage*; *Backup Immortality*; GDPR Art. 32; MHMD WA.  
**Remediation:** Move these files into the backup-excluded `ProtectedDefaults` store (or Application Support with
`isExcludedFromBackup` and an explicit protection class). Extend `OI-EDF-01` to name them, so that the `Documents/`
decision covers everything in it.

### F-03 [MEDIUM] One consent store, history and biometric flag per install, several profiles

**Where:** SwiftUI `app/ios/NeurOne/NeurOneApp.swift:29` (`ConsentStore()`), `:42` (`SessionHistoryStore()`), `:72`
(`np.onboarding.bipa-accepted`); Compose `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/AppServices.kt:216`,
`:303`. Both apps hold several `NPIndividualProfile`s (`NPLimitsStore`).  
**Category:** Pure privacy failure.  
**Issue:** CLAUDE.md §6.0 says each patient on a clinic device has their own `ConsentStore`. The apps support several
individual profiles on one install, but research consent, the biometric release, the session history and the analytics
key are one each. One person's L3 grant or biometric release covers everyone who uses the install, and every profile
sees every other profile's history.  
**Reference:** CLAUDE.md §6.0; GDPR Art. 7 (consent is the data subject's own); BIPA §15(b) (written release per
subject); *Bundled Consent*.  
**Remediation:** A design decision: key consent, the biometric release and the history by profile, or limit an install to
one wearer and say so. Raised as `OI-PRIV-59`.

### F-04 [MEDIUM] The warranty owner's choices are stored in the user's data

**Where:** `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/WarrantyOwnerScreen.kt:84` (the "used"
marker), `:119` (`library.archiveFamily`); `common/npps/migration-keys.json:4` (`np.archived-families` is in
`carried.protocols`); SwiftUI `app/ios/NeurOne/Views/HelmetOwnerView.swift:97`.  
**Category:** Pure privacy failure.  
**Issue:** The owner's "offered" switch writes the user's archive key, so the owner's choices travel in the user's
migration and backup and are restored on the user's next device, against `REQ-SHDR-10`. The owner page also marks each
protocol family the wearer has run. That is user health data shown to whoever opens the owner page, a clinic for example.  
**Existing record:** `OI-PRIV-20` covers the owner store's own backup, restore and transfer. Neither point is among its
seven listed gaps.  
**Reference:** CLAUDE.md §6.0, `REQ-SHDR-10`; CLAUDE.md §5.1 (protocols run are UHDR).  
**Remediation:** Keep the offered set in `WarrantyOwnerStore`, per helmet, and drop the "used" marker from the owner page.
The same key also holds the user's own Archive and Restore (`ProtocolMenuScreen.kt:243-244`, `:265`), so each side can
undo the other. Raised as `OI-PRIV-60`.

### F-05 [MEDIUM] An unsealed run stores the automatic-backup secret in the clear

**Where:** `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/migration/ScheduledBackup.kt:61-62`
(`turnOn` writes `np.backup.auto-secret`); `.../migration/MigrationBackupModel.kt:217`; `.../AppServices.kt:102`
(`ProtectedStorage.open`); `.../PlatformServices.kt:153` (`reportsUnsealedStorage` defaults to false).  
**Category:** Security failure.  
**Issue:** A run with no usable keystore and nothing sealed yet is `UNSEALED` and uses the host's plain store
(`SWR-MG-13` (b), decided). On such a run, `turnOn` still stores the backup passphrase or code key, which
`common/core/.../data/BundleKey.kt:21` says belongs in the sealed store. By default it sits beside the backups it
protects, so anyone who copies the folder can open them. Phones never show the unsealed notice, because
`reportsUnsealedStorage` is false outside the desktop apps.  
**Reference:** *Hardcoded Credentials / API Keys* (secret beside ciphertext); Defence in Depth; CLAUDE.md §5.1.  
**Remediation:** Call `turnOn` only when `storageProtection == SEALED`, and say why when it is refused. Report the
unsealed state on every host that keeps data on the device. Raised as `OI-PRIV-61`.

### F-06 [MEDIUM] Recording filenames carry the session time to the millisecond

**Where:** `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/session/EdfDownload.kt:30`
(`"session_" + sessionId + ".edf"`); SwiftUI `app/ios/NeurOne/Session/EDFDownloader.swift:109`.  
**Category:** Pure privacy failure.  
**Issue:** The hub session ID is a Unix-millisecond timestamp. The session history refuses to keep it, and NP-SW-001
Rev 73 removed the same name from the backup archives (`OI-BACKUP-01`). The live recordings folder still names each
file by it, so the folder lists exactly when each session ran. On iOS the folder is visible in the Files app
(`OI-EDF-01`).  
**Reference:** LINDDUN Linkability and Detectability; GDPR Art. 5(1)(c).  
**Remediation:** Name recordings by a random ID, and keep the ID-to-session map in the sealed store.

### F-07 [MEDIUM] Web: the helmet serial survives into the shared limits file

**Where:** `app/web/src/components/LimitsSettings.tsx:84`, `:123` (default name `` `Helmet ${serial}` ``);
`app/web/src/lib/limitsStore.ts:205-211` (share form, filename from `name`).  
**Category:** Pure privacy failure.  
**Issue:** The share form strips `helmetId` and `individualId` (`SWR-WO-07`) but keeps the name, which by default holds
the serial. It is written as `limits "Helmet NP-001"`, and the downloaded file is named `helmet-np-001.npps`. A
recipient can tie the file to one helmet, and through it to the warranty owner. The editor states the opposite: "no
helmet serial, no individual id (SWR-WO-07 (f))" (`LimitsSettings.tsx:754`).  
**Existing record:** a gap in `OI-PRIV-11`, closed 2026-11-19 (GitHub #889 is still open).  
**Reference:** *Linkable Identifiers*; CLAUDE.md §5.1.  
**Remediation:** Default the name to one without the serial, build the filename from the level, and warn on share when
the name or description contains the serial. Check whether the Compose and SwiftUI limits editors default the same way.

### F-08 [MEDIUM] Clinician use cases derive tier sets, not each one's minimum

**Where:** `app/NeurOneShared/Resources/clinician-use-cases.json:18-24` (`hrv_outcomes`), `:10-14` (`eeg_review`).  
**Category:** Pure privacy failure.  
**Issue:** CLAUDE.md §6.1 says the system derives the *minimum* UHDR elements for the chosen use cases. Each use case
lists its whole tier's set. An HRV-outcomes grant also gives the clinician raw EEG waveforms and the raw PPG signal, and
EEG review also gives PBM dose logs.  
**Existing record:** `OI-CONSENT-08` reviews the tier tables. Its point (3) asks whether each use case's elements match its
tier, not whether they are its minimum.  
**Reference:** HIPAA Minimum Necessary Rule (T2); GDPR Art. 5(1)(c); CLAUDE.md §6.1.  
**Remediation:** Add per-use-case minimality to `OI-CONSENT-08`. Reduce each use case to what it needs, and make each
extra element its own use case.

### F-09 [MEDIUM] Compose analytics switch sits outside the consent model

**Where:** `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/SettingsScreen.kt:366-380`.  
**Category:** Pure privacy failure.  
**Issue:** The switch is off by default, which is correct. Turning it on writes the key with no disclosure naming the
vendor, the pseudonymous ID or what is sent. Nothing is recorded in `ConsentStore`, so "never asked" and "said stop"
read the same, against CLAUDE.md §6.0. It can be turned on while L3 is withdrawn. `checked` reads `isOpen`, which is
not observable state, so the switch does not redraw when tapped.  
**Reference:** GDPR Art. 7(1) (demonstrable consent), Art. 13; *Lawful Consent*.  
**Remediation:** Record the grant and withdrawal in `ConsentStore` with timestamps, show a disclosure before the first
grant, decide whether a withdrawn L3 blocks it, and back the switch with observable state.

### F-10 [MEDIUM] Skip on research consent saves what was toggled

**Where:** SwiftUI `app/ios/NeurOne/Views/ConsentOnboardingView.swift:57-58` (comment "Skip defers all data
decisions", then `commitAndDismiss(grantResearchAnalytics: false)`, which calls `updateResearchConsent(draft)`) and
`app/ios/NeurOne/NeurOneApp.swift:150-151` (`consentOnboardingShown = true` when the sheet opens); Compose
`app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/ConsentOnboardingScreen.kt:96-98`.  
**Category:** Pure privacy failure.  
**Issue:** A person who turns on L3, L1 with a contact address, or categories, and then presses Skip, has granted them.
Nothing about "Skip" says that. On iOS this contradicts the code's own comment. On iOS, onboarding is also marked shown
when the sheet opens, so if the app is killed mid-flow the person is never asked again, and that state cannot be told
apart from a completed flow.  
**Reference:** GDPR Art. 4(11) (an unambiguous affirmative act); *Dark Patterns*.  
**Remediation:** Skip discards the draft (or confirms first), and only the final step commits. Mark onboarding shown
when the flow ends.

### F-11 [MEDIUM] Firmware, EDF-request and calibration writes skip the encrypted-link check

**Where:** Compose `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ble/NeurOneGattManager.kt:704-710`
(EDF request, OTA command); SwiftUI `app/ios/NeurOne/BLE/NeurOneGATTManager.swift:435`, `:449`, `:461` (EDF, OTA,
calibration) and `:706-713` (connects to the first peripheral advertising the service).  
**Category:** Security failure (CLAUDE.md §23).  
**Issue:** The protocol upload and the other writes that `OI-PRIV-17` (4) lists wait for the encrypted serial read.
These do not. The hub refuses unencrypted writes (`REQ-FWHUB-81`), so this is defence in depth. The app still sends the
firmware image and the session ID to a peripheral it has not shown to be the paired hub, and iOS does no identity check
before connecting.  
**Reference:** CLAUDE.md §23 (firmware is named, and no clear path may exist); Zero Trust.  
**Remediation:** Gate OTA, EDF and calibration writes on the encrypted read, as `ProtocolUploader` does. Add them to
`OI-PRIV-17`.

### F-12 [LOW] Recovery code can be copied; no screen-capture protection

**Where:** `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/MigrationBackupScreen.kt:159`,
`MigrationLinkSendScreen.kt:80` (`SelectionContainer`); `app/android/.../MainActivity.kt` (no `FLAG_SECURE`); the iOS host
(no snapshot blur).  
**Category:** Both.  
**Issue:** The recovery code, which with a backup opens all UHDR, can be copied into clipboard history and clipboard
sync. The code, consent and history screens appear in recents thumbnails and screenshots.  
**Reference:** OWASP MASVS-PLATFORM; Defence in Depth.  
**Remediation:** Set `FLAG_SECURE` on those screens, blur the iOS app-switcher snapshot, and drop `SelectionContainer`
or mark the clip sensitive and clear it.

### F-13 [LOW] Phones have no erase, on a premise false for iOS

**Where:** `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/PlatformServices.kt:188-196`; NP-SW-001
`SWR-MG-17` ("a phone's data goes with the app").  
**Category:** Pure privacy failure.  
**Issue:** iOS Keychain items survive an uninstall (the UHDR seed, warranty tokens, the signing key). An analytics ID
someone opted into is never sent to the erasure relay when the app is removed. The SwiftUI app has no erase either;
its only deletion is `SessionHistoryStore.clearHistory()`.  
**Reference:** GDPR Art. 17; *Backup Immortality*.  
**Remediation:** Offer `EraseDeviceModel` on phones, or purge Keychain items on first launch after a reinstall and drain
the erasure queue at erase. Revisit the premise of `SWR-MG-17` together with #935 and #936.

### F-14 [LOW] React app: no CSP; plaintext backup and individual-limits exports

**Where:** `app/web/index.html` (no CSP; the Compose web app has one); `app/web/src/lib/protocolBackup.ts`;
`app/web/src/lib/limitsStore.ts:205-211`.  
**Category:** Both.  
**Issue:** The React app keeps UHDR in page memory with no CSP backstop. No XSS sink was found. The principal chose
memory-only with a backup file (NP-SW-001 Rev 89). That file, and the individual-limits download, are plaintext, and
whether that is an unsealed copy under CLAUDE.md §5.1 was not weighed.  
**Reference:** Content Security Policy; CLAUDE.md §5.1.  
**Remediation:** Ship the Compose web app's CSP. Ask the principal whether the full-form backup file should be sealed
with the existing bundle path (`common/lib/bundleKey.ts`).

### F-15 [LOW] iOS consumable notification body visible on Touch ID lock screens

**Where:** `app/ios/NeurOne/Consumable/ConsumableTracker.swift:119-121`.  
**Category:** Pure privacy failure.  
**Issue:** NP-PRIV-ANALYSIS-003 LOW-1 moved the consumable name from the title into the body, relying on previews being
hidden until unlock. That is the default only on Face ID devices. On Touch ID devices the body ("Intranasal Sleeves …")
shows on the lock screen.  
**Remediation:** A generic body, with the detail shown in the app; or a category with `hiddenPreviewsBodyPlaceholder`.

### F-16 [LOW] Smaller items

| Item | Where |
|---|---|
| iOS copies the protocol script to the general pasteboard, which reaches other devices through Universal Clipboard | `app/ios/NeurOne/Views/Protocol/ProtocolScriptEditorView.swift:35` (use `.localOnly` and an expiry) |
| `PrivacyInfo.xcprivacy` declares HealthFitness and Crash Data as collected, though neither leaves the device and no crash reporter exists | `app/ios/NeurOne/PrivacyInfo.xcprivacy:84`, `:98` (near `AUDIT-03`) |
| Condition links from an imported file may carry a tracking query string; the dialog shows only the host | `app/web/src/lib/conditionLink.ts:88`, `app/web/src/components/ConditionLinkDialog.tsx:102` |
| Revoking EEG consent leaves the downloaded recordings and their backups | `app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/SettingsScreen.kt:174-178` |
| The web profile-note hint "Notes (optional)" invites clinical notes, against the model's "a short label" | `locales/en.json` `WEB_PROFILE_NOTES_PLACEHOLDER` |
| The Vite dev server's `fs.allow` covers the whole repository | `app/web/vite.config.ts:138` |
| The web sign-in popup keeps `window.opener` | `app/NeurOneUI/web/src/wasmJsMain/kotlin/life/neurone/web/WebAuthorizationLauncher.kt:52` |
| Windows keeps the bundle secret and signing seed in managed strings, and regenerates the DPAPI seed on a bad read (latent: no entry point) | `app/windows/NeurOne/Protocol/BundleKey.cs:65-71`, `SessionProtocol.cs:57`, `:71-84` (the `OI-PRIV-12` analogue) |

## 4. Found again: already in the record

| What the passes found | Held by |
|---|---|
| The presence gate is off on every host; the SwiftUI app is never gated and seals nothing under the biometric key (`UHDRKeyManager.authenticate()` has no caller) | `OI-PRES-05`, `-06`, `-07`; `OI-PRIV-19`; `OI-UI-KMP-05` |
| `UIFileSharingEnabled` exposes `Documents/` in both iOS hosts, including `UHDR/` and `shdr_staging.bin` | `OI-EDF-01` (1) |
| EDF+ recordings are unsealed at rest on every host | `OI-EDF-01` (2) |
| SwiftUI warranty consent is per install, and the enrolment survives a revoke | `OI-PRIV-20` (5) |
| PostHog sees the network address and the SDK's own context | `OI-PRIV-05` (5); relay decided, not built |
| No payload encryption; none on USB-C CDC | `OI-SIMFW-70` to `-73` |
| Backup size, count and file-time metadata | `OI-BACKUP-01`, `OI-BACKUP-02`, `OI-PRIV-09` (1) |
| Windows key material in memory that cannot be wiped | `OI-PRIV-12` (the core side) |

## 5. What looks good

- Android excludes every domain from cloud backup and device transfer (`allowBackup="false"`, data-extraction rules),
  and scans for Bluetooth with `neverForLocation` and no location permission.
- The React app keeps nothing in browser storage, purges keys an earlier build wrote, and loads no third-party script,
  font or analytics. Its lockfile resolves only to the npm registry, with integrity hashes.
- `SHDRUploader` has no reference to `ConsentStore` on either app; fleet upload goes over Oblivious HTTP and fails
  closed without a gateway key.
- L3 teardown fires on the true→false transition; select-all never sets L3; withdrawal timestamps are owned by the store.
- The SwiftUI `ProtectedDefaults` store is backup-excluded per file with an explicit protection class; Keychain items
  are `ThisDeviceOnly`.
- The session history keeps only the day; no date of birth is stored; the share form is produced by the core, not the app.
- `NeurOneShared` test data is synthetic.

## 6. Recommended next steps

1. **Fix F-01 and F-10 together** (about half a day): Done and Skip write no analytics key, Skip discards the draft,
   and analytics gets its own switch, off by default. Before any TestFlight build.
2. **Move the SwiftUI profile, limits and protocol files out of `Documents/`, and give recordings random names** (F-02
   and F-06, about a day). Extend `OI-EDF-01` to name the files.
3. **Decide and repair F-03, F-04 and F-05** (`OI-PRIV-59` to `-61`). F-03 and F-04 are design decisions under
   CLAUDE.md §6.0 and `REQ-SHDR-10`; F-05 needs a fail-closed guard on `turnOn`.
4. **Fix F-07, and add per-use-case minimality (F-08) to `OI-CONSENT-08`** (about an hour each).
5. **Route the Compose analytics switch through `ConsentStore` (F-09), and gate OTA, EDF and calibration writes on the
   encrypted read (F-11)**, adding the latter to `OI-PRIV-17`.

F-12 to F-16 follow these.

## 8. Repairs made after the analysis

> **Rev 2 (2026-11-22).** Four findings repaired in the Compose app and the React web app, each with a
> requirement row and tests. The SwiftUI parts were not changed: that app was removed by pull request #946, which closes F-01, F-02,
> F-15 and the SwiftUI halves of F-10, F-11 and F-13 with it.
>
> **Rev 4.** F-05 repaired in the Compose app, which closes `OI-PRIV-61`.
>
> **Rev 5 (2026-11-25).** F-04 repaired in the Compose app and the shared core; its second half waits on a
> decision (`OI-PRIV-60` stays open for it).

| Finding | Repair | Requirement | Tests | What remains |
|---|---|---|---|---|
| F-04 | The owner page's "offered" switch writes the owner's record for the connected helmet (`WarrantyOwnerRecord.withdrawnFamilies`, `WarrantyOwnerStore.setOffered`, in `np.warranty-owner.records`, which no user bundle carries), and the protocol library asks the owner's store whether a family is offered (`NPProtocolLibrary.isOffered`) and stores none of it. `np.archived-families` is now the person's own archive only: the person's Archive and Restore and the owner's switch no longer undo each other. The "used" marker is removed from the owner page | `SWR-WO-01`; `REQ-SHDR-10` (d) | `WarrantyOwnerOfferedTests`; `WarrantyOwnerScreenTest` | Whether a clinic owner may see usage with the wearer's consent: `docs/human-input/oi-priv-60-owner-sees-usage.md` (`OI-PRIV-60`). A family an earlier version archived from the owner page stays in the person's archive. The owner's own backup, restore and transfer: `OI-PRIV-20` |
| F-05 | `ScheduledBackup.turnOn` keeps the code or passphrase only on a run whose storage is sealed (`AppServices` passes `StorageProtection.SEALED` and the protected list), and refuses with nothing written otherwise; a copy in the clear left by an earlier unsealed run is removed at start (a sealed one is left alone); the backup screen says why the automatic backup is not offered (`MIGRATION_BACKUP_AUTO_UNAVAILABLE_UNSEALED`), and the backup itself is still made under what the person types. The unsealed notice is shown on every host that keeps state on disk, phones included: the `reportsUnsealedStorage` switch, which only the desktop apps set, is removed | `SWR-MG-13` (c), (e) | `ScheduledBackupTest`, `AutomaticBackupSealingTest`, `StorageNoticeTest` | The new string is English in every locale until translated. Not run on a device: no Android SDK or Mac in the session |
| F-07 | A new limits set is named for its level, never the helmet serial or the person, and a downloaded limits file is named for its level (React web `newLimitsSet`, `limitsFileName`). The Compose limits screen also named helmet limits after the serial and a person's limits after the person: both now take the tier's label | `SWR-WO-07` (g) | `limitsStore.test.ts`; `LimitsTiersTest` | Sets saved earlier keep their name until saved again |
| F-09 | The Compose analytics switch shows what is sent and turns analytics on only when the person confirms (`ConsentStore.grantResearchAnalytics`); turning it off after it was on is marked with the day by the store, and the screen shows it; the switch follows the screen's state | `REQ-CONSENT-28` | `ConsentStoreTests`; `ResearchAnalyticsSectionTest` | Whether analytics may be on while blanket consent is withdrawn: `OI-PRIV-77`. The three new strings are English in every locale until translated, and are marked for legal review |
| F-10 | Skip records nothing, on a first run and from Research Preferences | `REQ-CONSENT-27` | `ConsentOnboardingScreenTest` (both new tests fail against the old Skip) | — |
| F-11 | Every firmware command and chunk, every calibration command and the recording request wait for the encrypted serial read; the update, the setup step and the download say the hub is not paired (`HUB_NOT_PAIRED_NOTHING_SENT`) | `REQ-SW03-31` (d) | `UploadLinkGateTest`, `OtaManagerTest`, `SetupCoordinatorTest`, `EdfDownloadTest` | — |

### 8.1 Findings now tracked as open items

> **Rev 3 (2026-11-25).** F-08 and F-13 each need a decision before code changes, so they are recorded,
> not repaired. Nothing under `app/` changed.

| Finding | Tracked in | What it asks | Checked against `main` (2026-11-25) |
|---|---|---|---|
| F-08 | `OI-CONSENT-08` point (6); `docs/human-input/oi-consent-08-clinician-tiers.md` Q6; register row `UC-212`; GitHub #727 | Whether each use case names only the elements it needs. Proposed for the principal and the clinical adviser: adherence gets session timestamps and duration; EEG review gets EEG waveforms with them; HRV outcomes gets the HRV time series with them. Each dropped element becomes a use case of its own if a clinician needs it | Unchanged: `clinician-use-cases.json` still gives each use case its tier's set. `OI-PRIV-64` (1) made widening a grant follow the use cases, which leaves the table as it was. The JSON is not edited, because the review precedes any edit and a change alters what users are told a clinician can learn (`CLAUDE.md` §6.1) |
| F-13 | `OI-PRIV-78` (new); `docs/human-input/oi-priv-78-phone-erase.md`; GitHub #1060 | Whether phones get the in-app erase (`SWR-MG-17` offers it on computers only), whether iOS purges its Keychain items after a reinstall, and whether people are told to turn analytics off before removing the app | Partly. The shared iOS host keeps only the data key and the gated UHDR key in the Keychain. The finding's list (UHDR seed, warranty tokens, signing key) was the SwiftUI app's, deleted by #946. The sealed values go with the app container. Not limited to iOS: on both phones, removing the app sends no analytics ID to the erasure relay and revokes no cloud grant. #935 is closed (an erase now destroys the gated key, #944); #936 and `OI-PRES-07` are open |

## 7. Not reviewed

- Behaviour at run time: nothing was built or run, there is no Mac or Android SDK in the session, and what the PostHog
  SDK sends at start-up was not observed.
- Hub-side EDF content (the `patientID` field) and what the hub writes into the SHDR blob.
- Production hosting headers for the web app; Windows file ACLs.
- `common/` internals, reviewed separately on 2026-11-15.

## Revision history

Revisions before the first row below were not recorded in a table in this document.

| Rev | Date | Author | Description |
|---|---|---|---|
| 2 | 2026-11-25 | NeurOne Software Engineering | **§8 added: F-07, F-09, F-10 and F-11 repaired in the Compose app and the React web app, with their requirements, tests and what remains; `OI-PRIV-77` raised from F-09.** |
| 3 | 2026-11-25 | NeurOne Software Engineering | **New §8.1: where F-08 and F-13 are now tracked, each checked against `main`. F-08 is point (6) of `OI-CONSENT-08`, with a proposed minimum per use case (Q6 of its brief, register row `UC-212`). F-13 is new `OI-PRIV-78` (GitHub #1060, brief `oi-priv-78-phone-erase.md`). The iOS Keychain residue on the shared host is the data key and the gated key only, and the analytics ID and the cloud grant are left behind on both phones. §1, §3 and the header point to them.** |
| 4 | 2026-11-25 | NeurOne Software Engineering | **§8: F-05 repaired in the Compose app (`SWR-MG-13` (c), (e)), with its tests and what remains; closes `OI-PRIV-61`.** |
| 5 | 2026-11-26 | NeurOne Software Engineering | **§8: F-04 repaired in the Compose app and the shared core (the offered set in the owner's store per helmet, the used marker removed), with what remains: the principal's answer on whether a clinic owner may see usage.** |
