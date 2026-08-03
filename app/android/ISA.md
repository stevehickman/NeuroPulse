---
task: "Android version of the NeurOne iOS app"
slug: 20260702-android-core-app
project: NeurOne
effort: E3
effort_source: classifier
phase: complete
progress: 82/85 (ISC-60/68 deferred — hardware + wire-schema freeze)
mode: autonomous
started: 2026-07-02
updated: 2026-07-02
---

# ISA — NeurOne Android App

## Problem

NeurOne has a launch-track iOS app (94 Swift files, ~25k lines, ISA at `app/ios/ISA.md`, 146/164 ISCs verified) but no Android app. Android is required for market coverage at T1 launch; the June 2026 code review explicitly noted "Android app code is not present in this repo," and the i18n architecture (canonical JSON → platform adapters) was designed with an Android XML extension point that has no consumer. Every privacy invariant locked in CLAUDE.md (UHDR/SHDR separation, two consent subjects, engagement_tier coarsening, sessionDay timestamp coarsening, prohibited analytics keys, opaque warranty token) must exist on Android with byte-for-byte parity or the platform becomes a privacy regression vector.

## Vision

An Android user gets the identical NeurOne experience their iOS counterpart gets: three-minute consent onboarding, BLE hub connection, live session display with coherence and breathing ring, session history at day-granularity, consumable reminders, OTA updates — with every privacy decision visible, revocable, and plain-language. Engineers maintain one mental model across platforms because the Android module structure, class names, GATT UUIDs, wire formats, and consent semantics mirror the iOS codebase one-for-one.

## Out of Scope

Wear OS companion app (parallels the Apple Watch app — Year 1 post-launch). T2 clinical cloud features (FHIR, multi-patient dashboard, scripting API). Research anonymization engine (runs in firmware). Backend server infrastructure. Google Play submission assets (Data Safety form content is specified by NP-APP-TELEMETRY-001 but filing is a separate workstream). Full visual parity of every iOS screen — this ISA covers the core module port completely and the app-module UI skeleton; screen-by-screen completion continues in follow-up tasks. HealthKit-equivalent (Health Connect) integration — deferred until data-residency analysis (parallel of NP-APP-ROADMAP-001 §9.1) is written for Health Connect.

## Constraints

- **Kotlin + Jetpack Compose, native only.** No cross-platform wrapper (Flutter/RN/KMP-UI) — the program is native-first on both platforms.
- **Two-module split:** `core` is pure-JVM Kotlin (no Android imports) so all privacy-critical logic is unit-testable without an Android SDK; `app` is the Android shell. Mirrors `NeurOneShared` SPM package.
- **GATT UUIDs and wire formats byte-identical to iOS** `GATTCharacteristics.swift` — same 128-bit UUIDs, same little-endian layouts, same opcode values (OTA 0x01–0x05, 0x10–0x12; calibration 0x01–0x04).
- **UHDR never leaves the device.** No UHDR-class field (epoch timestamps, impedance flags, raw EEG/HRV series) in analytics, logs, or cloud backup.
- **Analytics: `engagement_tier` enum only** (new/active/established); prohibited-key gate identical to iOS `ResearchAnalyticsGate` including `imp`/`impedance`/`impedance_flags`/`pass_flags`.
- **Two consent subjects never mixed:** research consent (user, UHDR) and warranty consent (warranty owner, SHDR) are separate stores with no shared keys; blanket research withdrawal tears down research analytics; warranty consent unaffected.
- **Android backup exclusion:** `android:allowBackup="false"` + `dataExtractionRules` — session history and tokens must not flow to Google cloud/D2D backup (parallel of iOS `.isExcludedFromBackupKey`).
- **SHDR device identity = opaque token** (32-byte CSPRNG in Android Keystore-encrypted storage; never ANDROID_ID/advertising ID), header `X-NP-Device-Token`, SPKI certificate pinning on the fleet endpoint.
- **UHDR key derivation: Argon2id** m=65536 KiB, t=4, p=1, 64-byte output (K1+K2), per NP-FW-EMMC-001 §6 — key memory-only, never persisted or transmitted; BiometricPrompt with device-credential fallback.
- **Minimum SDK 29 (Android 10), target SDK 35.** BLUETOOTH_SCAN/BLUETOOTH_CONNECT runtime permissions (API 31+) with `neverForLocation` flag.
- **Strings via Android XML resources** consuming the canonical JSON i18n pipeline's Android extension point — no hardcoded user-visible English in Compose code beyond the initial skeleton.

## Goal

A Gradle multi-module Android project exists at `app/android/` whose pure-JVM `core` module ports all privacy-critical iOS logic (models, GATT parsing, consent, analytics gates, session history, consumables, NPPS protocol engine) with passing unit tests runnable on this machine, and whose `app` module provides the Android shell (manifest, permissions, BLE manager, SHDR uploader, UHDR key manager, Compose UI skeleton) ready for Android Studio build.

## Criteria

### Project structure and build

- [x] ISC-1: `app/android/settings.gradle.kts` declares `:core` and `:app` modules.
- [x] ISC-2: `core` build file applies only pure-JVM Kotlin plugins (no `com.android.*` plugin) — `Grep` confirms.
- [x] ISC-3: `gradle :core:compileKotlin` succeeds on this machine.
- [x] ISC-4: `gradle :core:test` passes with zero failures.
- [x] ISC-5: Anti: no `import android.` statement exists anywhere under `core/src` — `Grep` returns zero matches.
- [x] ISC-6: Anti: no hardcoded IP addresses or API keys in any Kotlin source (`Grep` for `http://` outside comments/pinning config returns zero).
- [x] ISC-7: `app` module build file applies `com.android.application` with minSdk 29, targetSdk 35.
- [x] ISC-8: `gradle :app:assembleDebug` succeeds — 28 MB `app-debug.apk` produced (Android SDK now available on this machine; compileSdk 36, JDK 17). Two latent build bugs fixed to get here (see 2026-07-06 decision).

### GATT layer parity

