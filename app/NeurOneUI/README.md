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
gradle :core:wasmJsNodeTest                      # the NPPS core through the WebAssembly binding, under Node
gradle :core:jvmTest                             # the rules
gradle :app:assembleDebug                        # Android (SDK + NDK)
xcodegen generate --spec app/NeurOneUI/iosApp/project.yml   # iOS (Xcode)
```

## What is shared today, and what is not

**Every screen is shared, and identical on every platform:** the onboarding gates (age gate → biometric release →
research consent), the five tabs (Session with the protocol menu and editors, History, Consumables, Privacy with the
consent dashboard and research portal, Settings with firmware, device setup, profiles and dosage limits), the
composition root, the hub link (`BleCentral`, `NeurOneGattManager`), and every string. There is no per-host
override: a host differs only through `PlatformServices` and the `expect`/`actual` set in `:core`. The Android
activity, the desktop window, the browser page and the iOS view controller each only host `NeurOneApp`.

What is left is not screens but wiring on some targets, below, and retiring the old platform UIs (`OI-UI-KMP-05`).

**Not yet wired on every target** (open items in `docs/status/pending-decisions.md`):

- `OI-UI-KMP-01` — the NPPS core binding. Done for the browser (WebAssembly, loaded by `:web` before it
  composes; parity-tested under Node; the protocol menu lists the library in a real browser) and written for Apple
  (cinterop over `common/npps-ffi`; its parity run needs macOS). The validator's locale keys resolve through
  `installValidationText` on every host.
- `OI-UI-KMP-07` — protocol signing. Every host signs: Android and desktop with the JDK's Ed25519, iOS and the browser
  with the common `Ed25519ProtocolSigner` (pinned to RFC 8032 and, on the JVM, to the JDK). All keys are ephemeral
  per process (`OI-AND-SIGN-01`); persisting one is open.
- `OI-UI-KMP-08` — plurals. Compose Multiplatform 1.7 has no plural resources, so `sync-locales --compose-res`
  writes each family as flat `_one` / `_other` strings and `pluralString` picks one. That is right while every
  locale's text is English; real Arabic or Russian plural forms need a category rule there.
- `OI-UI-KMP-02` — the desktop app needs the NPPS native library packaged per OS (`.dylib`, `.dll`).
- `OI-UI-KMP-03` — Bluetooth. Android is wired. The browser uses Web Bluetooth (`WebBleCentral`, verified against a
  mocked `navigator.bluetooth`, not a real hub) and iOS uses CoreBluetooth (`IosBleCentral`, unbuilt). **Desktop
  deliberately has no Bluetooth yet** and keeps `UnavailableBleCentral`; the options are recorded in the open item.
- `OI-UI-KMP-04` — Ed25519 study-descriptor verification exists on the JVM only; other targets answer
  "cannot check", so every study descriptor is refused there (the shipped default everywhere, `OI-CONSENT-07`).
- `OI-UI-KMP-05` — the existing SwiftUI app (`app/ios`), React app (`app/web`) and the Windows project are
  untouched, and `:shared` now covers every screen they have; retiring each waits on its production path (BLE,
  storage, signing) being wired in the shared build. macOS is the desktop app.
- `OI-UI-KMP-06` — iOS and desktop packaging are unbuilt here (no Xcode, DMG/MSI
  tooling or browser); `android-ci.yml` and a macOS runner are the first real builds.
