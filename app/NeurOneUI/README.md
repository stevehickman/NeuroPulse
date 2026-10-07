# NeurOne UI — one codebase for every platform

The app's screens and flow logic, written once in Compose Multiplatform and compiled for Android, iOS,
macOS, Windows and the web. The goal is that a behaviour difference between platforms can only come from
the few lines that talk to the operating system, never from a screen.

```
app/NeurOneUI/
  shared/    :shared   commonMain = every screen, the composition root (AppServices), the PlatformServices seam
  desktop/   :desktop  macOS + Windows (+ Linux) host: a window, file storage        (JVM)
  web/       :web      browser host: a canvas, localStorage                           (wasmJs)
  iosApp/              iOS host: a SwiftUI shell around one Kotlin view controller   (XcodeGen)
```

Android's host is `app/android/app` (an activity and the Android services); it depends on `:shared`. All five
modules are in the one Gradle build rooted at `app/android` (`settings.gradle.kts`), which is why the build
lives under `android/` while the code does not. `:core` (`app/android/core`) is the rules and stores, also
multiplatform, and `:shared` depends on it.

## Where a platform may differ

| What | Where | Why it cannot be common |
|------|-------|-------------------------|
| Storage, Bluetooth, runtime permissions, analytics vendor | `PlatformServices` (`shared/.../PlatformServices.kt`), one implementation per host | each is an operating-system API |
| Secure random, Ed25519 verify | `expect`/`actual` in `:core` (`life.neurone.core.platform`) | each is a platform crypto API |
| The NPPS core binding | `NppsBackend` in `:core` | the Rust core is reached by JNI, the C ABI or WebAssembly |
| Look and feel | Material 3 renders per platform | allowed by the brief; not a functional difference |

Anything else that differs between platforms is a defect. To add a platform capability, add a member to
`PlatformServices` only when no common implementation exists, and add its implementation to every host in the
same change, so no host can be left without it.

## Run it

```bash
cd app/android
gradle :desktop:run                              # desktop window (builds the NPPS native library first)
gradle :web:wasmJsBrowserDevelopmentRun          # browser
gradle :shared:desktopTest                       # the cross-platform flow, headless
gradle :core:jvmTest                             # the rules
gradle :app:assembleDebug                        # Android (SDK + NDK)
xcodegen generate --spec app/NeurOneUI/iosApp/project.yml   # iOS (Xcode)
```

## What is shared today, and what is not

**Shared, and identical on every platform:** the onboarding gates (age gate → biometric release → research
consent), the session-history list and detail, the consumables screen, the five-tab shell, the composition
root, the hub link (`BleCentral`, `NeurOneGattManager`), and every string.

**Still platform code.** These screens exist only in Android (`app/android/app/.../ui`) or in SwiftUI/React;
on a host with no override the tab shows "not available in this version yet" (`TabPending`) so the gap is
visible rather than silent. They move into `:shared` one at a time, deleting the platform copy as they go:

- Session and protocol menu (`SessionScreen`, `ProtocolMenuScreen`, caution dialog, cervical alerts)
- Privacy tab (`ConsentDashboardScreen`) and Settings (`SettingsScreen`, OTA, setup wizard, limits, profiles)
- Protocol editors (script, form, composer, modality) and the research-suggestion portal

**Not yet wired on every target** (open items in `docs/status/pending-decisions.md`):

- `OI-UI-KMP-01` — the shared NPPS core has a binding on the JVM only. iOS (`common/npps-ffi`) and the
  browser (the WebAssembly build) need an `NppsBackend`; until then `AppServices.protocolLibrary` throws on
  those targets, which is why the Session tab cannot move before it.
- `OI-UI-KMP-02` — the desktop app needs the NPPS native library packaged per OS (`.dylib`, `.dll`).
- `OI-UI-KMP-03` — Bluetooth: Android is wired; iOS (CoreBluetooth), the browser (Web Bluetooth) and desktop
  use `UnavailableBleCentral`, so the hub does not connect there.
- `OI-UI-KMP-04` — Ed25519 study-descriptor verification exists on the JVM only; other targets answer
  "cannot check", so every study descriptor is refused there (the shipped default everywhere, `OI-CONSENT-07`).
- `OI-UI-KMP-05` — the existing SwiftUI app (`app/ios`), React app (`app/web`) and the Windows project are
  untouched; retiring each is a decision for when `:shared` covers its screens. macOS is the desktop app.
- `OI-UI-KMP-06` — iOS and desktop packaging are unbuilt here (no Xcode, DMG/MSI
  tooling or browser); `android-ci.yml` and a macOS runner are the first real builds.
