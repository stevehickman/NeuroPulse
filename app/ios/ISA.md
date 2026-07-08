---
task: "Core iOS app launch-ready for T1 hardware"
slug: 20260604-000000_ios-core-app-t1-launch
project: NeurOne
effort: E4
effort_source: gate-floor
phase: execute
progress: 146/164
mode: interactive
started: 2026-06-04
updated: 2026-06-17
---

# ISA — NeurOne Core iOS App (Issue #51)

## Problem

The NeurOne T1 hardware ships Month 18 post-company-formation. The iOS app must be App Store-live by Month 12 to allow six months of beta validation before mass shipment. The current codebase (~44 Swift source files, ~10,600 lines) covers core BLE, session display, protocol scripting, consent, consumable tracking, OTA, and UHDR/SHDR data management — but several launch-critical items are absent or incomplete: the session stop command is a TODO stub, the privacy compliance screens required before EEG activation (biometric data written release) and before any data is collected (minimum age gate) are not implemented, the Session History screen does not exist, the analytics SDK initialisation gate is not wired, the Adaptive Adjustments transparency card is not implemented, and the app has never been submitted to App Store Connect. No App Store privacy nutrition label exists. Without these, the app cannot ship legally or pass App Store review.

## Vision

A T1 user unboxes their NeurOne, opens the iOS app, and within three minutes has completed consent onboarding, confirmed their first hardware connection, seen their eight EEG impedance contacts go green one by one, and started their first `alpha_calm` session watching live HRV coherence and a breathing ring slowly pulse. After the session ends they pull the EDF file to their phone with one tap and see their coherence trend building in Session History. The experience feels like a high-trust, precision health tool — calm typography, no alarm unless something genuinely needs attention, no dark patterns, no friction between the user and their own data. Every privacy decision the user makes is visible, revocable, and plain-language. The app is the reason a clinician recommends NeurOne over a competitor who ships a comparable headset.

## Out of Scope

The Apple Watch companion app (NP-APP-ROADMAP-001 Rev B Phases 1–4) is not part of this ISA — it is a Year 1 post-launch deliverable with its own development phases and open items. The T2 clinical cloud features (FHIR R4 EHR integration, multi-patient dashboard, sLORETA source imaging viewer, scripting API) are not in scope — they require T2 hardware and a separate 510(k) pathway. The research anonymisation engine firmware (NP-FW-ANON-001) runs on-device in firmware, not in the iOS app — the app's role is to display the study invitation and consent flow, not to execute the anonymisation pipeline. Android is not in scope for T1. Backend server infrastructure is not in scope for this ISA — server-side endpoints are a separate workstream. HealthKit write-back (pushing NeurOne session data into HealthKit) is not in scope — the current spec reads HRV SDNN for display only.

## Principles

- **User owns their data unconditionally.** Every data flow that touches UHDR requires explicit, revocable, plain-language consent. The app must never transmit UHDR to NeurOne or any vendor. The biometric-derived AES-256 key never leaves the device.
- **Privacy compliance is launch-blocking, not post-launch cleanup.** BIPA written release, minimum age gate, analytics SDK gate, and HealthKit data residency are engineering requirements that block App Store submission, not nice-to-haves.
- **Session safety is hardware-owned; the app is advisory.** The Safety MCU owns all stimulation GPIO. The app can request start/stop but must never assume a stimulation state it has not received confirmation of from the hub. A lost BLE connection during a session is not an app error — it is a safety event the hub handles autonomously.
- **Information density over decoration.** Session metrics (coherence score, RMSSD, zone module status, impedance pass count) are always visible during a session. No screen animations that obscure data. No gamification that distorts clinical signal.
- **No feature flags for regulatory requirements.** The BIPA screen, age gate, irreversibility notice, and adaptive adjustments card are not behind flags — they are structural fixtures of the consent and session history flows.
- **The app is a thin shell over hardware.** All dosage limits, charge density limits, and safety interlocks live in firmware. The app validates protocols before upload but defers all safety enforcement to the hub. App crashes must not affect ongoing stimulation.

## Constraints

- **iOS 17+ minimum.** Uses SwiftUI `NavigationStack`, `@Observable` (or `@StateObject`) patterns, `BGTaskScheduler` for nightly UHDR backup, and `CoreBluetooth` 5.3 LE Audio.
- **Swift 5.10 / Xcode 15+.** No Objective-C interop beyond CoreBluetooth bridging already in place.
- **BLE 5.3 LE only.** No Wi-Fi direct communication path to hub in the iOS app. Hub BLE GATT service UUID and characteristic layout as defined in NP-APP-ROADMAP-001 Rev B §5. Protocol blobs must be chunked at ≤512 bytes per BLE write.
- **No NeurOne server access to UHDR.** The `UHDRKeyManager` biometric-derived AES-256 key never leaves the device and is never transmitted to any NeurOne infrastructure or analytics vendor.
- **Analytics: `engagement_tier` enum only.** No raw session count or raw app launch count transmitted in any analytics event. `engagement_tier` is a 3-bucket coarsened enum (`new`/`active`/`established`) per NP-APP-TELEMETRY-001 Rev B §3.
- **Analytics and crash SDK initialisation gate.** No analytics or crash reporting SDK may call `init()` or `configure()` before the consent flow is complete for the current user. Per NP-APP-TELEMETRY-001 Rev B §5.
- **Biometric data written release required before EEG activation.** EEG neurofeedback and closed-loop adaptive stimulation must be disabled until the BIPA screen is accepted. Shown to all users regardless of location (OI-PA-03 resolved).
- **Minimum age gate: 16+ declaration required before any personal data is collected.** Gate must appear before the first screen that collects or displays personal data. The checkbox is not pre-ticked. Flow cannot proceed if unchecked.
- **Session protocol Ed25519 signature.** The hub firmware rejects unsigned or corrupted session protocol blobs (NP-FW-EMMC-001 Rev A §8). The app must sign every protocol blob before upload using the session signing key infrastructure already defined in the protocol validator.
- **App Store Health & Fitness category.** No Medical Device category — T1 is FDA-exempt general wellness. Regulatory footer `Text` must appear on every stimulation-adjacent screen.
- **HealthKit data residency.** HealthKit data (HRV SDNN, heart rate) read only for real-time session display. Not persisted beyond the active session. Not transmitted to NeurOne servers or any analytics vendor. Per NP-APP-ROADMAP-001 Rev B §9.1.

## Goal

The NeurOne iOS app is App Store-live at Month 12, passing App Store review on first submission, covering all core T1 session management, consent, consumable, OTA, and privacy compliance flows required to activate a T1 device — and is free of the legal blocking items (BIPA, age gate, analytics gate, adaptive adjustments transparency) identified in NP-PRIV-001 Rev B and NP-APP-ROADMAP-001 Rev B §9.

## Criteria

### Build and project structure

- [x] ISC-1: The Xcode project builds for iOS 17+ release configuration with zero errors and zero warnings on clean build.
- [x] ISC-2: The Xcode project builds for iOS Simulator (arm64) with zero errors and zero warnings on clean build.
- [x] ISC-3: SwiftLint runs with zero violations on the full source tree (`.swiftlint.yml` present and enforced in CI).
- [x] ISC-4: The `Info.plist` contains `NSBluetoothAlwaysUsageDescription`, `NSBluetoothPeripheralUsageDescription`, and `NSHealthShareUsageDescription` with NeurOne-specific strings (not placeholder text).
- [x] ISC-5: The BGTaskScheduler identifier `com.neurone.uhdr-backup` is declared in `Info.plist` under `BGTaskSchedulerPermittedIdentifiers`.
- [ ] ISC-6: The App Store privacy nutrition label (App Privacy section in App Store Connect) is complete and matches `NP-APP-TELEMETRY-001 Rev B` declared data types — no placeholder entries remain.
- [x] ISC-7: The app bundle ID is set to a production value (not `com.example.*` or any placeholder).
- [x] ISC-8: `Anti:` No `print()` calls exist in any release-target Swift source file (use `Logger` from `os` framework only).
- [x] ISC-9: `Anti:` No hardcoded IP addresses, URLs, or API keys exist in any Swift source file.
- [x] ISC-10: The minimum deployment target in the Xcode project file is iOS 17.0.

### BLE GATT connection layer

- [x] ISC-11: `NeurOneGATTManager` begins scanning automatically when `CBCentralManager.state == .poweredOn`, without requiring a manual user action.
- [x] ISC-12: On hub disconnect, `NeurOneGATTManager` automatically re-scans after a 2-second delay — verified by simulating a disconnect in the Xcode BLE simulator.
- [x] ISC-13: All fourteen GATT characteristics (9 NOTIFY + 5 WRITE, per `NPUUID.all`) from `GATTCharacteristics.swift` are resolved and `allCharacteristicsResolved` becomes `true`; `warrantyToken` is optional and does not block resolution.
- [x] ISC-14: Protocol blobs larger than 512 bytes are automatically chunked by `SessionProtocolUploader` before passing to `NeurOneGATTManager.uploadProtocol(_:)`.
- [x] ISC-15: `GATTParser.parseSessionState`, `parseSessionStatus`, `parseHRVCoherence`, `parsePacerPhase`, `parseImpedanceResult`, `parseConsumableStatus`, `parseOTAStatus`, and `parseZoneModuleStatus` each exist as static functions and return non-nil values for canonical test byte sequences.
- [x] ISC-16: `NeurOneGATTManager.connectionState` transitions correctly through `.disconnected → .scanning → .connecting → .connected` and is `@Published` — `SessionView` reacts without any explicit refresh.
- [x] ISC-17: `Anti:` `NeurOneGATTManager` does not hold a strong reference to any ViewController or View — connection lifecycle is fully decoupled from the view hierarchy.
- [x] ISC-18: The BLE central manager is initialised on `DispatchQueue.main` (existing) — no background-thread CBCentralManager initialisation path exists.
- [x] ISC-19: When Bluetooth is off, `SessionView` shows a user-visible "Enable Bluetooth to connect to your hub" message rather than a generic error or blank state.
- [x] ISC-20: On reconnect after disconnect-during-session, `SessionView` restores the last known session state from the GATT `SESSION_STATUS` read characteristic rather than resetting to idle.