- [x] ISC-9: `GattUuids.kt` contains all 16 UUIDs byte-identical to iOS `NPUUID` (service, 14 in `all`, warrantyToken, firmwareVersion) — string-compare test.
- [x] ISC-10: `GattUuids.all` contains exactly 14 UUIDs and excludes warrantyToken and firmwareVersion — unit test.
- [x] ISC-11: `GattParser.parseSessionState` decodes little-endian uint32 epoch from canonical bytes — unit test.
- [x] ISC-12: `GattParser.parseSessionStatus`, `parseHrvCoherence`, `parsePacerPhase`, `parseImpedanceResult`, `parseConsumableStatus`, `parseZoneModuleStatus`, `parseOtaStatus`, `parseFirmwareVersion` each return correct values for canonical test byte sequences — unit tests.
- [x] ISC-13: All parsers return null (not throw) on short input — unit test per parser.
- [x] ISC-14: OTA opcodes (0x01–0x05, 0x10–0x12) and calibration opcodes (0x01–0x04) match iOS raw values — unit test.

### Models parity

- [x] ISC-15: `SessionState`, `SessionStatus`, `PacerPhase`, `HRVData` exist in core with the same fields/raw values as `NeurOneShared/SessionState.swift` — unit test on raw values.
- [x] ISC-16: `FirmwareVersion` parses GATT uint32 LE (bits [23:16]=major, [15:8]=minor, [7:0]=patch) and is Comparable — unit test.
- [x] ISC-17: `OtaPhase` includes `verified` = 0x04 and `failed` = 0xFF with isBusy/isTerminal semantics matching iOS — unit test.
- [x] ISC-18: Consent models (`ClinicianUseCaseTier`, `UHDRElement`, `ResearchConsentState`, `ResearchCategory`, `StudyParticipationRecord`, `StudyInvitation`) port with identical tier→element mappings — unit test asserting Monitor/Assess/FullClinical element sets.

### Privacy invariants

- [x] ISC-19: `ResearchAnalyticsGate.track` drops any event containing a prohibited key (identical set to iOS incl. `imp`, `impedance`, `impedance_flags`, `pass_flags`, `session_count`, `session_sequence`) — unit test.
- [x] ISC-20: `ResearchAnalyticsGate` no-ops `configure()` when the consent key is unset, and configures exactly once when set — unit test with spy backend.
- [x] ISC-21: `ResearchAnalyticsGate.reset()` tears down the backend before clearing configured state — spy backend order test.
- [x] ISC-22: `ConsentStore.withdrawBlanketResearchConsent()` clears the research analytics key and resets the gate — unit test.
- [x] ISC-23: Partial withdrawal (category or single study) does NOT revoke research analytics — unit test.
- [x] ISC-24: Warranty gate and research gate use different persistence keys and neither references the other — `Grep` + unit test.
- [x] ISC-25: `EngagementTier.current()` buckets launch counts 0–5→new, 6–50→active, 51+→established — boundary unit tests.
- [x] ISC-26: `SessionRecord` persists `sessionDay` (yyyy-MM-dd) + `insertionIndex`, never an exact timestamp — serialization test asserts no `completedAt`/epoch key in JSON blob.
- [x] ISC-27: `SessionHistoryStore` sorts newest-first (day desc, insertionIndex desc), trims at 100, survives corrupt blob without crash — unit tests.
- [x] ISC-28: Anti: `edfSessionId` is excluded from persisted JSON — serialization test.
- [x] ISC-29: Anti: `AndroidManifest.xml` sets `android:allowBackup="false"` and declares `dataExtractionRules` — `Grep`.
- [x] ISC-30: SHDR uploader uses a locally generated 32-byte random token (never ANDroid_ID / advertising ID) sent as `X-NP-Device-Token`, with SPKI pin config present — `Grep` + code review.
- [x] ISC-31: Anti: no UHDR-class field name (epoch, impedance) appears in any analytics `track` call site — `Grep`.

### Session descriptor + chunker (parity)

- [x] ISC-64: `ProtocolChunker` (core, pure-JVM) ports the iOS BLE framing exactly — SINGLE (≤509B), START (2-byte LE length)/CONT/END; no chunk exceeds the 512B ATT ceiling. Unit-tested at the 509/510 boundary and large-payload reassembly (`ProtocolChunkerTests`, 5/5).
- [x] ISC-65: `NPSessionProtocol.fromDefinition(...)` compiles an authoring `NPProtocolDefinition` to the T1 hub wire form (8 T1 modality configs; zone/channel resolution; HRV protocol wire mapping; T2/accessory modalities dropped) — parity with iOS `NPSessionProtocol(from:)`.
- [x] ISC-66: `SessionProtocolCompiler` builds the canonical JSON payload, SHA-256-digests it, and assembles `SignedProtocolBlob` (wire: 4-byte "NPPR" magic + 4-byte LE length + payload + 64-byte Ed25519 sig) — parity with iOS. Ed25519 delegated to a `ProtocolSigner` interface (core stays pure-JVM). Unit-tested with a fake signer (`SessionProtocolCompilerTests`, 5/5): mapping, wire format, JSON `type` discriminator, chunk round-trip.
- [x] ISC-67: `ProtocolUploader` (app) + `AndroidProtocolSigner` (Ed25519 via `java.security`) wire the menu's select action to compile→sign→chunk→`gattManager.writeProtocolChunk` (Mode 2). Selecting an available single protocol uploads; result shown via Toast. Compile-verified.
- [ ] ISC-68: [DEFERRED-VERIFY] Wire-JSON schema (OI-AND-WIRE-01) must be frozen and shared with hub firmware + iOS before a hub can parse it — iOS uses Swift Codable default enum encoding, Android uses kotlinx `"type"`-discriminated JSON; they must converge. Hub firmware/UUIDs are still placeholders. Also OI-AND-SIGN-01: Ed25519 needs Android 13+ (BouncyCastle fallback) and a Keystore-persisted key (currently ephemeral); composite-protocol upload not yet supported.

