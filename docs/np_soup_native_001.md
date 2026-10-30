# Native App SOUP Inventory — Swift (iOS) and .NET (Windows)

**Project:** NeurOne
**Document:** NP-SOUP-NATIVE-001
**Revision:** 4
**Date:** 2026-10-21
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Software Engineering
**Approved By:** — (DRAFT, not approved)
**References:** `app/ios/project.yml` (the SwiftPM dependencies), `app/NeurOneShared/Package.swift`, `app/windows/NeurOne/NeurOne.csproj`, `app/windows/NeurOne.Tests/NeurOne.Tests.csproj`, `NP-SOUP-KOTLIN-001` and `NP-SOUP-RUST-001` (the method precedent and the rules, §2a of the former), `CLAUDE.md` §22
**Related Issues:** GitHub #569, `OI-SOUPKT-01` item 4
**Gate:** G2
**IEC 62304 Class:** Not assigned. See `NP-SOUP-KOTLIN-001` §3, whose argument applies unchanged.
**Supersedes:** None
**Change Summary:** Rev 4 (2026-10-30) — **ViewInspector 0.10.5 is added as a test-only SwiftPM dependency** of `NeurOneTests` (issue #569, `UploadBannersTests`), pinned exactly. It is not linked into the app. Rev 3 (2026-10-21) — **the watchOS app is retired and its project is deleted** (`NP-APP-ROADMAP-001` Rev 9): the record covers iOS and Windows only, and the PLCrashReporter row no longer says "(not watchOS)". No dependency is added, removed or changed. Rev 2 (2026-10-17) — **the vendored Argon2 is removed and PostHog is pinned** (`OI-SOUPKT-01`). iOS derives the UHDR key through the Rust core (`deriveUhdrKey`); `NeurOneArgon2`, `Argon2Bridge.swift` and the 12 vendored files are deleted. Before deleting, the vendored source was compiled and called on a fixed input, and the core was made to reproduce its output byte for byte (a test in `bundle_key.rs`), so a key made by an earlier build is unchanged. PostHog is `exactVersion` 3.60.0 in `project.yml`, so the version that ships is the one this record reads. Rev 1 (2026-10-17) — first issue. Closes the gap `NP-SOUP-KOTLIN-001` §1 named: the Swift package manifest, the Swift package dependencies and the NuGet references had no record. Written together with the removal of `NSec.Cryptography` from the Windows app (the Ed25519 signer now calls the Rust core, `CLAUDE.md` §22), so the Windows runtime inventory is one library.
**Review Cadence:** On any change to a SwiftPM dependency or a `PackageReference`, and at G2

---

## 1. What this record is, and is not

The iOS app is Swift; the Windows app is C# on .NET 8. They are the native apps that predate the shared Compose UI (`CLAUDE.md` §20) and are still built. Everything they link that NeurOne did not write is SOUP. This record **lists** it and says what has and has not been checked. The shared Rust core they all link is `NP-SOUP-RUST-001`; the shared Kotlin app is `NP-SOUP-KOTLIN-001`. The operating-system frameworks (Foundation, CryptoKit, Security, CoreBluetooth, the .NET 8 runtime) are platform, not SOUP, and are not listed. Build tools (Xcode, XcodeGen, the .NET SDK) are not in the shipped product.

## 2. Inventory

| Item | Version | Platform | Shipped | Licence (as declared) | Role |
|------|---------|----------|---------|-----------------------|------|
| PostHog (`posthog-ios`) | **3.60.0, exact** (`exactVersion` in `app/ios/project.yml`) | iOS | Yes | MIT (the repository's `LICENSE` at 3.60.0) | The analytics backend behind the research-consent gate (`PostHogAnalyticsBackend.swift`). A fault in it can send events the user did not allow (§5) |
| libwebp, vendored in PostHog as target `phlibwebp` | with PostHog | iOS | Yes | BSD-3-Clause (the vendored `COPYING`) | Image coding inside the SDK |
| PLCrashReporter, vendored in PostHog as `PHPLCrashReporter` | with PostHog | iOS | Yes | MIT (the vendored `LICENSE`, with the notices of the upstream project and its dependencies) | Crash capture inside the SDK |
| protobuf-c, vendored in `PHPLCrashReporter` | with PostHog | iOS | Yes | BSD-2-Clause (its own header notice; not checked against the file) | PLCrashReporter's report encoding |
| ViewInspector (`nalexn/ViewInspector`) | **0.10.5, exact** (`exactVersion` in `app/ios/project.yml`) | iOS tests | No | MIT (the repository's `LICENSE` at 0.10.5) | Renders a SwiftUI view in a test and reads the view tree (`UploadBannersTests`), so what a banner shows is tested without a device. A dependency of the `NeurOneTests` target alone; its `Package.swift` at 0.10.5 names no dependency of its own and requires iOS 15 or later (the app targets 17) |
| System.Security.Cryptography.ProtectedData | 8.0.0 | Windows | Yes | MIT | DPAPI wrapping of the signing seed in `SessionProtocol.cs`. It is a platform key-protection wrapper, which §22 leaves to the platform. It has no dependency on .NET 8 (the nuspec lists `System.Memory` only for .NET Standard 2.0) |
| Microsoft.NET.Test.Sdk | 17.11.1 | Windows tests | No | MIT | Test host |
| xunit | 2.9.2 | Windows tests | No | Apache-2.0 | Test framework |
| xunit.runner.visualstudio | 2.8.2 | Windows tests | No | Apache-2.0 | Test adapter |

Removed in this revision's change: **`NSec.Cryptography`**. It was declared at 24.10.0, a version nuget.org does not have (its releases run 24.4.0, 24.5.0-preview.1, 24.9.0-preview.2, 25.x), so the restore resolved the nearest stable higher one, 25.4.0 (MIT; it depends on `libsodium` [1.0.20.1, 1.0.21)). It signed the hub descriptor. `SessionProtocolSigner` now keeps the 32-byte seed under DPAPI as before, the file format unchanged so an existing key still works, and asks the core (`ed25519PublicKey`, `ed25519Sign`). No .NET cryptographic library other than the platform's remains in the app.

The licence entries were read from the packages' own nuspec files and repositories on 2026-10-17; they are **declared**, not checked against the licence texts, and the vendored-in-PostHog entries were read from the files at tag 3.60.0 and not at the version that resolves. None is copyleft (`NP-SOUP-KOTLIN-001` §2a, rule 5).

## 3. Selection rules applied

`NP-SOUP-KOTLIN-001` §2a binds these apps too (`CLAUDE.md` §22).

1. **No unused library.** None found: every item above is imported or linked.
2. **No pre-release.** None declared, and PostHog is pinned exactly, so no later release (stable or not) enters a build without an edit to `project.yml`.
3. **Cryptographic work is the core's.** Met on Windows (Ed25519 signing) and on iOS (the UHDR key, `deriveUhdrKey`; the vendored PHC Argon2 is deleted, Rev 2). The Swift app has no cryptographic library of its own; it uses CryptoKit and the Keychain, which are platform (the platform keeps key custody, `CLAUDE.md` §22).
4. **No managed-runtime code on the helmet.** These are phone and computer apps.
5. **Copyleft only if it does not reach NeurOne's code.** None is copyleft.

## 4. What was not done

- **No `Package.resolved`.** PostHog is pinned `exactVersion` 3.60.0, so the version is known. Its transitive packages are not recorded: PostHog's own manifest at 3.60.0 names three test-only dependencies (Quick, Nimble, OHHTTPStubs), which SwiftPM fetches for the graph but does not link into the product; that was read from the manifest, not from a resolved build.
- **No vulnerability scan** of PostHog or its vendored code. SwiftPM is not read by the scan job of `NP-SOUP-KOTLIN-001` §6.2, which reads Gradle and Cargo locks, and the NuGet references are not either.
- **No anomaly list** for PostHog beyond its repository's public issues, which were not read.
- **ViewInspector is read from its manifest and licence at tag 0.10.5, not from a resolved build,** and it has no vulnerability scan or anomaly list (SwiftPM is not read by the scan job). It reaches the test bundle only. It reads SwiftUI's private view-tree layout, so a new iOS release can break its tests without any product change; that fails the build, it cannot reach a user.
- **No NuGet lock file.** The remaining references are fixed versions (ProtectedData is platform-adjacent and the others are test-only); `RestorePackagesWithLockFile` would add `packages.lock.json`, which needs a restore this environment cannot run. Not adopted: one shipped library does not earn a lock.

## 5. Hazard reading

| Item | Fault | Effect | Held by |
|------|-------|--------|---------|
| PostHog | Sends an event, or identifies a user, with research consent withdrawn or never given | UHDR-class behaviour data leaves the device against the user's decision | `ResearchAnalyticsGate` and `WarrantyAnalyticsGate`: the backend is configured only after a gate allows it, and withdrawal opts the SDK out (`PostHogAnalyticsBackend.swift`, `ResearchAnalyticsGateTests`; `CLAUDE.md` §6.2). Whether the SDK honours `optOut` for everything it captures was not tested |
| PostHog / PLCrashReporter | A crash report carries memory contents | UHDR-class data in a report that leaves the device | Not held beyond the same gate: the crash reporter is part of the SDK that the gate keeps off |
| PHC Argon2 (removed, Rev 2) | A wrong derivation | A UHDR key that differs from the one the data was sealed with | Removed. The core's `deriveUhdrKey` reproduces the vendored reference byte for byte on a pinned vector (`bundle_key.rs`) and is tested at the locked cost; a Swift test pins the same bytes through the marshalling (`testUhdrKeyIsTheCoresArgon2id`) |
| ProtectedData | The seed is unwrapped by another Windows user | The protocol-signing key is exposed, so a descriptor could be signed in the user's name; no stimulation enable follows (the hub's safety MCU owns it, `CLAUDE.md` §4.2) | DPAPI user scope |
| ViewInspector | Reads a view tree wrongly, so a banner test passes or fails when it should not | A banner test that does not test (a false pass); none in the product, because the library is test-only and not linked into the app | `ProtocolUploadScreenTests` covers the state that decides when a banner is shown without it; the banner text itself is one `Text` of a string the flow produced and is also asserted by `ProtocolUploadFlowTests` |
| the rest | A fault in a test library | None in the product (not shipped) | — |

## 6. Keeping this record true

- A change to a SwiftPM dependency or a `PackageReference` changes §2 in the same change, and §5 if the item could reach UHDR-class data. A PostHog version change is an edit to `exactVersion` and to §2 together.
- The vendored Argon2 (`NeurOneArgon2`, 12 files, phc-winner-argon2 2021 release, CC0-1.0 OR Apache-2.0) was removed in Rev 2; its file hashes are in git history at `app/NeurOneShared/Sources/NeurOneArgon2/phc/VENDOR.md`.
- This record does not extend `NP-SW-001` §9.4 (the firmware register).
