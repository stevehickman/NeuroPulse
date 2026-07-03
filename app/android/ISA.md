---
task: "Android version of the NeuroPulse iOS app"
slug: 20260702-android-core-app
project: NeuroPulse
effort: E3
effort_source: classifier
phase: verify
progress: 39/40
mode: autonomous
started: 2026-07-02
updated: 2026-07-02
---

# ISA — NeuroPulse Android App

## Problem

NeuroPulse has a launch-track iOS app (94 Swift files, ~25k lines, ISA at `app/ios/ISA.md`, 146/164 ISCs verified) but no Android app. Android is required for market coverage at T1 launch; the June 2026 code review explicitly noted "Android app code is not present in this repo," and the i18n architecture (canonical JSON → platform adapters) was designed with an Android XML extension point that has no consumer. Every privacy invariant locked in CLAUDE.md (UHDR/SHDR separation, two consent subjects, engagement_tier coarsening, sessionDay timestamp coarsening, prohibited analytics keys, opaque warranty token) must exist on Android with byte-for-byte parity or the platform becomes a privacy regression vector.

## Vision

An Android user gets the identical NeuroPulse experience their iOS counterpart gets: three-minute consent onboarding, BLE hub connection, live session display with coherence and breathing ring, session history at day-granularity, consumable reminders, OTA updates — with every privacy decision visible, revocable, and plain-language. Engineers maintain one mental model across platforms because the Android module structure, class names, GATT UUIDs, wire formats, and consent semantics mirror the iOS codebase one-for-one.

## Out of Scope

Wear OS companion app (parallels the Apple Watch app — Year 1 post-launch). T2 clinical cloud features (FHIR, multi-patient dashboard, scripting API). Research anonymization engine (runs in firmware). Backend server infrastructure. Google Play submission assets (Data Safety form content is specified by NP-APP-TELEMETRY-001 but filing is a separate workstream). Full visual parity of every iOS screen — this ISA covers the core module port completely and the app-module UI skeleton; screen-by-screen completion continues in follow-up tasks. HealthKit-equivalent (Health Connect) integration — deferred until data-residency analysis (parallel of NP-APP-ROADMAP-001 §9.1) is written for Health Connect.

## Constraints

- **Kotlin + Jetpack Compose, native only.** No cross-platform wrapper (Flutter/RN/KMP-UI) — the program is native-first on both platforms.
- **Two-module split:** `core` is pure-JVM Kotlin (no Android imports) so all privacy-critical logic is unit-testable without an Android SDK; `app` is the Android shell. Mirrors `NeuroPulseShared` SPM package.
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
- [ ] ISC-8: [DEFERRED-VERIFY] `gradle :app:assembleDebug` succeeds on a machine with the Android SDK (follow-up: NP-AND-BUILD-01).

### GATT layer parity

- [x] ISC-9: `GattUuids.kt` contains all 16 UUIDs byte-identical to iOS `NPUUID` (service, 14 in `all`, warrantyToken, firmwareVersion) — string-compare test.
- [x] ISC-10: `GattUuids.all` contains exactly 14 UUIDs and excludes warrantyToken and firmwareVersion — unit test.
- [x] ISC-11: `GattParser.parseSessionState` decodes little-endian uint32 epoch from canonical bytes — unit test.
- [x] ISC-12: `GattParser.parseSessionStatus`, `parseHrvCoherence`, `parsePacerPhase`, `parseImpedanceResult`, `parseConsumableStatus`, `parseZoneModuleStatus`, `parseOtaStatus`, `parseFirmwareVersion` each return correct values for canonical test byte sequences — unit tests.
- [x] ISC-13: All parsers return null (not throw) on short input — unit test per parser.
- [x] ISC-14: OTA opcodes (0x01–0x05, 0x10–0x12) and calibration opcodes (0x01–0x04) match iOS raw values — unit test.

### Models parity

- [x] ISC-15: `SessionState`, `SessionStatus`, `PacerPhase`, `HRVData` exist in core with the same fields/raw values as `NeuroPulseShared/SessionState.swift` — unit test on raw values.
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

### NPPS protocol engine

- [x] ISC-32: NPPS lexer ports compound digit-idents (`660_808nm`, `660_808_1064nm`), hyphenated tags (`wind-down`), and `mW_cm2` unit — regression unit tests mirroring iOS `NPProtocolLibraryTests`.
- [x] ISC-33: NPPS serializer quotes tags containing non-`[A-Za-z0-9_]` chars; serialize→reparse preserves `wind-down` as a single tag — round-trip test.
- [x] ISC-34: All bundled NPPS protocol templates parse without error — unit test.

### Consumables and session flow

- [x] ISC-35: `ConsumableTracker` accepts any `ConsumableCountsProviding` implementation (DI), computes `isLow` correctly incl. intranasal single-use threshold 0 — unit tests.
- [x] ISC-36: Snooze state loads before count observation begins (iOS init-order bug not reintroduced) — unit test.

### App module shell

- [x] ISC-37: Manifest declares BLUETOOTH_SCAN (`neverForLocation`) + BLUETOOTH_CONNECT and no location permission — `Grep`.
- [x] ISC-38: `NeuroPulseGattManager` exists with `BleCentral` abstraction (testable without hardware), auto-scan on adapter-on, 2s reconnect delay, SHDR/UHDR guard on characteristic routing.
- [x] ISC-39: Compose UI skeleton: bottom-nav main scaffold with Session, History, Consumables, Consent, Settings destinations + age gate and BIPA screens present as composables.
- [x] ISC-40: `UhdrKeyManager` derives a 64-byte key via an `Argon2Provider` interface (params m=65536, t=4, p=1 asserted by unit test against a fake provider) and never persists the derived key — `Grep` shows no key write to storage.