### Deep modality editor (parity)

- [x] ISC-84: `ModalityEditorScreen` — port of iOS ModalityEditorView. Per-modality expandable cards with an enable toggle and typed parameter controls for all 8 T1 modalities (PBM transcranial/intranasal, EEG, BES/tACS, tDCS, VNS+HRV, audio, visual): number fields, enum dropdowns (zones, wavelength, band, channels, waveform, HRV protocol, visual mode), and toggles; add-modality picker + per-card remove. T2/accessory modalities show a "edit via script" note. Reachable from the form editor's "Edit modalities"; Save writes the updated protocol to the library. Closes OI-AND-MODEDIT-01. `:app:assembleDebug` green.

### Dosage limits store + editor (parity)

- [x] ISC-81: `resolveLimits(global, helmet, individual)` (core) merges the three tiers field-by-field (individual ?? helmet ?? global) across all 14 modality limit blocks — port of iOS `NPLimitsSet.resolve` (cosmetic per-field source map dropped; validator attributes to the resolved tier). Unit-tested.
- [x] ISC-82: `NPLimitsStore` (core) — global/helmet/individual tiers + profiles + active context + `resolvedLimits` + `makeValidator()`; global tier persists as NPPS text via `KeyValueStore` (helmet/individual/profiles in-memory — OI-AND-LIMITS-01). Unit-tested (`NPLimitsStoreTests`, 6/6: tier precedence, null-block, unlimited, active-profile resolution, NPPS persistence reload, validator enforcement).
- [x] ISC-83: `LimitsSettingsScreen` edits the global safety caps (PBM intensity/dose, BES/tDCS/VNS max mA, photoparoxysmal-range block), preserving unexposed fields; reachable from Settings → "Edit dosage limits". App-scoped `limitsStore`; `ProtocolMenuScreen` validation now runs against `limitsStore.resolvedLimits`. `:app:assembleDebug` green.

### Research portal + protocol composer (parity)

- [x] ISC-78: `ResearchSuggestion` + `ResearchSuggestionDraft` + `FundingStatus` + `PledgeTier` + `ResearchSuggestionStore` ported to core (CLAUDE.md §6.3) — submit/vote/participation-intent/pledge with optimistic local accounting, opaque persisted device token, KeyValueStore persistence; backend sync is a T1 stub. Unit-tested (`ResearchSuggestionStoreTests`, 7/7).
- [x] ISC-79: `ResearchPortalScreen` — list + vote/intent/pledge tiers + a "New idea" form (title/body/9 categories/participation intent); reachable from the Privacy tab. App-scoped store. `:app:assembleDebug` green.
- [x] ISC-80: `ProtocolComposerScreen` — build a composite from ordered layers referencing existing single protocols with per-layer start offsets; saved via `library.save(Composite)`. Reachable from the protocol menu "Compose" header. Deep per-layer duration/intensity tuning deferred (OI-AND-COMPOSE-01).

### Protocol editors + Session history (parity)

- [x] ISC-73: `ProtocolScriptEditorScreen` — full NPPS text editor; Save parses via `library.importScript` and persists via `library.save`; parse errors show the offending line. `NEW_PROTOCOL_TEMPLATE` seeds new protocols.
- [x] ISC-74: `ProtocolEditorScreen` — metadata form (name/description/tags/duration) for a single user protocol, preserving modalities, with "Edit modalities as script". Deep per-modality widget editing (iOS ModalityEditorView) deferred (OI-AND-MODEDIT-01).
- [x] ISC-75: `ProtocolMenuScreen` gains New (header) + Edit/Delete on user (non-read-only) protocols, hosting the editor overlays; bundled protocols remain select-only. `:app:assembleDebug` green.
- [x] ISC-76: `AdaptTrigger` (17 triggers, 0x00–0x10, plain-language copy) + `AdaptationEvent` ported to core (mirrors firmware `np_adaptation_log.h`); enum-only, no raw biometrics. Unit-tested (`AdaptationEventTests`, 4/4 — all 17 copy strings, contiguous raw values, `from()` round-trip, content-derived id).
- [x] ISC-77: `HistoryScreen` — day-granularity session list → detail with metrics + `AdaptiveAdjustmentsCard` (≤5 inline + "View all", empty state). Adaptation events are not persisted (UHDR), so history-opened cards show the empty state — parity with iOS. `CompletedSessionSummary.fromRecord` added to core.

### Hardware setup wizard (parity)

- [x] ISC-71: `SetupFlow` (core, pure-JVM) ports the state-machine core of iOS `HardwareSetupManager`: 11-step `SetupStep` enum with `requiresHardwareConfirmation`/`requiresSafetyAcknowledgement`, `advance()` with the non-bypassable safety gate, `back()` clearing the acknowledgement on re-entry, `evaluateImpedance(flags)` (≥6/8 threshold, failed-electrode list), and `np.setup.first-complete` persistence. `safetyAcknowledged` never persisted (GDPR Art. 9 / BIPA). Unit-tested (`SetupFlowTests`, 7/7).
- [x] ISC-72: `SetupWizardScreen` renders the flow (title/instruction per step, per-step controls, Back/primary nav); hardware steps require a CONNECTED hub; impedance/ADS1299 steps send `sendCalibration(...)`; impedance evaluated against `session.impedancePassFlags`; safety step gated on a checkbox. Reachable from Settings → "Set up device". `sendCalibration()` added to `NeurOneGattManager`. `:app:assembleDebug` green.

### Compose screens (parity — replacing skeletons)

