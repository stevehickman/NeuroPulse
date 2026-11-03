# OI-UI-KMP-05 — retire the SwiftUI iOS app: the plan, and the decisions it needs

**Status:** DRAFT, not sent. **From:** Software (Claude Code session, 2026-11-03). **To:** the principal.

**What is asked.** Whether to delete `app/ios` (the SwiftUI app), when, and the five rulings in §6. Nothing here is a decision, and nothing in the repo may act as if the app is retired: `OI-UI-KMP-05` stays open and every Swift file stays until you answer. The inventory below was taken from the tree on 2026-11-03 (`main` at `0e9c0141`); counts are what a search found, not a guarantee.

**What is known and what is assumed.** Known: every Swift test group has been compared with Kotlin and ported, ruled out or retired (`OI-UI-KMP-05` in `docs/status/pending-decisions.md`). Assumed: your 2026-10-20 statement that nobody outside the team runs the Swift app still holds, so there is no user data to migrate when the shared host takes the same bundle id (`life.neurone.app`).

## 1. What stands between the Swift app and its deletion

| # | Gate | State |
|---|------|-------|
| 1 | Every Swift test is ported, ruled out or retired | **Done** (#681, #759, #766, #772) |
| 2 | Every requirement the Swift tests verify is verified elsewhere, or retired in place (§3) | **Seven parts left, all rulings to retire in place** (the other three were retagged 2026-11-03) |
| 3 | The shared iOS host has run on a device: `OI-UI-KMP-06`, `OI-PRES-07` (`docs/human-input/oi-pres-07-ios-device-run.md`) | **Open**: it has only compiled and run on the simulator in CI |
| 4 | Your five rulings (§6) | **Open** |

Gates 2 and 3 can proceed now; gate 4 is yours.

## 2. What goes

| Item | Size or place | Note |
|------|---------------|------|
| `app/ios` | 174 files, 166 Swift; `ISA.md` has 86 ISCs | The app, its tests (`NeurOneTests`), `project.yml` (XcodeGen), `SupportingFiles` |
| Swift package `app/NeurOneShared/Sources` and `Package.swift` | `SupportedLocales.swift`, `SessionState.swift` | Used by the Swift app only (the Compose host does not link it). The JSON resources in `app/NeurOneShared/Resources` and `TestData` **stay**: Kotlin and the web read them |
| Generated Swift outputs of six sync scripts | `sync-constants`, `sync-use-cases`, `sync-clinician-tiers`, `sync-uhdr-elements`, `sync-socket-map`, `sync-locales` (the String Catalog) | The scripts keep their Kotlin and web outputs; the Swift writers and their `--check` paths go |
| `scripts/build-npps-xcframework.sh` | used only by `app/ios/project.yml` and `ios-ci.yml` | The Compose host builds the Rust core through Gradle cinterop |
| `.github/workflows/ios-ci.yml` | `xcodebuild test` and SwiftLint | `kotlin-ios-ci.yml` (compile, link, simulator tests, `iosApp` build) stays |
| CodeQL Swift scope, relevance globs | `codeql.yml`, `tooling-ci.yml`, `web-ci.yml` | Several self-check `assert scope… app/ios/…` lines name Swift files and must be re-pointed to Kotlin files, not just deleted |
| `.gitignore` lines for `app/ios/*.xcodeproj`, `Frameworks`, `build` | | |
| `ModelsParityTests` | reads `app/ios/NeurOne/Models/OTAModels.swift` | Retire; the enum tables are pinned by the Kotlin tests added in #759 |
| "Port of iOS …" comments | about 15 Kotlin files, 4 C# files | Comments only. Leave (they are history) or reword in one sweep |

**What stays and must not be touched:** `app/NeurOneUI` (including `iosApp`), `common/`, `app/android`, `app/web`, `app/windows` (see §7).

## 3. Requirements that would lose their only test

Found by comparing every `@verifies` tag in `app/ios/NeurOneTests` with the tags in every other test file, down to the part, with a throwaway comparison script that is not committed (`bun scripts/req-trace.ts` reports untested requirements, not which tests are Swift-only).

| Requirement part | Swift test | Proposed |
|------------------|------------|----------|
| `DI-SAFE-09`, `REQ-CVNS-09#f` | cervical gate in `SessionProtocolUploaderTests`, `ProtocolUploadScreenTests` | **Retagged 2026-11-03.** The Kotlin tests in `UploadRefusalTest` (cervical refused while a cutoff is unread, another person's cutoff needs one confirmation, Mode 3 too) verify them; they are tagged `REQ-SW03-03` only |
| `DI-SAFE-06` | `testSignAndUpload` | **Retagged 2026-11-03** (`UploadRefusalTest` and the signature case of `HubDescriptorCompilerTests`). `UploadRefusalTest` signs with Ed25519 and writes the chunks |
| `REQ-SW03-08` | `UHDRKeyManagerTests` (7) | **Retire in place with the app** (your ruling, `OI-PRES-05`): the Swift key manager's `authenticate()` has no caller, so the requirement describes code that never ran; the presence gate (`REQ-PRES-01` to `-09`) replaces it |
| `REQ-SW03-18#a` | `ProtectedDefaultsTests` | **Retire in place with the app** (your ruling, 2026-10-21) |
| `REQ-UHDRBK-01#a` to `#e` | `UHDRBackupFormatTests` | **Retire the row with the app** (your ruling, 2026-10-21): (a) to (c) are met by `REQ-SW03-29`, (d) is `REQ-SW03-29` (f), (e) is moot |

`REQ-UHDRFP-01` is not in the list: the shared iOS code is tested since #680.

Retiring a requirement is done in place (`CLAUDE.md` §18: mark retired, never delete), in the same change as the code that carried it.

## 4. Documents

| Document | Change |
|----------|--------|
| `NP-APP-ISA-001` (`app/ios/ISA.md`, DHF index row) | Controlled and ACTIVE. Mark retired in place when `app/ios` goes; it cannot stay at a path that no longer exists, so it moves to `docs/superseded/` with the DHF row pointing there. The 86 ISCs have no `REQ-` cross references to rehome |
| `NP-DHF-001` §software items (rows naming `app/ios/NeurOne/…` for BLE, Session, Protocol upload, History, …) | Re-point each to its Kotlin path (`app/NeurOneUI/shared`, `common/core`); a change-control entry |
| `NP-APP-ROADMAP-001`, `NP-SOUP-NATIVE-001` | The Swift half of the SOUP inventory is retired; the roadmap says the shared host is the iOS app |
| `NP-SW-001` | Retired rows above; a history entry |
| `NP-APP-PRESENCE-001` | `OI-PRES-05` closes ("the iOS app that ships does not meet `REQ-PRES-01` until the Swift app is retired") |
| `docs/status/unjustified-choices.md` (7 mentions), `pending-decisions`, `completed-decisions` | Update the paths; log the decision |
| Privacy analyses (`np_priv_analysis_002`, `np_priv_keysplit_001`) and other history | Leave: they describe what was reviewed |

All of this goes through the assign-at-merge revision fragments (`docs/revisions/README.md`).

## 5. Order

1. ~~Retag the Kotlin tests for `DI-SAFE-06`, `DI-SAFE-09`, `REQ-CVNS-09#f` (§3).~~ **Done 2026-11-03.**
2. **Device runs** of the shared iOS host and the presence gate (gate 3).
3. **Your rulings** (§6).
4. **PR A, plumbing, `app/ios` still present:** stop the sync scripts writing Swift; remove the Swift entries from the relevance globs and re-point the `assert scope…` self-checks; drop the SwiftLint and xcodebuild jobs from the required checks. CI stays green because nothing reads the Swift outputs any more.
5. **PR B, the deletion:** `app/ios`, the Swift package, `build-npps-xcframework.sh`, `ios-ci.yml`, `ModelsParityTests`, the `.gitignore` lines; retire the requirement rows; move `NP-APP-ISA-001`; DHF software-item rows; close `OI-PRES-05`; one decision log entry. A revert of PR B restores the app in one step.
6. **Follow-ups:** the "Port of iOS" comments; the same exercise for `app/windows` and the React `app/web` (§7).

## 6. What I need back

1. **Delete `app/ios`: yes, no, or after which date?** (My recommendation: yes, after the device runs.)
2. **May PR A land before the deletion** (it changes CI and generators but removes no Swift)? (Recommendation: yes; it makes the deletion a pure removal.)
3. **`NP-APP-ISA-001`: move to `docs/superseded/` (recommended) or keep a stub at the old path?**
4. **Required checks:** which GitHub status checks does the `main` ruleset require by name? Removing the iOS CI jobs while a ruleset still requires them blocks every PR; I cannot see the ruleset from here.
5. **Signing and the bundle id:** the shared host takes `life.neurone.app` (your 2026-10-21 ruling). Is the provisioning profile and App ID for that id in place, with HealthKit left off (the HealthKit read is retired)? I cannot check from here.

## 7. Not in scope, same pattern

`app/windows` (build-only, seals nothing, `SWR-MG-06`) and the React `app/web` (memory-only) are retired the same way when the shared desktop and browser apps cover them (`OI-UI-KMP-05`). Each needs its own pass of §3 and §4; neither is touched here. Do not include health data from any device when answering (`CLAUDE.md` §5).