## Test Strategy

| isc | type | check | threshold | tool |
|-----|------|-------|-----------|------|
| 1–2, 5–7, 29–31, 37 | static | grep/read of build files, manifest, sources | exact match | Grep/Read |
| 3–4 | build | gradle :core:compileKotlin / :core:test | exit 0, 0 failures | Bash |
| 8 | build | assembleDebug on SDK machine | exit 0 | DEFERRED (NP-AND-BUILD-01) |
| 9–28, 32–36, 40 | unit | JUnit tests in core | all pass | Bash gradle :core:test |
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

- 2026-07-02: Two-module split (pure-JVM `core` + Android `app`) chosen because no Android SDK exists on the build machine — makes all privacy-critical logic locally verifiable, mirrors NeuroPulseShared. Alternative (single Android module) rejected: zero local verification possible.
- 2026-07-02: Delegation floor relaxed from 2 to 1 (Forge only) — show-math: the port is single-repo sequential work in an already-isolated worktree; a second write-agent would contend on the same Gradle skeleton and package convention; directed lookups were done directly per the delegation gate.
- 2026-07-02: NPPS engine (1,488-line NPProtocolScripting.swift) delegated to Forge in background — largest self-contained, convention-stable unit.
- 2026-07-02: Argon2id behind an `Argon2Provider` interface — Android implementation to vendor signal-org argon2 or lazysodium as SOUP (parallel of iOS PHC vendoring, NP-SW-001 SOUP table entry required before beta).
- 2026-07-02: Local verification runs from a scratchpad copy of `app/android` — macOS TCC blocks Gradle/daemon directory creation inside `~/Documents` for shell children on this machine. Sources in the worktree are canonical; rsync → scratchpad → `gradle :core:test`.
- 2026-07-02: Gradle daemon pinned to Temurin JDK 17 locally (Homebrew Gradle defaults to JDK 26, which Kotlin 2.0.21 rejects with `IllegalArgumentException: 26.0.1`). Machine-specific `org.gradle.java.home` is documented in `gradle.properties` as a comment, not committed.
- 2026-07-02: Fixed Kotlin init-order bug in SessionHistoryStore — body-declared comparator `val` was null during `init { load() }`, silently emptying every reload via runCatching; moved to companion object. Regression caught by `insertionIndexSurvivesReload`.
- 2026-07-02: Forge's NPPS regression test wrapped the mW_cm2 probe in a nonexistent `limits_probe` modality block; removed the block (the mW_cm2 lexing assertion is standalone). All 67 tests pass after fix.

## Changelog

- **Conjectured:** the full 25k-line iOS app could be ported in one pass with a single flat Android module. **Refuted by:** no Android SDK on the build machine — a single `com.android.application` module would have made every line locally unverifiable. **Learned:** the pure-JVM `core` / Android `app` split is not just testability hygiene, it is what makes privacy-invariant parity provable in CI without an emulator. **Criterion now:** ISC-2/ISC-5 lock the core module's Android-import-free status permanently.

## Verification

- ISC-1/2/7: Read of settings.gradle.kts + module build files — `:core` pure `kotlin("jvm")`, `:app` `com.android.application` minSdk 29 / targetSdk 35.
- ISC-3/4: Bash `gradle :core:test` (scratchpad copy, JDK 17) — "BUILD SUCCESSFUL", XML report totals: **67 tests, 0 failures**.
- ISC-5: Grep `import android.` under core/src — 0 matches.
- ISC-6: Grep `http://` in *.kt — 0 matches (fleet endpoint is https with SPKI pins).
- ISC-8: DEFERRED-VERIFY — Android SDK machine required (follow-up NP-AND-BUILD-01).
- ISC-9–28, 32–36, 40: JUnit — GattParserTests (12), ModelsParityTests (11), AnalyticsGateTests (9), ConsentStoreTests (5), SessionHistoryStoreTests (6), ConsumableTrackerTests (7), NPPSRegressionTests (10), UhdrKeyDerivationTests (4) — all pass in the 67/0 run.
- ISC-29/37: Grep AndroidManifest.xml — `allowBackup="false"`, `dataExtractionRules` declared, `neverForLocation` on BLUETOOTH_SCAN, no ACCESS_FINE/COARSE_LOCATION `uses-permission` element.
- ISC-30: Read ShdrUploader.kt — SecureRandom 32-byte token in EncryptedSharedPreferences, `X-NP-Device-Token` header, CertificatePinner with SPKI pins (placeholders flagged).
- ISC-31: Grep `track(` call sites — no UHDR-class field names; prohibited-key gate additionally enforces at runtime (ISC-19 test).
- ISC-38: Read NeuroPulseGattManager.kt — BleCentral abstraction, auto-scan on adapter ON, 2s reconnect, SHDR early-return guards before session-state mutation.
- ISC-39: Read MainActivity.kt / OnboardingScreens.kt / Screens.kt — 5-tab scaffold, AgeGateScreen (unchecked box, disabled Continue), BipaConsentScreen present.
- ISC-40: UhdrKeyDerivationTests — fake provider receives m=65536/t=4/p=1/len=64; Grep shows no derived-key write to any store (only seed/salt persisted, seed zeroed after use).
- Commit: `2de6c03` on worktree branch.