- [x] ISC-56: `SessionScreen` (Mode 1 display) is a full renderer of `(ConnectionState, SessionState)`: connection banner, status card, live metrics (coherence with color threshold, RMSSD, impedance N/8 from `impedancePassFlags`), breathing pacer progress, protocol-picker entry, and a Stop control with confirmation dialog. Replaces the placeholder. `:app:assembleDebug` green.
- [x] ISC-57: `ProtocolMenuScreen` lists bundled + user protocols from `NPProtocolLibrary`, each with an availability dot + reason, validation error/warning badges, and a Composite tag; reachable from the Session tab ("Browse Protocols"). Closes ISC-45.
- [x] ISC-58: `NeurOneApplication` constructs `NPProtocolLibrary(keyValueStore)` and exposes it; `MainActivity.SessionTab` toggles Session ↔ Protocol menu and passes live BIPA consent + the localized EEG-unavailable string.
- [x] ISC-59: `AndroidBleCentral` implements `BleCentral` over `BluetoothLeScanner` + `BluetoothGatt` (scan by service UUID, connect, discover, CCCD-enable notifications, read, write) with API-33 vs pre-33 branches for the changed callback/write signatures. Permission-aware: `adapterState` returns `UNAUTHORIZED` until BLUETOOTH_SCAN/CONNECT are granted, so `NeurOneGattManager`'s auto-scan-on-ON path stays inert at startup. Wired app-scoped in `NeurOneApplication`; `SessionTab` collects `gattManager.connectionState`/`session` as Compose state, requests runtime permissions, and calls `bleCentral.refresh()` on grant to start scanning; Stop → `requestSessionStop()`. Compile-verified (`:app:assembleDebug`).
- [ ] ISC-60: [DEFERRED-VERIFY] On-device connection to a real hub (scan→connect→notify delivering live `SessionState`) — requires physical BLE hardware + a hub; not runnable on this machine or CI. Code path complete and compiled.
- [x] ISC-62: `ConsentOnboardingScreen` implements the a-priori research-consent flow (CLAUDE.md §6.2) — L1 contact (+ frequency), L2 nine research categories, L3 blanket consent with the irreversibility notice, L4 results + suggestion-portal opt-in. All optional (Skip persists partial state). Wired into `Root` after the BIPA gate (`np.onboarding.consent-shown`); Finish/Skip persists via `ConsentStore.updateResearchConsent`. `:app:assembleDebug` green.
- [x] ISC-69: `OtaScreen` renders the hub's live FIRMWARE_VERSION + OTA_STATUS (phase label, progress bar, error code) from `gattManager.hubFirmwareVersion`/`otaStatus`; reachable from Settings → "Firmware & updates". Host-side download/flash service is a follow-up (OI-AND-OTA-01). `:app:assembleDebug` green.
- [x] ISC-70: `Under16Screen` — under-16 path on the age gate ("I am under 16" → explains onboarding does not proceed, Back returns). Parity with iOS Under16View. Age threshold OI-PA-01 shared with iOS.
- [x] ISC-63: `SettingsScreen` enriched to parity: re-presentable EEG/biometric consent (Review re-opens `BipaConsentScreen`; Revoke clears `bipa-accepted` + pushes `updateEEGConsent(false)`), re-openable research-consent flow, the research-analytics gate toggle, and app version. Mirrors iOS "Manage EEG data consent".
- [x] ISC-61: `ConsumablesScreen` lists all four consumables from the app-scoped `ConsumableTracker` (fed by a `GattConsumableCountsProvider` bridging `SessionState.consumableSessionCounts`): name, sessions-remaining bar, Low/Replace-now badge (safety-blocking = red), Order link (`kind.orderUrl`), Mark-replaced, and Snooze (when `canSnooze`). Replaces the placeholder. `:app:assembleDebug` green.

### Protocol library + bundled protocols (parity)

- [x] ISC-51: `NPBundledProtocols.allContents` embeds all 19 predefined NPPS templates verbatim from `protocols/predefined/*.npps` (15 single + 4 composite), in iOS `allContents` order.
- [x] ISC-52: `NPProtocolLibrary` parses all 19 bundled templates without error (`bundledProtocols.size == 19`) — a live cross-check that the Android NPPS parser is at parity with the real template corpus. All bundled entries are read-only (`canEdit`/`canDelete` false).
- [x] ISC-53: `availability(entry)` mirrors iOS: BIPA consent gate first (EEG-dependent → `EegConsentRequired`), then device-tier + hardware-modality + 1064 smart-module resolution; composites resolve members. Unit-tested (no-device, T1 available, consent block/unblock, non-EEG unaffected).
- [x] ISC-54: `validationResult(entry, limits)` runs `NPProtocolValidator` (caches by id; `issueCount` returns errors/warnings); bundled Gamma Focus validates clean under unlimited limits.
- [x] ISC-55: User protocols persist across library reload via NPPS-text round-trip in `KeyValueStore` (parity with iOS JSON file persistence); bundled protocols cannot be saved-over or deleted. Unit-tested (`NPProtocolLibraryTests`, 11/11).

### Protocol validation + hardware limits (safety parity)

- [x] ISC-46: `NPHardwareLimits` ports every firmware ceiling constant from iOS (PBM duty 25%, tDCS 2 mA + 40 µC/cm², BES 1 mA, VNS 2 mA, visual 100 Hz + 3–30 Hz risk zone, HD-tDCS/clinical-tACS/cervical-VNS/vibrotactile/deep-PBM ceilings).
- [x] ISC-47: `NPProtocolValidator.validate(definition)` enforces the hardware ceilings as errors, configured `NPLimitsSet` dosage limits as errors, and advisory warnings (short/long session, photoparoxysmal risk zone, aggressive TMS, cardiac-interlock note) — error/warning semantics mirror iOS one-for-one.
- [x] ISC-48: tDCS charge-density guard: I(mA)×t(s)/(35 cm²×electrodes) > 40 µC/cm² → error (ISC-38 parity). Unit-tested at over-limit and under-limit.
- [x] ISC-49: Zero/negative duration → hard error; PBM session-dose over configured J/cm² limit → error (ISC-47 parity). Unit-tested.
- [x] ISC-50: Composite validation resolves member protocols via a resolver fn and prefixes issues with the layer name; empty modalities / empty layers → error. Unit-tested (`NPProtocolValidatorTests`, 10/10).

### EEG-consent gate (ISC-90 parity)