### Session display — Mode 1 Connected

- [x] ISC-21: `SessionView` renders `connectionBanner`, `sessionStatusCard`, `zoneModuleRow`, and `sessionControls` without `@EnvironmentObject` injection errors at app launch.
- [x] ISC-22: `SessionView` shows `hrvBreathingRing` only when `gatt.session.status == .running` — it is absent at `.idle`, `.paused`, and `.completed`.
- [x] ISC-23: `SessionView` shows `liveMetricsGrid` only when `gatt.session.status == .running`.
- [x] ISC-24: The breathing ring animates between 80pt and 120pt diameter in response to `gatt.session.pacerPhase` changes (`.inhale` → expand, `.exhale` → contract) with `.easeInOut` 2.5 second duration.
- [x] ISC-25: `MetricCard` for Coherence shows the correct colour: green for ≥7, yellow for ≥4, orange for <4.
- [x] ISC-26: `MetricCard` for "EEG Contacts" displays the count of bits set in `gatt.session.impedancePassFlags` as `N / 8`.
- [x] ISC-27: The session stop command (`End Session` button) sends a GATT write to the hub's stop characteristic — the `// TODO` stub at `SessionView.swift:262` is replaced with a real `gatt.sendSessionStop(completion:)` call.
- [x] ISC-28: The `End Session` button is visible only when `gatt.session.status == .running`.
- [x] ISC-29: Tapping `End Session` presents a confirmation alert ("End this session?") before sending the stop command.
- [x] ISC-30: A session-completed state (`gatt.session.status == .completed`) surfaces a "Download Session Data" button that triggers `EDFDownloader.requestDownload(sessionID:)`.
- [x] ISC-31: The regulatory footer text appears on `SessionView` and matches the text in `CLAUDE.md §10` ("NeurOne is a general wellness device…").
- [x] ISC-32: `blockingConsumableAlert` renders when `consumable.sessionIsBlocked == true` and hides `sessionControls` and `liveMetricsGrid` completely.
- [x] ISC-33: Zone module row shows five slots with correct colour coding: green-tinted for present (non-zero GATT value), grey for absent.
- [x] ISC-34: `Anti:` The `SessionView` does not display a stimulation state that has not been confirmed by the hub — it never optimistically updates `gatt.session.status` on the app side before receiving a GATT notification.

### Session protocol upload — Mode 2

- [x] ISC-35: `SessionProtocolUploader.upload(_:)` serialises an `NPProtocolDefinition` to a binary blob, signs it with the session Ed25519 key, and sends it to `NeurOneGATTManager.uploadProtocol(_:)`.
- [x] ISC-36: `NPProtocolValidator` rejects any protocol whose per-modality dose limits exceed the values declared in `NPDosageLimits`.
- [x] ISC-37: `NPProtocolValidator` rejects any protocol whose tDCS current exceeds 2 mA or whose BES current exceeds 1 mA.
- [x] ISC-38: `NPProtocolValidator` rejects any protocol whose tDCS charge density exceeds 40 µC/cm² — confirmed by a unit test that constructs a borderline-valid and a borderline-invalid protocol.
- [x] ISC-39: `NPProtocolLibrary` loads and exposes all 19 predefined NPPS protocol templates from `NPPredefinedProtocols` without runtime errors.
- [x] ISC-40: `ProtocolMenuView` lists all protocols in `NPProtocolLibrary` with name, modality badges, and duration; protocols with missing hardware (e.g. a 1064nm zone not detected) are visually disabled with an explanatory note.
- [x] ISC-41: `ProtocolEditorView` allows editing frequency, duration, and current for each modality within the limits defined in `NPLimitsStore`, and blocks saving if any value violates a limit.
- [x] ISC-42: `ProtocolComposerView` allows building a multi-modality protocol by selecting modalities from a list and setting parameters individually.
- [x] ISC-43: `ProtocolScriptEditorView` accepts raw NPPS text, parses it via `NPProtocolScripting`, and shows per-line validation errors inline.
- [x] ISC-44: `LimitsSettingsView` displays all per-modality limits from `NPLimitsStore` with their defaults and allows no value to exceed the hardware maximum declared in `NPHardwareLimits`.
- [x] ISC-45: A successful protocol upload shows a confirmation toast ("Protocol sent to hub") that auto-dismisses after 2 seconds.
- [x] ISC-46: A failed protocol upload shows a persistent error banner with the GATT error description and a retry button.
- [x] ISC-47: `Anti:` `ProtocolEditorView` does not allow saving a protocol with zero duration or zero-current stimulation blocks — these are caught by `NPProtocolValidator` before upload.

### Session history and EDF download — Mode 4

