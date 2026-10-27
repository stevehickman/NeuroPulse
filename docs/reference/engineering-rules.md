# Engineering rules — detail for CLAUDE.md §20 and §22

> Relocated verbatim from CLAUDE.md at Rev 74 to slim the always-loaded core. **CLAUDE.md §20 and §22
> keep the rule; this file holds the layout, the mechanics and the library-selection detail.** Where the
> two differ, CLAUDE.md states the invariant and this file states how it is carried out. The checkers
> (`scripts/check-shared-placement.ts`, `scripts/check-section-refs.ts`) enforce the rule, not this prose.

## §20 detail — shared files: one home each

**A file whose content is common to more than one app (iOS, Android, Windows, web) lives in
`app/NeurOneShared/`, directly or in a subdirectory of it, and is never stored in one app's
directory for the others to reach into.** Subdirectories are made as needed: `TestData/` for
fixtures several apps' tests read (`hub-descriptor-golden.json`), `Resources/` for generated or
hand-written resources several apps bundle (`Localizable.xcstrings`), `Sources/` for the Swift package.

- **Decide at creation.** A new file that a second app will read, bundle or diff against is created in
  `app/NeurOneShared/` the first time, and every reference points there.
- **Moving is not copying.** Two copies of one file is a violation; keep one in `app/NeurOneShared/`.
  A thin re-export shim (`@_exported import NeurOneShared`) is the one sanctioned residue at an old path.
- **Scope.** Data and resource files (json, xcstrings, plist, xml, npps, csv and the like). An app's own
  source, project spec, manifest and docs stay in its tree. Files a toolchain demands per app
  (`Info.plist`, Gradle wrapper, lockfiles) are exempt by name in the checker.
- **Enforced** by `bun scripts/check-shared-placement.ts` (CI job `shared-placement` in
  `tooling-ci.yml`), which fails an `app/<A>/` file naming a data file under `app/<B>/` and any
  identical content stored under two apps.

**A file shared by an app and a build artifact that is not an app (the simulator bundle, firmware)
lives in `common/` at the repository root, never in an app's tree and never in `app/NeurOneShared/`.**
`app/NeurOneShared/` is for apps only. `scripts/` is not a build artifact: it makes them, so a
script reading a file does not make that file shared.

- **Layout.** `common/` mirrors the old tree it was lifted from, so relative imports between its
  files do not change (`lib/`, `types/`, `locales/`, `generated/`). Today it holds the TypeScript
  the web app and the simulator bundle (`simulator/js/vendor/npps-runtime.js`) both build from, the NPPS
  field table (`npps/fields.json`) and the Rust NPPS core (`npps-core/`) and its bindings (`npps-jni/` for Android, `npps-ffi/` the C ABI iOS and Windows link, built to WebAssembly for the web), `OI-NPPS-CORE-01`; every app's parser and descriptor compiler (Android, iOS, Windows, web) and the simulator's parser run on it; the serializers and validators are still per-platform.
- **Generated shared files are written into `common/`.** `bun scripts/sync-locales.ts` writes
  `common/generated/locales/` (git-ignored) and `bun scripts/sync-socket-map.ts` writes
  `common/lib/socketMap.generated.ts`. A generator never writes a shared file into one app's tree.
- **Tests of `common/` code live in `common/`**, beside the file (`common/lib/x.test.ts`), and run under the
  web app's vitest (`app/web/vite.config.ts` includes `../../common`). A test that needs an app's code
  (the web serializer, for example) is not a test of `common/`; it stays with the app.
- **`common/` is the lowest layer.** It imports nothing outside `common/` (packages aside), so it may
  not depend on an app. If it needs one, the shared piece was cut in the wrong place.
- **Nothing non-app reaches into `app/`.** An import, `require` or `#include` under `simulator/` or
  `firmware/` that names a path under `app/` is a violation. A comment naming one is prose.
- **Never a React dependency.** `common/` has no `node_modules` above it, so a module needing React
  or another package the web app installs belongs in the app, not here.

**The UI is written once, in `app/NeurOneUI/` (Compose Multiplatform).** `shared/` holds every screen and
the composition root (`AppServices`); `desktop/` (macOS, Windows), `web/` and `iosApp/` are thin hosts, and
Android's activity hosts the same screens. A host differs only through the `PlatformServices` seam
(storage, Bluetooth, permissions, analytics vendor) and the `expect`/`actual` set in `:core`
(`life.neurone.core.platform`). **A screen written for one platform only is a defect to be moved, not a
feature**; the screens not yet moved and the wiring not yet done are listed in `app/NeurOneUI/README.md`
and `OI-UI-KMP-00`. `:core` (`common/core`) is Kotlin Multiplatform: its sources are `commonMain`.

**Where `:core` lives (moved 2026-10-27, the principal's restatement of §20).** `:core` is the engine of every
Kotlin app (Android, desktop, iOS, web: `:shared` has `api(project(":core"))`), so it lives at `common/core`, not
under `app/android/`. One module, not several: its public packages (`life.neurone.core.*`) are unchanged and
no caller changed. `app/android/settings.gradle.kts` registers it with
`project(":core").projectDir = file("../../common/core")` and keeps the Gradle name `:core`; the Gradle build
still runs from `app/android` (`./gradlew :core:jvmTest`). `app/android/` now holds only the Android shell
(`:app`). Its build script reaches `../../protocols/predefined`, `../../locales`, `../../npps/fixtures` and
the Rust workspace at `..`. It also reads `app/NeurOneShared/TestData` (jvmTest resources and the generated
goldens), as `common/npps-core`'s Rust tests already do: the fixtures are shared by the iOS and Windows tests,
so they stay in `NeurOneShared` and this one read-only reach into `app/` for test data is a stated exception
to the lowest-layer rule, not a dependency on app code. `:core` depends on no `:app` code.

## §22 detail — Rust first, real-time and cryptographic code

**Real-time and cryptographic work is written in Rust, once, in `common/npps-core`, and every
platform reaches it** (JNI on Android, the C ABI on iOS and Windows, WebAssembly on the web). It is not
written again in Kotlin, Swift, C# or TypeScript, and not taken from a platform or third-party library.

- **Cryptographic means** hashing, signatures and their verification, key derivation, password hashing,
  key agreement, message authentication, AEAD sealing, and the generation of keys and nonces. The
  platform keeps only what only it can do: hold a key in its keystore or secure enclave, supply entropy,
  and move bytes. A non-security use (an RFC 4122 name-based UUID) is the one exception, and the code says so.
- **No JVM code runs on the helmet.** The hub firmware is C on the safety MCU and the main processor (§4.1,
  §4.2) and meets its real-time constraints there. Kotlin, Java and any other managed-runtime code is
  companion-app code on a phone, a computer or a browser, and is never moved onto the headset.
- **Library selection.** A declared dependency that nothing imports is removed. An alpha, beta, release
  candidate or snapshot is not used where a stable version exists. A copyleft licence is allowed only if
  it does not oblige NeurOne's own code to be released under a copyleft licence (the principal, 2026-10-17). A
  library whose licence would (GPL, AGPL, or any licence that reaches the code linked with it) is not added;
  a file-level or dynamically-linked one (MPL, LGPL used as the licence allows) is, with its obligations in the
  SOUP record (`docs/np_soup_kotlin_001.md` §2a).
- **Adding a Kotlin, Swift, C# or TypeScript implementation of a primitive** to a platform tree is a
  design decision, recorded as an open item first. Code already there when this section was written is
  listed in the SOUP record's open item (`OI-SOUPKT-01`) and moves to the core.
- **Every new crate** joins `docs/np_soup_rust_001.md` in the change that adds it.