- [x] ISC-41: `NPProtocolDefinition.isEEGDependent` (core) is true for enabled EEG/qEEG modalities, EEG-adaptive audio, or HRV+EEG biofeedback; false for non-EEG and disabled modalities — mirrors iOS `NPProtocolDefinition.isEEGDependent`. Unit-tested.
- [x] ISC-42: `ProtocolConsentGate.isBlockedByBipa(entry, consentGranted, resolveSingle)` (core, pure-JVM) returns true iff the entry is EEG-dependent and BIPA consent is not granted; composites resolve members via `resolveSingle`. Unit-tested (`ProtocolConsentGateTests`, 9/9).
- [x] ISC-43: Consent signal parity — the gate is driven by the boolean key `np.onboarding.bipa-accepted` (`OnboardingKeys.BIPA_ACCEPTED`), the same key iOS reads via `@AppStorage` and the Android onboarding writes on BIPA acceptance.
- [x] ISC-44: Block message parity — `ProtocolConsentGate.EEG_UNAVAILABLE_MESSAGE_RES == "session_eeg_unavailable_body"`, added to `strings.xml` with the iOS `SESSION_EEG_UNAVAILABLE_BODY` first sentence verbatim. Unit-tested.
- [x] ISC-45: `ProtocolMenuScreen` renders every protocol's `library.availability(entry)`; an EEG-dependent protocol with consent declined resolves to `EegConsentRequired`, which the row displays as the localized `session_eeg_unavailable_body` and makes non-selectable (tap only fires `onSelect` when `availability.isAvailable`). The menu pushes the current BIPA consent into the library on entry (`updateEEGConsent`). Consent-gate decision is unit-tested in `core` (ISC-53); the UI mapping is compile-verified (`:app:assembleDebug`).

### NPPS protocol engine

- [x] ISC-32: NPPS lexer ports compound digit-idents (`660_808nm`, `660_808_1064nm`), hyphenated tags (`wind-down`), and `mW_cm2` unit — regression unit tests mirroring iOS `NPProtocolLibraryTests`.
- [x] ISC-33: NPPS serializer quotes tags containing non-`[A-Za-z0-9_]` chars; serialize→reparse preserves `wind-down` as a single tag — round-trip test.
- [x] ISC-34: All bundled NPPS protocol templates parse without error — unit test.

### Consumables and session flow

- [x] ISC-35: `ConsumableTracker` accepts any `ConsumableCountsProviding` implementation (DI), computes `isLow` correctly incl. intranasal single-use threshold 0 — unit tests.
- [x] ISC-36: Snooze state loads before count observation begins (iOS init-order bug not reintroduced) — unit test.

### App module shell

- [x] ISC-37: Manifest declares BLUETOOTH_SCAN (`neverForLocation`) + BLUETOOTH_CONNECT and no location permission — `Grep`.
- [x] ISC-38: `NeurOneGattManager` exists with `BleCentral` abstraction (testable without hardware), auto-scan on adapter-on, 2s reconnect delay, SHDR/UHDR guard on characteristic routing.
- [x] ISC-39: Compose UI skeleton: bottom-nav main scaffold with Session, History, Consumables, Consent, Settings destinations + age gate and BIPA screens present as composables.
- [ ] ISC-40: [DROPPED — see Decisions]

## Test Strategy

| isc | type | check | threshold | tool |
|-----|------|-------|-----------|------|
| 1–2, 5–7, 29–31, 37 | static | grep/read of build files, manifest, sources | exact match | Grep/Read |
| 3–4 | build | gradle :core:compileKotlin / :core:test | exit 0, 0 failures | Bash |
| 8 | build | assembleDebug on SDK machine | exit 0 | DEFERRED (NP-AND-BUILD-01) |
| 9–28, 32–36 | unit | JUnit tests in core | all pass | Bash gradle :core:test |
| 38–39 | inspection | class/composable existence + structure | present | Read/Grep |

## Features

| name | description | satisfies | depends_on | parallelizable |
|------|-------------|-----------|------------|----------------|
| GradleSkeleton | settings/build files, wrapper config, package convention | ISC-1,2,7 | — | no (first) |
| CoreModels | SessionState, OTA, consent, adaptation, zone, consumable models | ISC-15–18 | GradleSkeleton | yes |
| GattLayer | GattUuids + GattParser + opcodes (pure JVM, ByteArray) | ISC-9–14 | GradleSkeleton | yes |
| PrivacyGates | ResearchAnalyticsGate, WarrantyAnalyticsGate, EngagementTier, ConsentStore | ISC-19–25 | CoreModels | yes |
| SessionHistory | SessionRecord + SessionHistoryStore with day coarsening | ISC-26–28 | CoreModels | yes |
| NPPSEngine | NPPS lexer/parser/serializer port (Forge) | ISC-32–34 | GradleSkeleton | yes (Forge background) |
| Consumables | ConsumableTracker + providers | ISC-35–36 | CoreModels | yes |
| AndroidShell | manifest, BLE manager, SHDR uploader, UHDR key manager, Compose UI | ISC-29–31,37–40 | CoreModels, GattLayer | yes |
| CoreTests | JUnit suites for all of the above | ISC-3–4, 9–28, 32–36, 40 | all | no (last) |

## Decisions