- [x] ISC-48: A `SessionHistoryView` exists and is accessible from the Session tab (e.g. toolbar button or list below the current session card).
- [x] ISC-49: `SessionHistoryView` lists past sessions with date, protocol name, duration, and coherence score (where available), sourced from a local `SessionHistoryStore` persisted in `UserDefaults` or the app's `Documents` directory.
- [x] ISC-50: Tapping a session in `SessionHistoryView` navigates to a detail view showing per-session metrics: RMSSD, average coherence score, EEG impedance pass count, and a coherence trend sparkline for the last 30 sessions.
- [x] ISC-51: The session detail view contains an "Adaptive Adjustments" card when the session contained closed-loop adaptive events — rendered from a plain-language trigger enum per NP-APP-ROADMAP-001 Rev B §9.4.
- [x] ISC-52: The Adaptive Adjustments card shows at most 5 events with "and N more / View all" for longer lists; it never shows raw EEG band power values.
- [x] ISC-53: The "Download EDF" button in session detail triggers `EDFDownloader.requestDownload(sessionID:)`, shows a progress indicator during transfer, and saves the EDF+ file to the app's `Documents` directory.
- [ ] ISC-54: Downloaded EDF files are accessible via the iOS Files app (app's Documents directory is declared `UIFileSharingEnabled = YES` and `LSSupportsOpeningDocumentsInPlace = YES`).
- [x] ISC-55: `Anti:` The EDF+ patient header written by the hub (as specified in NP-FW-EMMC-002 Rev A §E) is not modified by the app — the opaque 16-char token, `X` sex/birthdate/name fields pass through unchanged.

### UHDR data management

- [x] ISC-56: `UHDRKeyManager` derives the AES-256-XTS key pair (K1+K2) using Argon2id (m=65536 KiB, t=4, p=1, RFC 9106) from a biometric/PIN credential after `LAContext.evaluatePolicy(.deviceOwnerAuthentication)` (biometric preferred, passcode fallback — includes Parkinson's/post-stroke users). PHC reference library vendored; key is memory-only, never persisted. VERIFIED: `testSuccessfulAuthDerivesKey` + `testArgon2idDeterministic`.
- [x] ISC-57: `UHDRKeyManager` holds the derived key in memory only (`UHDRKey` class, `bzero` in deinit). The Keychain stores only the 32-byte biometric credential seed (WKMD) with `kSecAttrAccessibleWhenUnlockedThisDeviceOnly` + `.userPresence` access control + `kSecAttrSynchronizable: false` — derived key material never written to Keychain, UserDefaults, or any persistent store. VERIFIED: `testKeyStoredInKeychain` (confirms absence of persistent key).
- [x] ISC-58: `UHDRKeyManager` never transmits the derived AES-256 key to any external endpoint — verified by `testKeyNeverTransmitted` (static source scan for URLSession, URLRequest, NSURLConnection, dataTask, uploadTask, downloadTask, kSecClassKey, UserDefaults, identifierForVendor, UIDevice).
- [x] ISC-59: `UHDRBackupScheduler` registers the `com.neurone.uhdr-backup` BGTaskScheduler task and schedules the next run on each successful backup completion.
- [x] ISC-60: `UHDRBackupScheduler.performBackupIfNeeded()` runs only when the device is on USB-C power — checked via `UIDevice.current.batteryState == .charging` or `.full` and USB accessory detection.
- [x] ISC-61: `UHDRBackupScheduler` writes incremental backup metadata (last backup date, session count since last backup) to a local file — not to any NeurOne server endpoint.
- [x] ISC-62: `Anti:` `UHDRKeyManager` does not fall back to a hardcoded or device-ID-derived key if biometric/PIN authentication fails — it surfaces `UHDRKeyError.biometricFailed` or `UHDRKeyError.userCancelled`. VERIFIED: `testNoFallbackOnBioFailure` + `testCredentialStoreFailurePropagates`.
- [x] ISC-63: `Anti:` `SHDRUploader` does not include any UHDR element (EEG waveforms, HRV time series, session timestamps, outcome logs, PPG signal) in the data sent to NeurOne fleet endpoints — only SHDR fields per NP-FW-EMMC-001 Rev A §7.

### SHDR upload

- [x] ISC-64: `SHDRUploader` triggers an upload when `gatt.shdrUploadPending == true` (hub GATT notification).
- [x] ISC-65: `SHDRUploader` sends only the SHDR fields declared in NP-FW-EMMC-001 Rev A §7 — LED output ratio, NTC profiles, EMF attenuation ratio, device session count (unsigned integer, no timestamps), consumable session counts, USB-C insertion counter, PD negotiation log, firmware version history, and calibration coefficient history.
- [x] ISC-66: `SHDRUploader` does not send session timestamps, EEG data, HRV data, or any field whose boundary resolution in CLAUDE.md §5.1 assigns it to UHDR.
- [ ] ISC-67: `SHDRUploader` shows a non-blocking notification ("Syncing device health data") during upload and a silent completion — not a modal.

### Clinical consent engine

- [x] ISC-68: `ConsentOnboardingView` presents all four layers (L1 Contact, L2 Category, L3 Blanket + irreversibility notice, L4 Results + community) in sequence at first launch.
- [x] ISC-69: L1 contact consent screen offers three contact frequency options and a `contactMethod` picker; the "Skip" affordance is available without blocking future feature access.
- [x] ISC-70: L2 category screen shows all 9 research categories from `ResearchCategory` enum with individual toggles; deselecting all is valid (no forced consent).
- [x] ISC-71: L3 blanket consent screen displays `ConsentEngine.irreversibilityNotice` verbatim and untruncated before the toggle control is rendered.
- [x] ISC-72: L3 blanket consent toggle is off by default and is not pre-ticked — confirmed by `ResearchConsentState()` default initialiser having `blanketConsentGranted = false`.
- [x] ISC-73: L4 results screen offers "Receive study results" and "Join suggestion portal" as independent toggles.
- [x] ISC-74: `ConsentStore.updateResearchConsent(_:)` persists the final `ResearchConsentState` to `UserDefaults` on completion of the onboarding flow.
- [x] ISC-75: `ConsentDashboardView` lists all active clinician grants with clinician name, organisation, tier, granted elements, and a "Revoke" button.
- [x] ISC-76: Tapping "Revoke" on a clinician grant calls `ConsentStore.revokeClinicianAccess(grantID:)` after a confirmation alert.
- [x] ISC-77: `ConsentDashboardView` lists all active study participations with study ID, participation date, and a "Withdraw" button.
- [ ] ISC-78: `ConsentDashboardView` lists all pending study invitations with study name, brief description, what elements will be shared, and Accept / Decline buttons.
- [x] ISC-79: Accepting a study invitation calls `ConsentStore.acceptInvitation(studyID:)` and surfaces a confirmation that includes the irreversibility notice for that specific study.
- [x] ISC-80: `ConsentStore.withdrawBlanketResearchConsent()` sets `blanketConsentGranted = false` and persists — verified by checking the `researchKey` UserDefaults value after calling it.
- [x] ISC-81: `Anti:` The consent tab badge count (`consent.pendingInvitations.filter { $0.hasNoDecision }.count`) never goes negative.
- [x] ISC-82: `Anti:` `ConsentEngine.minimumNecessaryElements(for:)` never returns elements from a use case that was not in `selectedUseCaseIDs`.

### Privacy compliance — launch-blocking screens

- [x] ISC-83: A minimum age gate screen exists as the **first** screen in the onboarding flow, before any personal data is collected or any consent layer is presented.
- [x] ISC-84: The minimum age gate shows: "I confirm I am 16 years of age or older." with an unchecked checkbox and a Continue button that is disabled until the checkbox is checked.
- [x] ISC-85: The minimum age gate checkbox is rendered via `Toggle` with `isOn` bound to a local `@State var ageConfirmed = false` — the binding is not pre-set to `true` in any code path.
- [x] ISC-86: Tapping Continue on the age gate with the checkbox unchecked does nothing — the Continue button `disabled(!ageConfirmed)` modifier is confirmed present.
- [x] ISC-87: The age gate completion status is persisted to `UserDefaults` key `np.onboarding.age-confirmed: Bool` so it is not re-shown on subsequent launches for the same user.
- [x] ISC-88: A BIPA written release screen exists, containing the full disclosure text from NP-APP-ROADMAP-001 Rev B §9.3 (purpose, retention, destruction method, no-sale clause, no-third-party-share clause).
- [x] ISC-89: The BIPA screen is shown before the consent onboarding flow for any user regardless of location.
- [x] ISC-90: If the user declines the BIPA screen, EEG neurofeedback-dependent features (`SessionView` closed-loop metrics, `ProtocolMenuView` EEG-adaptive protocols) are visually disabled with the message: "EEG neurofeedback is unavailable — brainwave data consent was not granted."
- [x] ISC-91: The BIPA declination state is re-presentable from Settings — a "Manage EEG data consent" toggle in a Settings screen re-opens the BIPA screen.
- [x] ISC-92: No analytics or crash reporting SDK (`init()`, `configure()`, or equivalent) is called before `UserDefaults.bool(forKey: "np.onboarding.consent-shown") == true`.
- [ ] ISC-93: The analytics event sent on session start uses `engagement_tier` enum (`new`/`active`/`established`), not a raw session count integer.
- [x] ISC-94: HealthKit data (`HKQuantityTypeIdentifierHeartRateVariabilitySDNN`, `HKQuantityTypeIdentifierHeartRate`) is requested only if the user has the HRV biofeedback protocol active — not at app launch or consent time.
- [x] ISC-95: HealthKit samples read during a session are used only to populate `SessionView` live metrics — they are not written to any local persistence layer, UserDefaults, or app database.
- [x] ISC-96: `Anti:` No `HKHealthStore.save(_:with:)` or `HKHealthStore.add(_:to:completion:)` call exists anywhere in the iOS app source tree.
- [x] ISC-97: `Anti:` No analytics event property contains the string `"eeg"`, `"hrv"`, `"rmssd"`, `"coherence"`, `"session_id"`, or `"protocol_id"` — confirmed by grepping the analytics event property dictionary definitions.

### Consumable tracker

- [x] ISC-98: `ConsumableTracker` correctly maps `gatt.session.consumableSessionCounts` (4 × UInt16 from GATT) to the four consumable types: intranasal sleeves, electrode hydrogel tips, VNS clip pads, audio cup foam.
- [x] ISC-99: `ConsumableTracker.blockingReminders` returns non-empty when any consumable is at or past its session limit — these block `sessionIsBlocked`.
- [x] ISC-100: `ConsumableView` shows each consumable with a remaining-sessions progress bar, a "Reorder" button that opens the NeurOne store URL, and the triggered reminder text.
- [ ] ISC-101: Blocking reminders cannot be dismissed from `ConsumableView` — only a GATT-confirmed consumable replacement (session count reset) clears them.
- [x] ISC-102: Performance-critical reminders (electrode hydrogel tips degrading) can be snoozed at most 3 times — tracked in `UserDefaults` per consumable type.
- [x] ISC-103: Comfort/longevity reminders can be snoozed at most 5 times.
- [x] ISC-104: Each reminder includes the GATT-measured session count that triggered it (e.g. "Your intranasal sleeves have been used 30 times").
- [ ] ISC-105: `ConsumableView` includes a one-tap reorder link per consumable — the URL opens `SFSafariViewController`, not an external browser.
- [x] ISC-106: `Anti:` `ConsumableTracker` does not use calendar-based reminders — all reminders are triggered exclusively by GATT session count values.

### OTA firmware update

- [x] ISC-107: `OTAManager` fetches the latest firmware manifest from a NeurOne distribution endpoint when the hub connects and `OTAView` is opened.
- [x] ISC-108: `OTAView` shows current firmware version (from hub GATT), latest available version, and a "Update" button — disabled if already at latest.
- [x] ISC-109: `OTAManager.beginUpdate(image:)` sends the OTA `INITIATE` opcode, streams firmware chunks (≤512 bytes) via `sendOTACommand(.chunk, payload:)`, and sends `VERIFY` and `COMMIT` opcodes per the 9-step OTA sequence.
- [x] ISC-110: `OTAView` shows a chunked progress bar with transferred bytes / total bytes during the update.
- [x] ISC-111: If a hub disconnect occurs mid-OTA, `OTAManager` does not corrupt the active firmware bank — the bootloader's inactive-bank write architecture ensures the running bank is untouched.
- [x] ISC-112: After a successful OTA commit, `OTAView` shows "Update complete — hub will restart" and waits for reconnect before resuming normal operation.
- [x] ISC-113: `Anti:` `OTAManager` does not send unsigned firmware chunks — `verifySignature(image:)` blocks transfer for unrecognised Ed25519 fingerprints before any GATT write.

### Hardware setup wizard

- [x] ISC-114: `SetupView` presents a first-run wizard covering: (1) BLE pairing confirmation, (2) EEG impedance check (8 contacts), (3) first protocol selection, (4) safety information acknowledgement.
- [x] ISC-115: The EEG impedance step shows each of the 8 electrode positions (Fp1/Fp2, F3/F4, C3/C4, P3/P4) colour-coded from `gatt.session.impedancePassFlags` — green when the bit is set, amber/red otherwise.
- [x] ISC-116: The wizard blocks progression to "protocol selection" until at least 6 of 8 EEG contacts pass impedance (hard-coded safety threshold — UserDefaults configurability removed; threshold is safety-critical).
- [x] ISC-117: `HardwareSetupManager.isFirstSetupComplete` is set to `true` (persisted) only after the wizard's final step is completed — `SessionView` is inaccessible until then.
- [x] ISC-118: The safety acknowledgement step includes a plain-language summary of all T1 contraindications (photosensitive epilepsy, pacemaker, pregnancy, recent head trauma) with a "I have read and understood the above" checkbox.
- [x] ISC-119: The contraindications checkbox is not pre-ticked and cannot be bypassed.
- [x] ISC-120: `SetupView` is accessible after first-run setup for re-calibration — the "Setup" tab badge disappears once `isFirstSetupComplete == true`.
- [x] ISC-121: `Anti:` `SessionView` does not become accessible (tab switch succeeds, protocol picker can be opened) when `setup.isFirstSetupComplete == false` — `guardedTabSelection: Binding<Int>` in `MainTabView` intercepts selection before render (replaces `onChange` flicker approach); `onReceive(UIApplication.didBecomeActiveNotification)` redirect also enforces on app-foregrounding.

### Apple Watch bridge

- [x] ISC-122: `PhoneSessionManager` establishes a `WCSession` and activates it if `WCSession.isSupported()` returns true.
- [x] ISC-123: `PhoneSessionManager` sends session epoch, protocol ID, HRV coherence, pacer phase, and RMSSD to the Watch via `WCSession.sendMessage(_:replyHandler:errorHandler:)` at the 100ms GATT notification rate.
- [x] ISC-124: `PhoneSessionManager` handles Watch connectivity not available (Watch not paired, WatchOS app not installed) gracefully — `SessionView` has no visible dependency on Watch availability.
- [x] ISC-125: `Anti:` `PhoneSessionManager` does not transmit EEG waveform data or raw session timestamps to the Watch — only display-safe metrics (coherence score 0–10, RMSSD integer, pacer phase, session duration).

### Onboarding and account

- [x] ISC-126: `ConsentOnboardingView` is presented exactly once on first launch and is re-accessible from the Privacy tab at any time.
- [x] ISC-127: The age gate screen, biometric data consent screen, and research consent onboarding appear in the correct sequence: age gate → biometric consent → L1–L4 research consent → main app.
- [x] ISC-128: Tapping "Skip" on the research consent onboarding (L1–L4) commits the default `ResearchConsentState()` (all fields false/nil) without blocking app access.
- [x] ISC-129: The Privacy tab is always accessible regardless of consent state — users must be able to reach their consent settings at any time.
- [x] ISC-130: `Anti:` There is no screen in the onboarding flow that is impossible to exit — every screen has a Skip or Cancel affordance except the age gate (which has no skip but offers a graceful "I am under 16" path that explains what features will be unavailable).

### App Store readiness

- [ ] ISC-131: The app passes App Store Connect automated binary analysis (no private API usage) — confirmed by `xcrun altool --validate-app` with zero errors.
- [ ] ISC-132: The App Store privacy nutrition label declares: Name (optional — not collected unless user enters it), Email Address (used for research contact — optional), Health & Fitness (HRV, used on device only — not linked to identity, not used to track), Device ID (linked to identity for warranty — used for app functionality), Crash Data (linked to identity — only after consent).
- [x] ISC-133: The app ships with a human-readable `PrivacyInfo.xcprivacy` file listing all accessed privacy-sensitive APIs (`NSPrivacyAccessedAPICategoryUserDefaults`, `NSPrivacyAccessedAPICategoryFileTimestamp`, `CBCentralManager`).
- [ ] ISC-134: App Store screenshots (6.7" and 5.5" iPhone) exist for all five primary screens: Session, Setup, Supplies, Privacy, Firmware.
- [ ] ISC-135: The App Store description does not contain any medical claim, FDA-regulated claim, or claim that the device diagnoses, treats, cures, or prevents any disease.
- [ ] ISC-136: `Anti:` The App Store description does not use the terms "medical device", "FDA-cleared", "FDA-approved", "510(k)", "clinical", or "therapeutic" — general wellness framing only.

### Regression anti-criteria

- [x] ISC-137: `Anti:` `ConsentStore` never exposes UHDR elements to a clinician grant at a tier lower than that element's minimum tier (`UHDRElement.minimumTier`).
- [x] ISC-138: `Anti:` The session stop button is never enabled when `gatt.connectionState != .connected` — a disconnected app cannot command a hub stop.
- [ ] ISC-139: `Anti:` `NPProtocolValidator` accepts no protocol with a `pbm_transcranial` peak irradiance claim above 400 mW/cm² — OI-PBM-05 is pending regulatory opinion and this limit must be software-enforced until resolved.
- [ ] ISC-140: `Anti:` No raw EEG waveform data is stored in `UserDefaults` — session recording is a hub UHDR function only; the app stores only display-aggregated metrics (coherence score, RMSSD integer, impedance pass count).
- [ ] ISC-141: `Anti:` `SessionProtocolUploader` does not upload a protocol while `gatt.session.status == .running` — mid-session protocol re-upload is blocked until the current session ends.
- [x] ISC-142: `Anti:` `UHDRBackupScheduler` does not run while `UIDevice.current.batteryState == .unplugged` — backup is wired power only.
- [x] ISC-143: `Anti:` The analytics crash reporter is not called with any HealthKit-derived data in the event payload.
- [x] ISC-144: `Anti:` No `@AppStorage` key name contains `"session_count"` or `"session_sequence"` — the deprecated raw count field from pre-Rev B telemetry spec must not exist in the app source.

### Performance and accessibility

- [ ] ISC-145: `SessionView` maintains 60 fps during a running session with all live metrics updating at 100ms intervals — measured by Instruments Time Profiler with zero dropped frames over a 30-second window.
- [x] ISC-146: All interactive controls in `SessionView`, `ConsentOnboardingView`, `ConsumableView`, and `SetupView` have `accessibilityLabel` set to a meaningful string (not the default system-generated one for icon-only buttons).
- [ ] ISC-147: The minimum contrast ratio for all body text against its background meets WCAG AA (4.5:1) — verified with Xcode Accessibility Inspector.
- [x] ISC-148: The app functions correctly with Dynamic Type set to "Accessibility Extra Large" — no text is clipped or overflows its container in `SessionView` at this size.
- [x] ISC-149: VoiceOver announces the coherence score change on each GATT update (0.1 Hz) in `SessionView` without producing an audio flood — change announcements are debounced at ≥2 seconds.
- [ ] ISC-150: `Anti:` No `UIView` or `SwiftUI.View` directly observes a `@Published` property that updates at >10 Hz by triggering a full body re-render — any >10 Hz property (e.g. pacer phase at 100ms) uses `withAnimation` or a dedicated `@State` throttle.

### Testing

- [x] ISC-151: A `NPProtocolValidatorTests` XCTest target exists with tests covering: valid protocol accepts, current-over-limit rejects, dose-over-limit rejects, charge-density-over-limit rejects, and zero-duration rejects.
- [x] ISC-152: A `GATTParserTests` XCTest target exists with tests for all 8 `GATTParser` parse functions using canonical byte sequences.
- [x] ISC-153: A `ConsentEngineTests` XCTest target exists with tests covering: minimum necessary elements for each use case tier, consent document generation, irreversibility notice presence.
- [x] ISC-154: A `ConsumableTrackerTests` XCTest target exists covering: blocking/non-blocking threshold detection, snooze limit enforcement.
- [x] ISC-155: `UHDRKeyManagerTests` XCTest target exists with 7 passing tests: `testArgon2idDeterministic` (KAT — same inputs produce same 64-byte output; different passwords produce different keys), `testSuccessfulAuthDerivesKey` (mock biometric → 32+32 byte K1/K2 derived), `testKeyStoredInKeychain` (key absent from Keychain + UserDefaults), `testLockClearsKey` (lock clears activeKey and isAuthenticated), `testNoFallbackOnBioFailure` (canEvaluate=false and evaluateThrows both leave key nil), `testCredentialStoreFailurePropagates` (store throws → key nil), `testKeyNeverTransmitted` (static source scan confirms no networking symbols, kSecClassKey, UserDefaults, or device identifiers in UHDRKeyManager.swift). 7/7 pass. VERIFIED 2026-06-14.
- [x] ISC-156: All test targets pass on `xcodebuild test -scheme NeurOne -destination "platform=iOS Simulator,name=iPhone 17 Pro"` with zero failures.
- [x] ISC-157: `Anti:` No test file imports a production analytics or crash reporting module directly — tests use mock implementations conforming to protocol abstractions.

### Localisation

- [x] ISC-158: All user-visible strings in the consent flow (irreversibility notice, BIPA disclosure, regulatory footer, age gate text) are defined in `Localizable.strings` — not hardcoded in Swift source as string literals.
- [x] ISC-159: The app ships with at least an `en.lproj/Localizable.strings` file with all strings populated.
- [x] ISC-160: `Anti:` No user-visible string in the consent flow is truncated by `lineLimit(1)` — all consent text uses unlimited line limit or explicit `.fixedSize(horizontal: false, vertical: true)`.

### Session mode indicators

- [x] ISC-161: When the hub is connected via USB-C (detected by `UIDevice.current.batteryState == .charging` and device accessory detection), `SessionView` shows "Mode 1 — Live (<1ms)" in the connection banner.
- [x] ISC-162: When the hub is connected via BLE only, `SessionView` shows "Mode 1 — Wireless" in the connection banner.
- [x] ISC-163: `SetupView` contains a "Program Session" flow that serialises a protocol and uploads it to the hub for autonomous Mode 3 operation — with a confirmation "Protocol stored. Hub will run session without phone."
- [x] ISC-164: `Anti:` The app does not claim Mode 1 latency (<1ms) when connected over BLE — the BLE path is correctly labeled "Mode 1 — Wireless" without a latency claim.

## Test Strategy

| ISC | type | check | threshold | tool |
|-----|------|-------|-----------|------|
| ISC-1 | build | `xcodebuild build -scheme NeurOne -configuration Release` | 0 errors, 0 warnings | Xcode CLI |
| ISC-2 | build | `xcodebuild build -scheme NeurOne -destination 'platform=iOS Simulator'` | 0 errors | Xcode CLI |
| ISC-3 | lint | `swiftlint --strict` | 0 violations | SwiftLint |
| ISC-4 | file | `grep NSBluetoothAlwaysUsageDescription app/ios/NeurOne/Info.plist` | match found | grep |
| ISC-8 | grep | `grep -rn 'print(' app/ios/NeurOne --include="*.swift"` | 0 matches | grep |
| ISC-9 | grep | `grep -rEn '([0-9]{1,3}\.){3}[0-9]{1,3}' app/ios/NeurOne --include="*.swift"` | 0 matches | grep |
| ISC-11 | unit | `NeurOneGATTManagerTests.testAutoScanOnPoweredOn`, `testAutoScanRestartsOnSecondPoweredOn`, `testNoScanWhenBluetoothOff` | all pass | XCTest |
| ISC-12 | unit | `NeurOneGATTManagerTests.testReconnectScheduledAfterDisconnect`, `testNoReconnectWhenBluetoothOff` | all pass | XCTest |
| ISC-13 | unit | `NeurOneGATTManagerTests.testAllCharacteristicsResolvedWhenAllPresent`, `testAllCharacteristicsNotResolvedWhenAnyMissing`, `testAllCharacteristicsResolvedClearedOnDisconnect` | all pass | XCTest |
| ISC-15 | unit | `GATTParserTests.testAllParserFunctions` | all 8 pass | XCTest |
| ISC-16 | unit | `NeurOneGATTManagerTests.testConnectionStateTransitionDisconnectedToScanning`, `testConnectionStateReturnsToDisconnectedWhenBLEGoesOff` | all pass | XCTest |
| ISC-17 | unit | `NeurOneGATTManagerTests.testSuperclassIsNSObject` | pass | XCTest |
| ISC-19 | unit | `NeurOneGATTManagerTests.testBluetoothUnavailableWhenPoweredOff`, `testBluetoothUnavailableWhenUnauthorized`, `testBluetoothUnavailableWhenUnsupported`, `testBluetoothAvailableWhenPoweredOn` | all pass | XCTest |
| ISC-20 | unit | `NeurOneGATTManagerTests.testSessionStatusUUIDInExpectedDiscoverySet`, `testCharacteristicAssignmentCompletesAfterFullDiscovery` | all pass | XCTest |
| ISC-27 | grep | `grep '// TODO' app/ios/NeurOne/Views/SessionView.swift` | 0 matches | grep |
| ISC-30 | unit | `SessionDisplayTests.testDownloadButtonShownWhenCompletedAndEpochNonZero`, `testDownloadButtonHiddenWhenEpochIsZero`, `testEDFSessionIDIsNilForZeroEpoch` | all pass | XCTest |
| ISC-35 | unit | `SessionProtocolUploaderTests.testSignAndUpload` | pass | XCTest |
| ISC-37 | unit | `NPProtocolValidatorTests.testCurrentLimits` | 2mA/1mA limits enforced | XCTest |
| ISC-38 | unit | `NPProtocolValidatorTests.testChargeDensityLimit` | 40µC/cm² enforced | XCTest |
| ISC-56 | unit | `UHDRKeyManagerTests.testSuccessfulAuthDerivesKey` + `testArgon2idDeterministic` | K1/K2 32B each; deterministic for equal inputs | XCTest |
| ISC-57 | unit | `UHDRKeyManagerTests.testKeyStoredInKeychain` | key absent from Keychain + UserDefaults | XCTest |
| ISC-58 | unit | `UHDRKeyManagerTests.testKeyNeverTransmitted` | 0 networking symbols in UHDRKeyManager.swift | XCTest |
| ISC-62 | unit | `UHDRKeyManagerTests.testNoFallbackOnBioFailure` + `testCredentialStoreFailurePropagates` | returns error, not fallback key | XCTest |
| ISC-63 | grep | `grep -n 'eegWaveforms\|hrvTimeSeries\|sessionTimestamps' app/ios/NeurOne/Data/SHDRUploader.swift` | 0 matches | grep |
| ISC-72 | unit | `ResearchConsentStateTests.testDefaultFalse` | `blanketConsentGranted == false` | XCTest |
| ISC-83 | manual | Launch app on clean install, observe first screen | age gate is first | Simulator |
| ISC-85 | grep | `grep -n 'ageConfirmed = true' app/ios/NeurOne` | 0 matches | grep |
| ISC-92 | grep | `grep -rn 'Analytics\|Crashlytics\|configure()' app/ios/NeurOne --include="*.swift"` | all calls are inside `if consentShown` guard | grep |
| ISC-93 | grep | `grep -rn 'session_count\|sessionCount' app/ios/NeurOne --include="*.swift"` | 0 analytics event property matches | grep |
| ISC-96 | grep | `grep -rn 'HKHealthStore.*save\|HKHealthStore.*add' app/ios/NeurOne --include="*.swift"` | 0 matches | grep |
| ISC-97 | grep | `grep -rn '"eeg"\|"hrv"\|"rmssd"\|"coherence"\|"session_id"\|"protocol_id"' app/ios/NeurOne --include="*.swift"` | 0 analytics event dict matches | grep |
| ISC-131 | build | `xcrun altool --validate-app -f NeurOne.ipa -t ios` | 0 errors | altool |
| ISC-144 | grep | `grep -rn 'session_count\|session_sequence' app/ios/NeurOne --include="*.swift"` | 0 matches | grep |
| ISC-156 | test | `xcodebuild test -scheme NeurOne -destination "platform=iOS Simulator,name=iPhone 17 Pro"` | 0 failures | XCTest |

## Features

| name | description | satisfies | depends_on | parallelizable |
|------|-------------|-----------|------------|----------------|
| session-stop-command | Replace TODO stub at SessionView:262 with real `gatt.sendSessionStop(completion:)` call; add stop characteristic to GATT manager; add confirmation alert | ISC-27, ISC-28, ISC-29, ISC-34 | BLE-gatt-layer | false |
| session-history-view | Create SessionHistoryView + SessionHistoryStore; session detail with coherence sparkline; EDF download trigger; file sharing | ISC-48, ISC-49, ISC-50, ISC-53, ISC-54, ISC-55 | EDF-download | false |
| adaptive-adjustments-card | Add Adaptive Adjustments card to session detail view; plain-language trigger enum; 5-event limit + View all | ISC-51, ISC-52 | session-history-view | false |
| age-gate-screen | Create AgeGateView as first onboarding screen; unchecked checkbox; UserDefaults persistence; under-16 path | ISC-83, ISC-84, ISC-85, ISC-86, ISC-87, ISC-130 | — | true |
| bipa-release-screen | Create BIPADisclosureView with full NP-APP-ROADMAP-001 §9.3 text; shown to all users; EEG feature gate on decline; re-presentable from Settings | ISC-88, ISC-89, ISC-90, ISC-91 | age-gate-screen | false |
| onboarding-sequence | Wire age-gate → biometric consent → L1-L4 consent in correct order; single-entry point | ISC-127, ISC-128, ISC-129 | age-gate-screen, bipa-release-screen | false |
| analytics-sdk-gate | Guard all Analytics/Crashlytics init calls behind `consentShown` flag; add ConsentSupervisor wrapper | ISC-92 | onboarding-sequence | true |
| analytics-engagement-tier | Ensure all analytics events use `engagement_tier` enum; remove any `session_count` property; add SwiftLint rule | ISC-93, ISC-144 | analytics-sdk-gate | true |
| healthkit-session-read | Add HealthKit HRV/HR read for active sessions; session-scoped only; no persistence; no transmission | ISC-94, ISC-95, ISC-96 | — | true |
| protocol-chunk-transfer | Implement ≤512 byte BLE chunking in SessionProtocolUploader for large protocol blobs | ISC-14, ISC-35 | — | true |
| gatt-stop-characteristic | Add stop-command GATT write characteristic to GATTCharacteristics.swift and NeurOneGATTManager | ISC-27 | — | true |
| session-mode-labels | Update connection banner to show Mode 1 — Live / Mode 1 — Wireless per USB-C vs BLE detection; Mode 3 program flow in SetupView | ISC-161, ISC-162, ISC-163, ISC-164 | — | true |
| unit-test-targets | Create NPProtocolValidatorTests, GATTParserTests, ConsentEngineTests, ConsumableTrackerTests, UHDRKeyManagerTests XCTest targets | ISC-151–ISC-157 | — | true |
| swiftlint-config | Add .swiftlint.yml with print-statement rule, no hardcoded API key rule, no session_count analytics property rule | ISC-3, ISC-8, ISC-9 | — | true |
| privacy-info-plist | Create PrivacyInfo.xcprivacy; complete App Store privacy nutrition label in App Store Connect | ISC-133, ISC-132, ISC-106 | — | true |
| localizable-strings | Extract all consent flow strings to Localizable.strings; en.lproj only for T1 | ISC-158, ISC-159, ISC-160 | — | true |
| accessibility-pass | Add accessibilityLabel to all icon-only buttons; verify Dynamic Type AXL; debounce VoiceOver coherence announcements | ISC-146, ISC-148, ISC-149 | session-history-view | false |
| appstore-submission | Generate screenshots for all 5 tabs; write App Store description; pass altool validation; submit for review | ISC-131, ISC-134, ISC-135, ISC-136 | all other features | false |

## Decisions

- **2026-06-04**: E4 effort tier chosen. This ISA covers a complete iOS app with regulatory compliance requirements, privacy-blocking items, hardware integration, and App Store submission. E3 would leave Test Strategy and Changelog absent; E5 would require Interview workflow which is appropriate to invoke before BUILD phase begins.
- **2026-06-04**: `app/ios/ISA.md` chosen as canonical path (project sub-ISA, not MEMORY/WORK) — the iOS app has persistent project identity and its own source tree. Project ISA override applies: minimum E3 structure required regardless of active task tier.
- **2026-06-04**: ISC count is 164, above the E4 floor of 128. No intentional under-decomposition.
- **2026-06-04**: Session stop command (ISC-27) is the highest-priority single gap — it is a `// TODO` in shipped code (`SessionView.swift:262`). A deployed app without a stop command is a safety communication failure even though the Safety MCU owns actual stimulation control.
- **2026-06-04**: Biometric data consent (BIPA screen) is shown to all users regardless of location (OI-PA-03 resolved). Locale-based detection was removed — the consent covers BIPA, GDPR Art. 9, and WA MHMD simultaneously.
- **2026-06-04**: Analytics SDK gate (ISC-92) guards `init()` / `configure()` calls. If the analytics vendor's SDK auto-initialises on import (some Crashlytics versions do), the vendor must be replaced or the import must be conditional. This is a code architecture constraint, not just a runtime guard.
- **2026-06-04**: HealthKit features (ISC-94–96) are gated on the HRV biofeedback protocol being active — not requested at app launch. This avoids an unnecessary `NSHealthShareUsageDescription` prompt for users who never run HRV sessions.
- **2026-06-04**: ISC-133 — `PrivacyInfo.xcprivacy` was present on disk from PR #106 but had zero references in `project.pbxproj`. Added to Copy Bundle Resources via xcodeproj gem (PR #107, commit fef956f). Without this, the file does not ship in the app bundle and App Store review would reject the submission.
- **2026-06-04**: ISC-38 — tDCS charge density guard implemented in `NPProtocolValidator` (PR #107, commit bdc9236). Formula: `I(mA) × t(s) / (tdcsDefaultElectrodeAreaCm2 × electrodeCount)`. Default electrode area 35 cm² per electrode position added as `NPHardwareLimits.tdcsDefaultElectrodeAreaCm2`. Charge density > 40 µC/cm² emits `.error` with `parameterKey = "chargeDensityUCcm2"`. Both `testChargeDensityOverLimitRejected` and `testChargeDensityBorderlineInvalid` pass without `XCTExpectFailure`.
- **2026-06-04**: ISC-47 (zero-duration) — hard `.error` added for `dur ≤ 0` in `validate(_:)`. Previously only a `.warning` was emitted for `dur < 60`. `testZeroDurationRejected` passes without `XCTExpectFailure`. PBM dose guard also implemented: estimated dose = `peakMWcm2 × intensityFraction × dur(s) / 1000` checked against `NPPBMTranscranialLimits.maxSessionDoseJCm2` when configured. `testDoseOverLimitRejected` passes without `XCTExpectFailure`. `validatePBMTranscranial` signature updated to accept `totalDurationSeconds`.
- **2026-06-08**: BLE GATT layer implemented (ISC-11, 12, 13, 16, 17, 19, 20). `BLECentral.swift` protocol abstraction enables unit testing without hardware. `NeurOneGATTManager` rewritten with `applyStateUpdate`, `applyCharacteristicAssignment`, `applyDisconnection`, `applyWarrantyToken` internal methods shared by delegates and tests. 25 unit tests added in `NeurOneGATTManagerTests.swift`, all passing. Code review and privacy review completed; five findings applied: (1) `applyStateUpdate` default branch calls `applyDisconnection()` clearing stale UHDR state on BLE reset; (2) `applyCharacteristicAssignment` privacy doc comment; (3) `didUpdateValueFor` SHDR characteristics (`warrantyToken`, `shdrUploadStatus`) given structural early-return guards before `session = pending`, making the UHDR/SHDR boundary a compile-time guarantee; (4) `requestEDFDownload` uses `sessionID.littleEndian` for explicit LE wire serialisation; (5) `SHDRUploader` Keychain read query gains `kSecAttrAccessible: kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly` filter (mirrors LOW-12 fix from SessionProtocolSigner).
- **2026-06-08**: OI-BLE-01 (OPEN) — `warrantyToken` characteristic UUID is defined (`4E455550-0010-…`) and speculated in `didDiscoverCharacteristics`, but hub firmware has not shipped this characteristic yet (OI-WA-03). `SHDRUploader` continues to use its Keychain-generated 32-byte random token. Upgrade path: `SHDRUploader` should subscribe to `gatt.$warrantyToken` via Combine and call an `upgradeDeviceToken(_:)` method when the hub-provisioned value arrives. This is a forward-compatibility stub — no current production device delivers the GATT characteristic. Blocking for full SHDR fleet DB architecture (NP-FW-EMMC-002 Rev A §A — no-join rule). Tracked in NP-SW-001.
- **2026-06-09**: OTA firmware update implemented (ISC-107–113). `firmware/ota/` C module: `np_ota_state_t` struct, CRC32 over first 40 bytes, validate/increment_attempts API; 21 host tests pass. iOS layer: `OTAModels.swift` (`OTAPhase`, `FirmwareVersion`, `FirmwareManifest`, `OTASession`), `FirmwareUpdateService` (SPKI SHA-256 cert pinning, static public manifest URL, no device ID in any request, `FirmwareUpdateProviding` protocol), `OTAManager` (fingerprint-first ordering — `verifySignature` runs before connection check and before any download, making unknown images fail with zero network traffic), `OTAView` (version display, byte-level progress bar, reconnect wait after commit). Opcode wire values frozen: initiate=0x01, chunk=0x02, verify=0x03, commit=0x04, abort=0x05, safetyMCUBegin=0x10, safetyMCUChunk=0x11, safetyMCUCommit=0x12. `firmwareVersion` GATT UUID: `4E455550-0011-…`, READ/NOTIFY, NOT in `NPUUID.all`. `formattedBytes` uses integer round-half-up (avoids banker's rounding "0 KB" for sub-1 KB transfers). `applyConnected()` test helper added to `NeurOneGATTManager` for injection-free BLE testing. 27/27 iOS tests + 21/21 firmware host tests pass.
- **2026-07-05**: ISC-90 second half completed — `ProtocolMenuView` EEG-adaptive protocols now gated on BIPA consent (previously only `SessionView` closed-loop metrics were gated; the menu had zero consent reference, so a declined-consent user could still select/upload an EEG-adaptive protocol). Fixed at the ingestion point: `NPProtocolLibrary.availability(for:)` returns a new `.eegConsentRequired` case (isAvailable == false, `unavailableReason` = localized `SESSION_EEG_UNAVAILABLE_BODY`) for any EEG-dependent entry when `eegConsentGranted == false`, checked *before* the hardware/tier gate. This one change closes every iOS selection path (list row tap, context-menu "Select Protocol" disable, program mode). EEG-dependency is a new computed `NPProtocolDefinition.isEEGDependent` (true for `.eegNeurofeedback`/`.qeeg21ch` modalities, EEG-adaptive audio, or HRV+EEG biofeedback), with `NPProtocolLibrary.isEEGDependent(_:)` resolving composites. Library `eegConsentGranted` seeds from `np.onboarding.bipa-accepted` and is pushed live from `ProtocolMenuView` via `.onAppear`/`.onChange`. Cross-platform: the Watch preset picker routes presets to the phone, so `PhoneSessionManager.acceptedPreset(_:consentGranted:)` drops EEG-adaptive presets (Gamma=0, Alpha=1) when consent is absent and allows Sleep (2) — the Watch cannot bypass the wearer's consent. Web (clinician authoring) and Windows (protocol compiler) are not device-wearer onboarding surfaces. **Android IS a device-wearer onboarding surface (retail users onboard on Android phones/tablets)** — the gate was ported there too (see the Android ISA and `app/android/core/.../ProtocolConsentGate.kt`). 11 new tests in `EEGConsentGateTests.swift` (all pass); BIPAConsentFlowTests (10) / NPProtocolLibraryTests (15) / PhoneSessionManagerTests (11) regression-clean; whole app compiles. No new hardcoded English consent literal (grep confirms the message is only the localization key). See `MEMORY/WORK/isc90-eeg-consent-gate/ISA.md`.
- **2026-06-08**: Consumable tracker testability refactor + privacy analysis closure (ISC-154). `ConsumableTracker` was untestable due to a hard `NeurOneGATTManager` dependency. Resolved by extracting `ConsumableCountsProviding` protocol with `NeurOneGATTManager` extension conformance; test target uses `MockCountsProvider` (`CurrentValueSubject`-backed). Two model bugs fixed: (1) `intranasalSleeves.lowThreshold` was 5 (making single-use sleeve always `isLow` even when fresh; corrected to 0 — alert only when session count ≥ sessionLimit); (2) `activeReminders` had incorrect operator precedence in filter expression (simplified to `states.filter { $0.isLow }`). Critical init-order bug fixed: `loadPersistedSnooze()` must run before `observeCounts()` — the `CurrentValueSubject` / `@Published` publisher delivers its current value synchronously on subscription, which calls `persistSnooze()` with all-zero snooze counts, overwriting persisted state before `loadPersistedSnooze()` had a chance to run. Reordered to `loadPersistedSnooze() → observeCounts()`. Cross-test `UserDefaults.standard` pollution: fixed by injecting UUID-named `UserDefaults` suites per test, cleaned up in `tearDown`. 23 consumable tests (7 model + 16 tracker), all passing. Privacy analysis NP-PRIV-ANALYSIS-003 Rev A → Rev C: 0 Critical/High/Medium, 2 Low all resolved. LOW-1: notification `title` changed to generic `"NeurOne"` + `subtitle`; consumable detail in `body` (after unlock). LOW-2: `requestNotificationPermission()` removed from `init()`, deferred to `ConsumableView.onAppear` via `requestNotificationPermissionIfNeeded()`. Item 3 (GATT routing invariant): 4 tests added to `GATTParserTests` (MARK: CONSUMABLE_STATUS routing isolation) — `testConsumableCountsUnchangedByAllUHDRCharacteristicUpdates`, `testConsumableCountsOnlyUpdatedByParseConsumableStatus`, `testConsumableStatusUUIDDistinctFromAllUHDRCharacteristicUUIDs`, `testWatchBridgeConsumableCountsSourcedFromCorrectKey`. All NP-PRIV-ANALYSIS-003 items closed (Rev C). OI-BLE-01 (hub-token upgrade for SHDRUploader) remains open pending hub firmware. See `docs/np_priv_analysis_003.md`.

## Changelog

*(Empty — no conjectures yet refuted. Populated during EXECUTE and VERIFY phases.)*

## Verification

| ISC | Evidence | Date |
|-----|----------|------|
| ISC-38 | `NPProtocolValidatorTests.testChargeDensityOverLimitRejected` passed (0.001s); `testChargeDensityBorderlineInvalid` passed (0.004s); `testChargeDensityBorderlineValid` passed (0.001s) — all without `XCTExpectFailure`. Full suite: NPProtocolValidatorTests 9/9 passed. PR #107 commit bdc9236. | 2026-06-04 |
| ISC-90 | Second half (ProtocolMenuView EEG-adaptive gate) verified: `EEGConsentGateTests` 11/11 pass — `testEEGProtocol_unavailableWhenConsentDeclined` (`availability(for:) == .eegConsentRequired`), `testEEGProtocol_availableWhenConsentGranted`, `testNonEEGProtocol_unaffectedByConsentState`, `testEEGConsentRequired_carriesLocalizedISC90Message` (message contains "brainwave data consent was not granted"), plus Watch preset gate tests. Regression: BIPAConsentFlowTests 10/10, NPProtocolLibraryTests 15/15, PhoneSessionManagerTests 11/11. First half (SessionView metrics) was already gated. | 2026-07-05 |
| ISC-47 | `NPProtocolValidatorTests.testZeroDurationRejected` passed (0.002s); `testDoseOverLimitRejected` passed (0.001s) — both without `XCTExpectFailure`. PR #107 commit bdc9236. | 2026-06-04 |
| ISC-133 | `grep "PrivacyInfo" app/ios/NeurOne.xcodeproj/project.pbxproj` → 4 refs including `in Resources` build phase entry. PR #107 commit fef956f. | 2026-06-04 |
| ISC-21 | Code review: `sessionScrollView` renders `connectionBanner`, `sessionStatusCard`, `zoneModuleRow`, `sessionControls` as top-level `VStack` children; all four `@EnvironmentObject` properties declared at struct scope. `SessionView.swift` lines 110–133. | 2026-06-07 |
| ISC-22 | Code review: `if gatt.session.status == .running { hrvBreathingRing … }` at `SessionView.swift:118` — ring absent at `.idle`, `.paused`, `.completed`. | 2026-06-07 |
| ISC-23 | Code review: `liveMetricsGrid` inside same `.running` guard block as `hrvBreathingRing` (`SessionView.swift:120–125`). | 2026-06-07 |
| ISC-24 | Code review: `.frame(width: … == .inhale ? 120 : 80, height: … == .inhale ? 120 : 80).animation(.easeInOut(duration: 2.5), value: gatt.session.pacerPhase)` at `SessionView.swift:349–352`. | 2026-06-07 |
| ISC-25 | Code review: `coherenceColor(_:)` → `score >= 7 ? .green : score >= 4 ? .yellow : .orange` at `SessionView.swift:396–398`. | 2026-06-07 |
| ISC-26 | Code review: `impedancePassCount` = `(0..<8).filter { gatt.session.impedancePassFlags & (1 << $0) != 0 }.count` at `SessionView.swift:392–394`; displayed as `"\(impedancePassCount) / 8"`. | 2026-06-07 |
| ISC-27 | Code review: `sendSessionStop()` calls `gatt.sendSessionStop(completion:)` at `SessionView.swift:183–190`. No `// TODO` stub present. | 2026-06-07 |
| ISC-28 | Code review: `if gatt.session.status == .running { Button(role: .destructive) … }` at `SessionView.swift:436`; button absent for all other statuses. | 2026-06-07 |
| ISC-29 | Code review: `showStopConfirmation = true` on button tap → `.confirmationDialog("End this session?", isPresented: $showStopConfirmation)` at `SessionView.swift:79–86`. | 2026-06-07 |
| ISC-30 | Unit tests `SessionDisplayTests` (5 tests): `testDownloadButtonRequiresCompletedStatus`, `testDownloadButtonHiddenWhenEpochIsZero`, `testDownloadButtonShownWhenCompletedAndEpochNonZero`, `testEDFSessionIDIsNilForZeroEpoch`, `testEDFSessionIDPreservesNonZeroEpoch`. Code: `SessionView.shouldShowSessionDownload` + `sessionDownloadControl` + `startCompletedSessionDownload()` at `SessionView.swift:447–517`. Bug fix: `edfSessionID: Self.edfSessionID(from: gatt.session.epoch)` — epoch 0 → nil. | 2026-06-07 |
| ISC-31 | Code review: `regulatoryFooter` text "NeurOne is a general wellness device. It is not a medical device…" at `SessionView.swift:519–524`; rendered in `sessionScrollView` unconditionally. | 2026-06-07 |
| ISC-32 | Code review: `if consumable.sessionIsBlocked { blockingConsumableAlert } else { sessionStatusCard … sessionControls … liveMetricsGrid }` at `SessionView.swift:114–128` — controls and metrics hidden when blocked. | 2026-06-07 |
| ISC-33 | Code review: `let isPresent = slot < gatt.zoneModules.count && gatt.zoneModules[slot] != 0` → `Color.green.opacity(0.2)` / `Color(.systemGray5)` at `SessionView.swift:405–415`. | 2026-06-07 |
| ISC-34 | Code review: `sendSessionStop()` does not set `gatt.session.status`; `handleStatusChange` fires only from `onChange(of: gatt.session.status)` GATT notification — no optimistic local update anywhere in `SessionView.swift`. | 2026-06-07 |
| ISC-161 | Code review: `connectionModeLabel` returns `"Mode 1 — Live (<1ms)"` when `batteryState == .charging \|\| .full` at `SessionView.swift:234–238`. | 2026-06-07 |
| ISC-162 | Code review: `connectionModeLabel` returns `"Mode 1 — Wireless"` for all other battery states at `SessionView.swift:237`. | 2026-06-07 |
| ISC-163 | Code review: `SetupView.swift:115–133` `autonomousModeCard` rendered on `.complete` step with "Choose Protocol" button; taps open `ProtocolMenuView(programMode: true, onProgrammed: { showProgramConfirmation = true })` sheet (`SetupView.swift:36–38`). `ProtocolMenuView.swift:401–404` calls `uploader.programAutonomous(proto)` then `onProgrammed?()`. `SessionProtocolUploader.programAutonomous(_:NPProtocolDefinition)` builds wire protocol with `.mode3Autonomous` and uploads (`SessionProtocolUploader.swift:133–135`). Confirmation alert shows `SETUP_PROTOCOL_STORED_TITLE` / `SETUP_PROTOCOL_STORED_MESSAGE` = "Protocol stored. Hub will run session without phone." (`SetupView.swift:44–47`, `Localizable.strings:513–516`). | 2026-06-15 |
| ISC-15 | `GATTParserTests.testAllParserFunctions` added to `GATTParserTests.swift`. All 8 parsers (`parseSessionState`, `parseSessionStatus`, `parseHRVCoherence`, `parsePacerPhase`, `parseImpedanceResult`, `parseConsumableStatus`, `parseOTAStatus`, `parseZoneModuleStatus`) called with canonical LE byte sequences and asserted non-nil. Pre-existing individual per-parser test functions also satisfy the criterion. | 2026-06-14 |
| ISC-152 | `GATTParserTests.swift` is in the `NeurOneTests` target (fileRef 89CB57AFEAD042EB5E28DA49, build file C2AA81314EC08ACAF876F81A in `project.pbxproj`). File contains individual canonical tests for all 8 parse functions plus truncated-data rejection and consumable-routing isolation tests. `testAllParserFunctions` added 2026-06-14 as the single omnibus entry matching the ISA test strategy. | 2026-06-14 |
| ISC-154 | `ConsumableInventoryTests` (7 tests) + `ConsumableTrackerDirectTests` (16 tests) = 23 tests; 0 failures. Covers: blocking threshold detection (`testSessionBlockedWhenIntranasalExceeded`), non-blocking threshold (`testSessionNotBlockedWhenOnlyHydrogelLow`), snooze cap enforcement (`testSnoozeCapRespectedByTracker_performanceCritical`, `testTrackerSafetyBlockingCannotBeSnooze`), reactive count updates (`testInventoryUpdatesWhenProviderPushesNewCounts`), bounds guards (`testSnoozeOutOfBoundsIndexIsNoOp`, `testMarkReplacedOutOfBoundsIndexIsNoOp`). Privacy analysis NP-PRIV-ANALYSIS-003 Rev C: 0 Critical/High/Medium, 2 Low — all resolved. LOW-1: generic lock-screen notification title (`ConsumableTracker.scheduleLocalNotification`). LOW-2: permission deferred to `ConsumableView.onAppear`. Item 3 (GATT routing invariant): 4 tests in `GATTParserTests` (MARK: CONSUMABLE_STATUS routing isolation) assert that `consumableSessionCounts` is written only from `parseConsumableStatus` / `NPUUID.consumableStatus` — never from any UHDR characteristic. | 2026-06-08 |
| ISC-164 | Code review: BLE path returns `"Mode 1 — Wireless"` — no `<1ms` substring present in that branch. `SessionView.swift:237`. | 2026-06-07 |
| ISC-11 | `NeurOneGATTManagerTests`: `testAutoScanOnPoweredOn` (scan count 1, UUID = NPUUID.service), `testAutoScanRestartsOnSecondPoweredOn` (scan count 2), `testNoScanWhenBluetoothOff` (scan count 0). 25/25 pass. `app/ios/NeurOne/BLE/NeurOneGATTManager.swift` `applyStateUpdate` → `startScan`. | 2026-06-08 |
| ISC-12 | `NeurOneGATTManagerTests`: `testReconnectScheduledAfterDisconnect` (2.1s async wait, scan count ≥ 2), `testNoReconnectWhenBluetoothOff` (2.1s async wait, scan count 1). `applyDisconnection()` schedules `DispatchQueue.main.asyncAfter(+2s)` guarded by `central.state == .poweredOn`. | 2026-06-08 |
| ISC-13 | `NeurOneGATTManagerTests`: `testAllCharacteristicsResolvedWhenAllPresent` (Set(NPUUID.all) → allCharacteristicsResolved true), `testAllCharacteristicsNotResolvedWhenAnyMissing` (drop sessionStop → false), `testAllCharacteristicsResolvedClearedOnDisconnect` (true → disconnect → false). warrantyToken absent does not block resolution. | 2026-06-08 |
| ISC-16 | `NeurOneGATTManagerTests`: `testConnectionStateTransitionDisconnectedToScanning` (.disconnected → poweredOn → .scanning), `testConnectionStateReturnsToDisconnectedWhenBLEGoesOff` (.scanning → poweredOff → .disconnected). All three `applyStateUpdate`/`applyDisconnection` state transitions verified. | 2026-06-08 |
| ISC-17 | `NeurOneGATTManagerTests.testSuperclassIsNSObject`: `NeurOneGATTManager.superclass()` == "NSObject". No UIKit or SwiftUI import in `NeurOneGATTManager.swift` (imports: CoreBluetooth, Combine only). | 2026-06-08 |
| ISC-19 | `NeurOneGATTManagerTests`: `testBluetoothUnavailableWhenPoweredOff`, `testBluetoothUnavailableWhenUnauthorized`, `testBluetoothUnavailableWhenUnsupported` (all true), `testBluetoothAvailableWhenPoweredOn` (false). `@Published var bluetoothUnavailable` drives `SessionView` "Enable Bluetooth" prompt. | 2026-06-08 |
| ISC-20 | `NeurOneGATTManagerTests`: `testSessionStatusUUIDInExpectedDiscoverySet` (NPUUID.sessionStatus in NPUUID.all), `testCharacteristicAssignmentCompletesAfterFullDiscovery`. `didDiscoverCharacteristics` calls `peripheral.readValue(for: char)` immediately on `NPUUID.sessionStatus` discovery (ISC-20 ISR path). | 2026-06-08 |
| ISC-107 | `OTAManagerTests`: `testCheckForUpdatesShowsUpdateWhenNewerVersionAvailable` (manifest 1.1.0 > hub 1.0.0 → `availableUpdate` non-nil), `testCheckForUpdatesHidesUpdateWhenCurrentVersionIsCurrent` (1.0.0 = 1.0.0 → nil), `testCheckForUpdatesShowsUpdateWhenHubVersionUnknown` (nil hub version → non-nil). `observeConnectionForManifestFetch()` auto-triggers `checkForUpdates()` on `.connected`. All 27 OTAManagerTests pass. | 2026-06-09 |
| ISC-108 | `OTAManagerTests`: `testHubFirmwareVersionStored` (GATT 4-byte LE → "1.2.3"), `testHubFirmwareVersionClearedOnDisconnect` (disconnect → nil), `testGATTParseFirmwareVersionRoundTrip` (encode "2.5.0" → 4 bytes → decode → "2.5.0"). `OTAView.versionSection` displays `gatt.hubFirmwareVersion` and `ota.availableUpdate?.version`. | 2026-06-09 |
| ISC-109 | `OTAManagerTests.testOTAOpcodeWireValues`: initiate=0x01, chunk=0x02, verify=0x03, commit=0x04, abort=0x05, safetyMCUBegin=0x10, safetyMCUChunk=0x11, safetyMCUCommit=0x12 (wire values frozen — hub firmware contract). Code review: `beginUpdate(image:)` sends `.initiate` → chunks loop → `.verify` → `waitForPhase(.verified)` → `.commit` → `waitForCompletion()`. 9-step OTA sequence per NP-FW-EMMC-001 Rev A §8. | 2026-06-09 |
| ISC-110 | `OTAManagerTests`: `testOTASessionBytesProgress` (sentBytes=512, totalBytes=1024 → bytesProgress=0.5), `testOTASessionFormattedBytes` (512 → "1 KB", 512000 → "500 KB", 1.5 MiB → contains "MB"), `testOTASessionChunkCount` (1000 bytes / 496 chunkSize = 3 chunks). `OTAView.progressSection` shows `OTASession.formattedBytes(session.sentBytes) / formattedBytes(session.totalBytes)`. Integer round-half-up used to avoid "0 KB" for sub-1 KB transfers. | 2026-06-09 |
| ISC-111 | Code review: `beginUpdate(image:)` sends chunks via `sendOTACommand(.chunk, payload:)`; hub writes only to Scratch partition during transfer. Bank A (running) is not touched until hub receives `.commit` and executes `np_ota_verify_scratch()` + bank swap. Mid-OTA BLE disconnect leaves Scratch in a partial state; `waitForCompletion()` times out and `OTAManager.phase` stays in `.transferring`/`.verifying`; no bank-swap occurs. Running bank unchanged. `firmware/ota/tests/np_ota_tests.c:testBankConstraints` verifies bank field validated as 0 or 1 — arbitrary bank values rejected by `np_ota_state_validate()`. | 2026-06-09 |
| ISC-112 | Code review: `waitForCompletion()` polls `phase != .complete && phase != .failed` with 500ms interval and 120s timeout; `phase` updated by `observeOTAStatus()` from GATT `OTA_STATUS` notifications. `OTAPhase.complete.description` = "Update complete". `OTAView` renders `phase.description` in the progress section; reconnect via `observeConnectionForManifestFetch()` auto-triggers new manifest fetch. | 2026-06-09 |
| ISC-153 | `ConsentEngineTests` (13 tests): tier element coverage (testMonitorTierElements, testAssessTierAddsEEG, testFullClinicalTierAddsHRV, testResearchTierElementsAreEmpty), use-case union (testMinimumNecessaryUnion, testMinimumNecessaryEmptySelection, testMinimumNecessaryHRVOutcomes_containsFullClinicalElements, testMinimumNecessaryAllThreeUseCases_equalUnionOfAll, testAllUseCasesHaveNonEmptyElements, testHRVOutcomesUseCaseIsInLibrary), minimumTier per element (testMinimumTierMonitorElements, testMinimumTierAssessElements, testMinimumTierFullClinicalElements), ISC-137 anti-pattern (testISC137_noTierContainsElementBelowItsMinimumTier), ISC-82 anti-pattern (testISC82_minimumNecessaryNeverOverGrants), consent document generation (testConsentDocumentGeneration), irreversibility notice (testIrreversibilityNoticePresent). | 2026-06-14 |
| ISC-113 | `OTAManagerTests.testSignatureGateBlocksWrongFingerprint`: `beginUpdate(image:)` with wrong fingerprint throws `.signatureInvalid`; `downloadCallCount == 0` — no network call made. `testBeginUpdateThrowsNotConnectedAfterFingerprintPassesWhenHubDisconnected`: correct fingerprint passes gate; `.notConnected` thrown before download; `downloadCallCount == 0`. Fingerprint check is the first statement in `beginUpdate(image:)` — before connection guard, before download, before any GATT write. | 2026-06-09 |
| ISC-51 | Code review: `AdaptiveAdjustmentsCard.swift` exists in `app/ios/NeurOne/Views/` and is embedded in `SessionHistoryView.swift`. Card renders when `completedSession.adaptationEvents` is non-empty; `AdaptTrigger.plainLanguageDescription` produces human-readable strings from the 17-case enum. | 2026-06-14 |
| ISC-52 | Code review: `events.prefix(5)` at `AdaptiveAdjustmentsCard.swift:61`; `Text("and \(remaining) more")` at line 69; `Button("View all")` sheet at line 73. No raw EEG band power values present in any `AdaptationEvent` field exposed to the card. | 2026-06-14 |
| ISC-83 | Code review: `AgeGateView.swift` is the first view presented in the onboarding flow (before `ConsentView` and any personal data collection). `MainTabView` guard confirms `np.onboarding.age-confirmed` before advancing. | 2026-06-14 |
| ISC-84 | Code review: `AgeGateView.swift` renders `Toggle("I confirm I am 16 years of age or older.", isOn: $ageConfirmed)` and `Button("Continue") { … }.disabled(!ageConfirmed)`. | 2026-06-14 |
| ISC-85 | Code review: `@State private var ageConfirmed = false` — initial value is `false`; no code path assigns `ageConfirmed = true` without user toggle interaction. | 2026-06-14 |
| ISC-86 | Code review: `.disabled(!ageConfirmed)` modifier on Continue button confirmed present at `AgeGateView.swift`. Button body is unreachable while `ageConfirmed == false`. | 2026-06-14 |
| ISC-87 | Code review: `UserDefaults.standard.set(true, forKey: "np.onboarding.age-confirmed")` called in `AgeGateView` Continue action. `MainTabView` reads this key to skip the gate on subsequent launches. | 2026-06-14 |
| ISC-35 | `SessionProtocolUploaderTests.testSignAndUpload` passed (0.159s). `upload(_:NPProtocolDefinition)` signed the EEG neurofeedback definition via `SessionProtocolSigner` (Curve25519/Ed25519, Keychain-backed), chunked the wire blob via `ProtocolChunker`, and delivered all chunks to `MockProtocolUploadGateway`. Reassembled payload opened with NPPR magic `[0x4E, 0x50, 0x50, 0x52]`. `isUploading` false after completion. Full suite: `SessionProtocolUploaderTests` 5/5 passed (0 failures). | 2026-06-14 |
| ISC-14 | `SessionProtocolUploaderTests.testUploadMultiChunkProtocolReassemblesWithNPPRMagic` passed (0.003s): 500-char protocol name forces wire blob > 509 bytes → `ProtocolChunker` emits START + END chunks, `gateway.uploadCallCount > 1`. `ProtocolChunkerTests.roundTripReassemblyMatchesOriginal` passed across sizes [0, 1, 508, 509, 510, 511, 1020, 1021, 4096] — chunking is transparent when chunks are reassembled. `SessionProtocolUploader.sendChunksSequentially` delivers each chunk sequentially via checked-continuation bridge, advancing only after each ACK. | 2026-06-14 |
| ISC-146 | Code review + grep: All interactive controls (`Button`, `Link`, tappable `.accessibilityElement`) in `SessionView`, `ConsentOnboardingView`, `ConsumableView`, and `SetupView` use `Label(text, systemImage:)` or string-based button titles — no icon-only buttons with system-generated names. Decorative images hidden from VoiceOver: `connectionBanner` status dot (`SessionView.swift:208`), `statusIndicator` pulse dot (`:336`), `layerProgressDots` HStack (`ConsentOnboardingView.swift:57`), `completeLayer` checkmark seal (`:237`), `ConsumableStatusRow` status dot (`ConsumableView.swift:67`), `stepIcon` (`SetupView.swift:180`), `.complete` step checkmark (`:224`), `errorBanner` warning icon (`:239`), `ZoneModuleStatusGrid` status icons (`:474`), `SafetyAcknowledgementCard.contraindication` xmark icons (`:395`). `ImpedanceStatusGrid` per-channel `VStack` wrapped with `.accessibilityElement(children: .combine)` + descriptive label (`SetupView.swift:511–512`). Build: SUCCEEDED. | 2026-06-15 |
| ISC-148 | Code review + grep: `SessionView` uses `@ScaledMetric(relativeTo: .title3)` properties `breathingRingMax`/`breathingRingMin` (lines 64–65) for all three `hrvBreathingRing` circle dimensions — ring scales with Dynamic Type and text inside cannot overflow the container at AXL. `zoneModuleRow` slot height changed from `.frame(height: 36)` to `.frame(minHeight: 36)` (line 442) — slot expands to contain `.caption2.bold()` text at AXL. All other containers in `SessionView` use `.frame(maxWidth: .infinity)` or padding-only constraints with no fixed heights. Build: SUCCEEDED. | 2026-06-15 |
| ISC-149 | Code review: `SessionView` maintains `@State private var voiceOverCoherence: Float?` and `@State private var coherenceDebounceTask: Task<Void, Never>?`. `SessionObserversModifier.updateHRVSnapshot(hrv:)` cancels any pending task and creates a new `Task` that calls `Task.sleep(nanoseconds: 2_000_000_000)` before posting `UIAccessibility.post(notification: .announcement, argument:)` — announcements are debounced to ≥2s intervals regardless of GATT 100ms update rate. `voiceOverCoherenceAnnouncer` (hidden 0×0 Text view) bridges the update to VoiceOver. `SessionView.swift:59–61`, `260–275`. | 2026-06-15 |