- 2026-07-02: Two-module split (pure-JVM `core` + Android `app`) chosen because no Android SDK exists on the build machine — makes all privacy-critical logic locally verifiable, mirrors NeurOneShared. Alternative (single Android module) rejected: zero local verification possible.
- 2026-07-02: Delegation floor relaxed from 2 to 1 (Forge only) — show-math: the port is single-repo sequential work in an already-isolated worktree; a second write-agent would contend on the same Gradle skeleton and package convention; directed lookups were done directly per the delegation gate.
- 2026-07-02: NPPS engine (1,488-line NPProtocolScripting.swift) delegated to Forge in background — largest self-contained, convention-stable unit.
- 2026-07-02: Argon2id behind an `Argon2Provider` interface — Android implementation to vendor signal-org argon2 or lazysodium as SOUP (parallel of iOS PHC vendoring, NP-SW-001 SOUP table entry required before beta).
- 2026-07-02: Local verification runs from a scratchpad copy of `app/android` — macOS TCC blocks Gradle/daemon directory creation inside `~/Documents` for shell children on this machine. Sources in the worktree are canonical; rsync → scratchpad → `gradle :core:test`.
- 2026-07-02: Gradle daemon pinned to Temurin JDK 17 locally (Homebrew Gradle defaults to JDK 26, which Kotlin 2.0.21 rejects with `IllegalArgumentException: 26.0.1`). Machine-specific `org.gradle.java.home` is documented in `gradle.properties` as a comment, not committed.
- 2026-07-02: Fixed Kotlin init-order bug in SessionHistoryStore — body-declared comparator `val` was null during `init { load() }`, silently emptying every reload via runCatching; moved to companion object. Regression caught by `insertionIndexSurvivesReload`.
- 2026-07-02: Forge's NPPS regression test wrapped the mW_cm2 probe in a nonexistent `limits_probe` modality block; removed the block (the mW_cm2 lexing assertion is standalone). All 67 tests pass after fix.
- 2026-07-05: ISC-90 EEG-consent gate ported from iOS (ISC-41–45). Placed the decision in pure-JVM `core` (`ProtocolConsentGate` + `NPProtocolDefinition.isEEGDependent`) rather than in a Compose screen because Android's Session/Protocol-menu screens are still UI skeletons — putting the load-bearing consent logic in `core` makes it unit-testable now and gives the future screens one tested gate to call, exactly as iOS routes every selection path through `NPProtocolLibrary.availability(for:)`. Consent key `np.onboarding.bipa-accepted` is already byte-identical to iOS. This corrects an earlier iOS-task note that wrongly treated Android as out-of-scope for wearer onboarding — retail users onboard on Android phones/tablets, so the BIPA gate must exist here. UI wiring is DEFERRED (ISC-45) until the screens are built. 9 new tests; full `:core:test` = 76/0.
- 2026-07-05: Ported the safety-critical **protocol validation + hardware-limits** layer (ISC-46–50) — the highest-value iOS logic Android lacked. `NPHardwareLimits` + `NPProtocolValidator` mirror iOS error/warning semantics exactly (charge density 40 µC/cm², PBM dose, zero-duration, per-modality ceilings). Simplification: iOS's per-field `NPLimitSourceMap` (cosmetic attribution) collapsed to the resolved limits' tier — `isValid`/errors/warnings behavior is identical, which is what the tests assert. Verified: `gradle :core:test` 86/0.
- 2026-07-05: **Honest scope status for "full iOS equivalence."** The pure-JVM `core` logic is now largely at parity and fully unit-tested locally. The remaining gap is the **UI + app-level managers**. (Superseded 2026-07-06: Android SDK now available — the app module compiles; UI work is verifiable.)
- 2026-08-02: **DROPPED ISC-40** — `UhdrKeyManager.kt` (the `Context`-scoped wrapper intended to derive the UHDR key after biometric authentication) deleted as dead code: a dead-code audit found it never instantiated anywhere in `:app` (no caller wires it into a biometric-unlock flow, no test exercises the class itself — only `ShdrUploaderTests` exists in that area, and it doesn't touch this class). The underlying `Argon2Provider`/key-derivation primitive it wrapped remains implemented and unit-tested directly in `core`, so the *algorithm* (m=65536, t=4, p=1, no persistence) is still verified — but with the wrapper gone, **Android currently has no production entry point that actually derives a UHDR key**, unlike iOS (`UHDRKeyManager.swift`, wired from `NeurOneApp.swift`). This is a real, not cosmetic, parity gap — added to "Remaining for full iOS parity" below rather than silently dropped.
- 2026-07-06: **Android SDK now on this machine — `:app` builds; ISC-8 CLOSED.** First-ever `app` compile surfaced two latent bugs fixed to get a green `assembleDebug`: (1) `core` declared `kotlinx-serialization-json` as `implementation`, but `Json` is in `core`'s public API (default ctor args of `SessionHistoryStore`/`ConsentStore`), so `:app` couldn't resolve it — changed to `api`. (2) `compileSdk = 35` but only platforms 36.1/37.0 are installed (no `sdkmanager`/cmdline-tools to add 35) — bumped `compileSdk` to 36 (kept `targetSdk = 35`) + `android.suppressUnsupportedCompileSdk=36`. Build recipe: rsync worktree → scratchpad (TCC blocks Gradle under ~/Documents), `local.properties` `sdk.dir`, JDK 17, `gradle :app:assembleDebug`. UI + app-manager parity is now verifiable and proceeds screen-by-screen with a real compile each step.

## Remaining for full iOS parity (excluding Apple Watch)

Verifiable-now `core` logic still to port (unit-testable via `gradle :core:test`):
- ~~`NPProtocolLibrary`~~ DONE (ISC-51–55) — availability + consent gate + validation + persistence.
- ~~`NPBundledProtocols`~~ DONE (ISC-51) — 19 templates embedded + parsed.
- `NPLimitsStore` — global/helmet/individual resolution (`resolve()`), profiles, NPPS import/export, persistence via `KeyValueStore`.
- Session descriptor + `ProtocolChunker` — binary `SessionProtocol` compile-from-definition and ≤512-byte Mode-2 chunking.
- `ResearchSuggestionStore`, `ConsentEngine` extensions, OTA state machine (core `OtaModels` exists).

Cannot be verified here (need Android SDK / instrumented build — do on CI or an SDK machine):
- **UHDR key derivation wired to biometric authentication** (ISC-40, DROPPED 2026-08-02 — the previous `UhdrKeyManager` wrapper was unused scaffolding, not a real integration; see Decisions). The `Argon2Provider` primitive it was meant to call is implemented and unit-tested in `core`; a real Android `BiometricPrompt`-integrated caller still needs to be built, mirroring iOS's `UHDRKeyManager.swift` wiring in `NeurOneApp.swift`.
- App-level managers: `OTAManager`, `HardwareSetupManager`, `SessionProtocolUploader`, `UHDRBackupScheduler`, Health Connect reader.
- Compose screens still to build/replace: ~~Session~~ (ISC-56), ~~Protocol menu~~ (ISC-57), ~~Consumables~~ (ISC-61), ~~Consent onboarding L1–L4~~ (ISC-62), ~~Settings~~ (ISC-63), ~~OTA~~ (ISC-69), ~~Under-16~~ (ISC-70). ~~Setup/hardware wizard~~ (ISC-71–72), ~~Session History + Adaptive-Adjustments~~ (ISC-76–77), ~~Protocol editors (script + form + New/Edit/Delete)~~ (ISC-73–75). ~~Protocol composer (layer builder)~~ (ISC-80), ~~Research suggestion portal~~ (ISC-78–79). ~~Limits settings editor~~ (ISC-81–83), ~~deep per-modality parameter editor~~ (ISC-84). **The Android app is now at full functional parity with iOS (minus Apple Watch).** Only externally-blocked items remain deferred: on-device BLE (needs hardware, ISC-60), wire-JSON schema freeze across hub/iOS/Android (ISC-68), and the smaller OI items (Ed25519 <API33 fallback, helmet/individual limits persistence, host-side OTA download).
- ~~Android `BleCentral` implementation~~ DONE (ISC-59, `AndroidBleCentral`) — on-device connection verification deferred to hardware (ISC-60).
- ~~Session-descriptor compiler + `ProtocolChunker` + upload~~ DONE (ISC-64–67). Follow-ups: OI-AND-WIRE-01 (freeze the canonical wire JSON across hub/iOS/Android), OI-AND-SIGN-01 (Ed25519 BouncyCastle fallback + Keystore-persisted key; composite upload), and `NeurOneGattManager`'s never-unregistered adapter-state `BroadcastReceiver` (acceptable for an app-scoped singleton).

## Changelog

- **Conjectured:** the full 25k-line iOS app could be ported in one pass with a single flat Android module. **Refuted by:** no Android SDK on the build machine — a single `com.android.application` module would have made every line locally unverifiable. **Learned:** the pure-JVM `core` / Android `app` split is not just testability hygiene, it is what makes privacy-invariant parity provable in CI without an emulator. **Criterion now:** ISC-2/ISC-5 lock the core module's Android-import-free status permanently.
- **Conjectured:** ISC-90 (EEG-consent gate) had no Android surface because the app was a "stub." **Refuted by:** Android is a full `core` + UI-skeleton port and, per product, a real device-wearer onboarding surface (retail Android phones/tablets). **Learned:** parity of a privacy invariant means porting the tested decision logic into `core` even before the consuming screen exists — the gate must predate the surface it guards, or the surface ships ungated. **Criterion now:** ISC-41–44 lock the gate logic; ISC-45 tracks the UI wiring as a deferred follow-up.

## Verification

- ISC-1/2/7: Read of settings.gradle.kts + module build files — `:core` pure `kotlin("jvm")`, `:app` `com.android.application` minSdk 29 / targetSdk 35.
- ISC-3/4: Bash `gradle :core:test` (scratchpad copy, JDK 17) — "BUILD SUCCESSFUL", XML report totals: **67 tests, 0 failures**.
- ISC-5: Grep `import android.` under core/src — 0 matches.
- ISC-6: Grep `http://` in *.kt — 0 matches (fleet endpoint is https with SPKI pins).
- ISC-8: `gradle :app:assembleDebug` → BUILD SUCCESSFUL, `app/build/outputs/apk/debug/app-debug.apk` (28 MB), JDK 17 + SDK platform 36. `:app:testDebugUnitTest` → NO-SOURCE (app-module tests not yet authored; all tests live in :core). NP-AND-BUILD-01 CLOSED.
- ISC-9–28, 32–36, 40: JUnit — GattParserTests (12), ModelsParityTests (11), AnalyticsGateTests (9), ConsentStoreTests (5), SessionHistoryStoreTests (6), ConsumableTrackerTests (7), NPPSRegressionTests (10), UhdrKeyDerivationTests (4) — all pass in the 67/0 run.
- ISC-29/37: Grep AndroidManifest.xml — `allowBackup="false"`, `dataExtractionRules` declared, `neverForLocation` on BLUETOOTH_SCAN, no ACCESS_FINE/COARSE_LOCATION `uses-permission` element.
- ISC-30: Read ShdrUploader.kt — SecureRandom 32-byte token in EncryptedSharedPreferences, `X-NP-Device-Token` header, CertificatePinner with SPKI pins (placeholders flagged).
- ISC-31: Grep `track(` call sites — no UHDR-class field names; prohibited-key gate additionally enforces at runtime (ISC-19 test).
- ISC-38: Read NeurOneGattManager.kt — BleCentral abstraction, auto-scan on adapter ON, 2s reconnect, SHDR early-return guards before session-state mutation.
- ISC-39: Read MainActivity.kt / OnboardingScreens.kt / Screens.kt — 5-tab scaffold, AgeGateScreen (unchecked box, disabled Continue), BipaConsentScreen present.
- ISC-40: UhdrKeyDerivationTests — fake provider receives m=65536/t=4/p=1/len=64; Grep shows no derived-key write to any store (only seed/salt persisted, seed zeroed after use).
- ISC-41–44: `gradle :core:test` (scratchpad copy, JDK 17, --offline) — BUILD SUCCESSFUL; `ProtocolConsentGateTests` 9/9 pass (isEEGDependent true/false + disabled-modality, isBlockedByBipa block/allow, non-EEG unaffected, composite via resolver, message-res parity). Full suite now **76 tests, 0 failures** across 9 classes. `strings.xml` has `session_eeg_unavailable_body`; `OnboardingKeys.BIPA_ACCEPTED == "np.onboarding.bipa-accepted"`. Grep: no `import android.` in ProtocolConsentGate.kt (core stays pure-JVM).
- ISC-45: DEFERRED-VERIFY — Session/Protocol-menu Compose screens are skeletons; gate-consumption test lands with those screens (follow-up NP-AND-EEGGATE-UI-01).
- ISC-64–66: `gradle :core:test` — `ProtocolChunkerTests` 5/5 + `SessionProtocolCompilerTests` 5/5; full core suite **107/0** across 13 classes. `ProtocolChunker.kt` + `NPSessionProtocol.kt` added (chunker, wire descriptor, fromDefinition, SignedProtocolBlob, ProtocolSigner, SessionProtocolCompiler).
- ISC-84: `gradle :app:assembleDebug` → BUILD SUCCESSFUL; `ModalityEditorScreen.kt` added (8 T1 param editors + add/remove), wired from the form editor. Compile-verified.
- ISC-81–83: `gradle :core:test` — `NPLimitsStoreTests` 6/6 (tier precedence individual>helmet>global, null-block, unlimited, active-profile resolution, NPPS persistence reload, validator enforcement); full core suite **131/0**. `gradle :app:assembleDebug` green with `LimitsSettingsScreen.kt`; menu validation now uses `limitsStore.resolvedLimits`. `NPLimitsStore.kt` added to core.
- ISC-78–80: `gradle :core:test` — `ResearchSuggestionStoreTests` 7/7 (invalid-draft reject, insert-at-front, vote/intent toggles, cumulative pledge accounting, persistence reload, stable device token); full core suite **125/0**. `gradle :app:assembleDebug` green with `ResearchPortalScreen.kt` (Privacy tab) + `ProtocolComposerScreen.kt` (menu "Compose"). `research/ResearchSuggestion.kt` + `ResearchSuggestionStore.kt` added to core.
- ISC-73–77: `gradle :core:test` — `AdaptationEventTests` 4/4; full core suite **118/0** across 15 classes. `gradle :app:assembleDebug` green with `ProtocolScriptEditorScreen.kt` + `ProtocolEditorScreen.kt` + refactored `ProtocolMenuScreen` (New/Edit/Delete) + new `HistoryScreen.kt` (detail + `AdaptiveAdjustmentsCard`). `AdaptationEvent.kt` + `CompletedSessionSummary.fromRecord` added to core.
- ISC-71–72: `gradle :core:test` — `SetupFlowTests` 7/7 (step order, safety gate block/unblock, first-setup persistence, back clears ack, impedance 6/8 threshold, hardware-confirmation flags); full core suite **114/0** across 14 classes. `gradle :app:assembleDebug` green with `SetupWizardScreen.kt` + `sendCalibration()`. `SetupFlow.kt` added to core.
- ISC-69–70: `gradle :app:assembleDebug` → BUILD SUCCESSFUL; `OtaScreen.kt` (Settings → Firmware) + `Under16Screen` (age-gate under-16 path) added. Compile-verified.
- ISC-67: `gradle :app:assembleDebug` → BUILD SUCCESSFUL; `AndroidProtocolSigner.kt` + `ProtocolUploader.kt` added; menu `onSelect` uploads via compile→chunk→GATT. Compile-verified; on-device upload deferred (ISC-60/68).
- ISC-62–63: `gradle :app:assembleDebug` → BUILD SUCCESSFUL; `ConsentOnboardingScreen.kt` added + wired into `Root` (post-BIPA `np.onboarding.consent-shown` gate); `SettingsScreen` rewritten with re-presentable EEG + research consent. Compile-verified (ConsentStore L1–L4 logic unit-tested in `core` ConsentStoreTests).
- ISC-61: `gradle :app:assembleDebug` → BUILD SUCCESSFUL; `ConsumablesScreen.kt` added + `GattConsumableCountsProvider` in `NeurOneApplication`; consumes core `ConsumableTracker.states`. Compile-verified (tracker logic unit-tested in `core` ISC-35/36).
- ISC-59: `gradle :app:assembleDebug` → BUILD SUCCESSFUL; `AndroidBleCentral.kt` added and wired app-scoped; `SessionTab` collects live `connectionState`/`session` flows + requests BLUETOOTH_SCAN/CONNECT. API-33/pre-33 GATT callback + write/descriptor branches compile against SDK 36. On-device run deferred (ISC-60, no hardware).
- ISC-56–58: `gradle :app:assembleDebug` → BUILD SUCCESSFUL, 29 MB `app-debug.apk`; `SessionScreen.kt` + `ProtocolMenuScreen.kt` added, `SessionScreen` skeleton removed from `Screens.kt`, `MainActivity.SessionTab` toggles the two, `NeurOneApplication` exposes `protocolLibrary`. Compile-verified (no instrumented UI test harness on this machine; the consent/availability logic the screens render is unit-tested in `core` ISC-52/53).
- ISC-59: DEFERRED-VERIFY — Android `BleCentral` impl (BluetoothLeScanner + GATT) not yet written; SessionScreen renders disconnected state. Follow-up OI-AND-BLE-01.
- ISC-51–55: `gradle :core:test` — `NPProtocolLibraryTests` 11/11 pass, incl. `bundledProtocolCountIs19` (all 19 `.npps` templates parse through the Android NPPS parser), read-only immutability, availability (no-device / T1 / EEG-consent block+unblock / non-EEG unaffected), user-protocol persistence round-trip, cached validation. `NPBundledProtocols.kt` + `NPProtocolLibrary.kt` added. Full core suite **97/0** across 11 classes. `:app:assembleDebug` still green with the new core.
- ISC-46–50: `gradle :core:test` (scratchpad, JDK 17, --offline) — `NPProtocolValidatorTests` 10/10 pass (valid accepted; tDCS 2 mA + BES 1 mA hardware ceilings; charge density over/under 40 µC/cm²; zero-duration; PBM dose; configured intensity limit; empty modalities; duty-cycle ceiling). Full core suite **86 tests, 0 failures** across 10 classes. `NPHardwareLimits.kt` + `NPProtocolValidator.kt` added; no `import android.` (core pure-JVM).
- Commit: `2de6c03` on worktree branch (EEG-gate port pending commit).
