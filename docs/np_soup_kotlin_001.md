# Kotlin Libraries SOUP Inventory — Shared App (Android, Desktop, iOS, Web)

**Project:** NeurOne
**Document:** NP-SOUP-KOTLIN-001
**Revision:** 4
**Date:** 2026-10-17
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Software Engineering
**Approved By:** — (DRAFT, not approved)
**References:** `app/android/core/build.gradle.kts`, `app/android/app/build.gradle.kts`, `app/NeurOneUI/shared/build.gradle.kts`, `app/NeurOneUI/desktop/build.gradle.kts`, `app/NeurOneUI/web/build.gradle.kts` (the declared versions), `NP-SOUP-RUST-001` (the Rust record and the method precedent), `NP-SOUP-BTLE-001`, `NP-SW-001` §9.4 (the firmware SOUP register, which this record does not extend)
**Related Issues:** GitHub #569 (packages 6u, 6v), `OI-SOUPKT-01`
**Gate:** G2
**IEC 62304 Class:** Not assigned. See §3.
**Supersedes:** None
**Change Summary:** Rev 4 (2026-10-17) — **lock files, a scan job and the native-app record** (`OI-SOUPKT-01` items 2 to 4). Gradle dependency locking is on in lenient mode and `gradle.lockfile` is committed for `:core`, `:shared`, `:desktop` and `:web` (§4); `.github/workflows/dependency-scan.yml` regenerates them and fails on drift, and runs OSV-Scanner over them and `common/Cargo.lock` (§6.2). **That scan has not yet run:** the sandbox cannot reach OSV, so the first result is the first CI run on the branch, and §6.2 stays open until it is recorded. Dependency verification metadata (checksums) is not added, and §4 says why. The Swift and NuGet dependencies have a record, `NP-SOUP-NATIVE-001`. Rev 3 (2026-10-17) — **the Kotlin Ed25519 and SHA-2 are the Rust core's** (issue #569 package 6w, `OI-SOUPKT-01` item 1, `CLAUDE.md` §22). `Digests.sha256`/`sha512` and `Ed25519` (public key, sign, verify) call the core; the hand-written `Sha512.kt` and the TweetNaCl port in `Ed25519.kt` are gone, and so are the JDK signers (`JvmProtocolSigner`, `AndroidProtocolSigner`) and the per-target `ed25519Verify` actuals: every host signs with the common `Ed25519ProtocolSigner`, and a study descriptor's signature is now checked on iOS and in the browser too (it was refused there). Android no longer needs API 33 to sign. Only MD5 stays in Kotlin, for RFC 4122 version-3 UUIDs (not a security use). No library was added or removed, so the inventory (§5) is unchanged. The licence reading is extended (§5, end). Rev 2 (2026-10-17) — **the unused and pre-release libraries are gone, and the cryptography is the Rust core's** (issue #569, on the principal's direction: use no library that is not used, avoid pre-releases, prefer Rust to Kotlin for real-time or cryptographic work, and no JVM code on the helmet). Removed from `:app`: `org.signal:argon2` (GPLv3, unused), `androidx.security:security-crypto` 1.1.0-alpha06 and `androidx.biometric:biometric` 1.2.0-alpha05 (both pre-releases), and `navigation-compose`, `lifecycle-viewmodel-compose`, `lifecycle-runtime-compose`, `ui-tooling-preview` and `material3` (declared, imported nowhere). The SHDR device token is now sealed by the core's AES-256-GCM through the app's `EncryptedKeyValueStore`, with the data key wrapped by the Android Keystore, which replaces `security-crypto`; the UHDR key is derived by the core's `argon2` crate (`deriveUhdrKey`), which removes the `Argon2Provider` that no Android code ever implemented. `MainActivity` is a `ComponentActivity` (it was a `FragmentActivity` only for the biometric prompt, which was never built). The inventory is regenerated: **216** libraries, was 242, none a pre-release. §2a records the selection rules. A device token an earlier build stored in `EncryptedSharedPreferences` is not read: no released build is known, and a new token is made. Rev 1 (2026-10-17) (2026-10-17) — first issue. Written when `ktor-network` 3.0.3 joined the shared app (issue #569 package 6u), which made the absence of any record of the Kotlin libraries a gap: the Rust core (`NP-SOUP-RUST-001`) and the desktop Bluetooth crate (`NP-SOUP-BTLE-001`) had records and the Kotlin side, which is most of what ships, had none. The inventory (242 libraries) is generated from Gradle's resolved graphs for the four shipped targets. **Two findings, neither fixed here:** `org.signal:argon2` 13.1, declared by the Android app and shipped in it, is licensed GPLv3 by its own POM and no source file uses it; and no vulnerability scan could be run (§6.2). The rest of the anomaly evaluation is not done, and the record says so.
**Review Cadence:** On any change to a `build.gradle.kts` dependency or the Kotlin, Compose or Android Gradle plugin version, and at G2

---

## 1. What this record is, and is not

The shared app is Kotlin: `:core` (`app/android/core`, the stores and rules, compiled for the JVM, Android, iOS and the browser),
`:shared` (`app/NeurOneUI/shared`, the screens and composition root in Compose Multiplatform), `:app` (the Android host),
`:desktop` (the macOS and Windows host) and `:web` (the browser host). Everything those modules link that NeurOne did not write
is SOUP. This record **lists** that SOUP and records what has and has not been checked about it. It is the inventory half of the
work, plus the licence reading, which the inventory made possible.

It does not cover the Rust core (`NP-SOUP-RUST-001`), the desktop Bluetooth crate (`NP-SOUP-BTLE-001`), the firmware (`NP-SW-001`
§9.4) or the native apps (`NP-SOUP-NATIVE-001`: the Swift package dependencies and the NuGet references). Build
tools (Gradle 8.11.1, the Kotlin compiler 2.0.21, the Compose compiler plugin, the Android Gradle Plugin 8.9.3, Xcode) are not in
the shipped product and are not listed.

## 2. Direct dependencies

The version in the third column is what a `build.gradle.kts` declares; the resolved version of the same library can be higher (§5).

| Library | Module | Declared | Role |
|---------|--------|----------|------|
| kotlin-stdlib | all | 2.0.21 (the Kotlin plugin) | The language's runtime |
| org.jetbrains.kotlinx:kotlinx-serialization-json | `:core` (api) | 1.7.3 | Every stored record and every JSON document of the core's API is read and written through it (`ConsentStore`, `SessionHistoryStore`, the migration flow) |
| org.jetbrains.kotlinx:kotlinx-datetime | `:core` (api) | 0.6.1 | Dates and times of stored records, including the withdrawal times of `SWR-MG-04` |
| org.jetbrains.kotlinx:kotlinx-coroutines-core | `:core`, `:shared` | 1.9.0 | Every `suspend` call; the direct-link sessions' timeouts and step flows (`SWR-MG-10`) |
| org.jetbrains.kotlinx:kotlinx-coroutines-swing | `:shared` desktop, `:desktop` | 1.9.0 | The main dispatcher of the desktop apps |
| io.ktor:ktor-network | `:shared` (sockets source set: Android, desktop, iOS) | 3.0.3 | The TCP and UDP sockets of the direct link (`LanLinkTransport`, `SWR-MG-10`). Pulls `ktor-io`, `ktor-utils`, `kotlinx-io` |
| org.jetbrains.compose (runtime, foundation, ui, material3, components-resources) | `:shared`, `:web` | 1.7.3 | The screens, written once for every target; the string resources generated from `locales/` |
| org.jetbrains.compose.material:material-icons-core | `:shared` | 1.7.3 | The icons the screens draw |
| androidx.compose (BOM) with ui | `:app` | BOM 2024.12.01 | Compose's `LocalContext`, used by the host's file dialogs |
| androidx.activity:activity-compose | `:app` | 1.9.3 | The activity that hosts the shared screens |
| com.squareup.okhttp3:okhttp | `:app` | 4.12.0 | The SHDR fleet upload, with SPKI certificate pinning (the transport only; the token it sends is sealed by the Rust core) |
| Removed in Rev 2 | `:app` | — | `org.signal:argon2` 13.1 (GPLv3, unused), `androidx.security:security-crypto` 1.1.0-alpha06, `androidx.biometric:biometric` 1.2.0-alpha05, `androidx.navigation:navigation-compose`, `androidx.lifecycle:lifecycle-viewmodel-compose` and `lifecycle-runtime-compose`, `androidx.compose.ui:ui-tooling-preview`, `androidx.compose.material3:material3` |
| Test only: kotlin-test, kotlinx-coroutines-test 1.9.0, junit 4.13.2, junit-jupiter 5.10.2, compose.uiTest, compose.desktop.currentOs | tests | as stated | Not shipped |

## 2a. Selection rules

Set by the principal on 2026-10-17 and applied to every library in this record. They are a standing rule for adding or keeping one.

1. **A library that nothing uses is not declared.** A declared dependency that no source file imports is removed, with the comments that name it.
2. **No pre-release.** An alpha, beta, release-candidate or snapshot version is not used where a stable one exists. Where only a pre-release exists, the library is replaced rather than accepted, and the record says why if it cannot be.
3. **Real-time and cryptographic work is Rust.** Argon2, AES-GCM, SPAKE2, HMAC, HKDF and the sealing of stored values are in the shared core (`common/npps-core`), reached by every platform, and not in a Kotlin or platform library. The platform keeps only what only it can do: holding a key in its keystore, and moving bytes.
4. **No JVM code runs on the helmet.** The hub firmware is C on the safety MCU and the main processor (`CLAUDE.md` §4.1, §4.2) and meets its real-time constraints there. Everything in this record is companion-app code on a phone, a computer or a browser.
5. **A copyleft licence is allowed only if it does not force NeurOne's own code to be copyleft.** Set by the principal on 2026-10-17. A strong-copyleft licence (GPL, AGPL) or any licence whose terms reach the code linked with the library is not accepted. A file-level or link-exception licence (MPL, LGPL used as it allows, Classpath exception) is, and the library's row states the licence and what it obliges (source offer, relinking, notices).

What these rules leave in Kotlin, as of this revision: MD5 for version-3 UUIDs (`Digests.md5`, a non-security use), and the bytes-moving wrappers over the core (`Digests`, `Ed25519`). The Ed25519 and SHA-2 that predated rule 3 moved to the core in Rev 3. Still outside the rules: the Swift and NuGet code (`NSec.Cryptography` in the Windows app is a cryptographic library, `OI-SOUPKT-01`).

## 3. Class

Not assigned. The companion app is not a stimulation-controlling item: the hub's safety MCU owns every stimulation enable line
(`CLAUDE.md` §4.2), so a fault in any library here cannot enable stimulation. What a fault *can* do is: lose or corrupt a stored
record; accept an altered or substituted migration bundle (the bundle is sealed by the Rust core, so a Kotlin fault here is a
fault in what carries it); expose UHDR-class data (the keystore, the biometric prompt, the sockets); send SHDR to the wrong
party (the TLS stack); or draw a wrong screen. Which software safety class that earns the app, and so which of these libraries
need more than an inventory, is a classification decision this record does not make. `NP-SOUP-RUST-001` §3 argues Class B
for the same reasons; this record repeats the argument and not the conclusion.

## 4. Where the versions are pinned

They are not pinned as a set. Each `build.gradle.kts` declares the versions of its direct dependencies and Gradle resolves the
rest to the highest version any path asks for (the Compose BOM and the Ktor and coroutines BOMs align their families). Since Rev 4
**Gradle dependency locking is on** (`lockAllConfigurations`, lenient mode, in `app/android/build.gradle.kts`) and a
`gradle.lockfile` is committed for `:core`, `:shared`, `:desktop` and `:web`. Every version here is already fixed except those
the BOMs and the highest-wins rule choose, so the lock files pin the transitive set and make a change in it a diff. Lenient mode
means a library missing from a lock file does not fail a build; `dependency-scan.yml` is what fails on drift. **`:app` is not
locked** (resolving it needs the Android SDK, which only its CI job has); its libraries are the AndroidX and Compose set of §5.
**There is still no dependency verification metadata** (`gradle/verification-metadata.xml`), so a downloaded artifact is not
checked against a recorded checksum. It is not added because it must also cover `:app`'s configurations and the Kotlin, Compose and
Android plugin artifacts, cannot be generated here, and fails every build on any artifact it does not list; adding it blind would
break CI for no information. It is `OI-SOUPKT-01` item 3's remainder. §5 remains a record of one resolution, taken on the date
below, now with the lock files as its source.

## 5. Transitive inventory

Every library on the resolved runtime classpath of each shipped target, generated on 2026-10-17 from `gradle dependencies`:
`:app` `releaseRuntimeClasspath` (Android), `:shared` `desktopRuntimeClasspath` (macOS and Windows), `:shared`
`wasmJsRuntimeClasspath` (the browser) and `:shared` `iosArm64CompileKlibraries` (iOS; Kotlin/Native libraries are linked into the
framework). **216** libraries; a library that resolves to different versions on different targets lists both. Licences are the
`<license>` element of each library's own POM, read from the Gradle cache; they are **declared**, not checked against the licence
text. A library whose POM declares none says so.

| Library | Resolved version(s) | Targets | Licence (as declared) |
|---------|---------------------|---------|-----------------------|
| androidx.activity:activity | 1.9.3 | android | Apache-2.0 |
| androidx.activity:activity-compose | 1.9.3 | android | Apache-2.0 |
| androidx.activity:activity-ktx | 1.9.3 | android | Apache-2.0 |
| androidx.annotation:annotation | 1.8.0, 1.8.1 | android, desktop, ios | Apache-2.0 |
| androidx.annotation:annotation-experimental | 1.4.1 | android | Apache-2.0 |
| androidx.annotation:annotation-iosarm64 | 1.8.0 | ios | Apache-2.0 |
| androidx.annotation:annotation-jvm | 1.8.0, 1.8.1 | android, desktop | Apache-2.0 |
| androidx.arch.core:core-common | 2.2.0 | android, desktop | Apache-2.0 |
| androidx.arch.core:core-runtime | 2.2.0 | android | Apache-2.0 |
| androidx.autofill:autofill | 1.0.0 | android | Apache-2.0 |
| androidx.collection:collection | 1.4.0, 1.4.4 | android, desktop, ios | Apache-2.0 |
| androidx.collection:collection-iosarm64 | 1.4.0 | ios | Apache-2.0 |
| androidx.collection:collection-jvm | 1.4.0, 1.4.4 | android, desktop | Apache-2.0 |
| androidx.collection:collection-ktx | 1.4.4 | android | Apache-2.0 |
| androidx.compose:compose-bom | 2024.12.01 | android | Apache-2.0 |
| androidx.compose.animation:animation | 1.7.6 | android | Apache-2.0 |
| androidx.compose.animation:animation-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.animation:animation-core | 1.7.6 | android | Apache-2.0 |
| androidx.compose.animation:animation-core-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.foundation:foundation | 1.7.6 | android | Apache-2.0 |
| androidx.compose.foundation:foundation-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.foundation:foundation-layout | 1.7.6 | android | Apache-2.0 |
| androidx.compose.foundation:foundation-layout-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.material:material-icons-core | 1.7.6 | android | Apache-2.0 |
| androidx.compose.material:material-icons-core-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.material:material-ripple | 1.7.6 | android | Apache-2.0 |
| androidx.compose.material:material-ripple-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.material3:material3 | 1.3.1 | android | Apache-2.0 |
| androidx.compose.material3:material3-android | 1.3.1 | android | Apache-2.0 |
| androidx.compose.runtime:runtime | 1.7.6 | android | Apache-2.0 |
| androidx.compose.runtime:runtime-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.runtime:runtime-saveable | 1.7.6 | android | Apache-2.0 |
| androidx.compose.runtime:runtime-saveable-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-geometry | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-geometry-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-graphics | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-graphics-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-text | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-text-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-unit | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-unit-android | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-util | 1.7.6 | android | Apache-2.0 |
| androidx.compose.ui:ui-util-android | 1.7.6 | android | Apache-2.0 |
| androidx.concurrent:concurrent-futures | 1.1.0 | android | Apache-2.0 |
| androidx.core:core | 1.13.1 | android | Apache-2.0 |
| androidx.core:core-ktx | 1.13.1 | android | Apache-2.0 |
| androidx.customview:customview-poolingcontainer | 1.0.0 | android | Apache-2.0 |
| androidx.emoji2:emoji2 | 1.3.0 | android | Apache-2.0 |
| androidx.graphics:graphics-path | 1.0.1 | android | Apache-2.0 |
| androidx.interpolator:interpolator | 1.0.0 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-common | 2.8.5 | android, desktop, ios | Apache-2.0 |
| androidx.lifecycle:lifecycle-common-iosarm64 | 2.8.5 | ios | Apache-2.0 |
| androidx.lifecycle:lifecycle-common-java8 | 2.8.5 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-common-jvm | 2.8.5 | android, desktop | Apache-2.0 |
| androidx.lifecycle:lifecycle-livedata-core | 2.8.5 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-process | 2.8.5 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-runtime | 2.8.5 | android, desktop, ios | Apache-2.0 |
| androidx.lifecycle:lifecycle-runtime-android | 2.8.5 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-runtime-compose | 2.8.5 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-runtime-compose-android | 2.8.5 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-runtime-desktop | 2.8.5 | desktop | Apache-2.0 |
| androidx.lifecycle:lifecycle-runtime-iosarm64 | 2.8.5 | ios | Apache-2.0 |
| androidx.lifecycle:lifecycle-runtime-ktx | 2.8.5 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-runtime-ktx-android | 2.8.5 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-viewmodel | 2.8.5 | android, desktop, ios | Apache-2.0 |
| androidx.lifecycle:lifecycle-viewmodel-android | 2.8.5 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-viewmodel-desktop | 2.8.5 | desktop | Apache-2.0 |
| androidx.lifecycle:lifecycle-viewmodel-iosarm64 | 2.8.5 | ios | Apache-2.0 |
| androidx.lifecycle:lifecycle-viewmodel-ktx | 2.8.5 | android | Apache-2.0 |
| androidx.lifecycle:lifecycle-viewmodel-savedstate | 2.8.5 | android | Apache-2.0 |
| androidx.profileinstaller:profileinstaller | 1.3.1 | android | Apache-2.0 |
| androidx.savedstate:savedstate | 1.2.1 | android | Apache-2.0 |
| androidx.savedstate:savedstate-ktx | 1.2.1 | android | Apache-2.0 |
| androidx.startup:startup-runtime | 1.1.1 | android | Apache-2.0 |
| androidx.tracing:tracing | 1.0.0 | android | Apache-2.0 |
| androidx.versionedparcelable:versionedparcelable | 1.1.1 | android | Apache-2.0 |
| com.google.guava:listenablefuture | 1.0 | android | none declared in the cached POM |
| com.squareup.okhttp3:okhttp | 4.12.0 | android | Apache-2.0 |
| com.squareup.okio:okio | 3.6.0 | android | Apache-2.0 |
| com.squareup.okio:okio-jvm | 3.6.0 | android | Apache-2.0 |
| io.ktor:ktor-io | 3.0.3 | android, desktop, ios | Apache-2.0 |
| io.ktor:ktor-io-iosarm64 | 3.0.3 | ios | Apache-2.0 |
| io.ktor:ktor-io-jvm | 3.0.3 | android, desktop | Apache-2.0 |
| io.ktor:ktor-network | 3.0.3 | android, desktop, ios | Apache-2.0 |
| io.ktor:ktor-network-iosarm64 | 3.0.3 | ios | Apache-2.0 |
| io.ktor:ktor-network-jvm | 3.0.3 | android, desktop | Apache-2.0 |
| io.ktor:ktor-utils | 3.0.3 | android, desktop, ios | Apache-2.0 |
| io.ktor:ktor-utils-iosarm64 | 3.0.3 | ios | Apache-2.0 |
| io.ktor:ktor-utils-jvm | 3.0.3 | android, desktop | Apache-2.0 |
| org.jetbrains:annotations | 23.0.0 | android, desktop | Apache-2.0 |
| org.jetbrains.androidx.lifecycle:lifecycle-common | 2.8.4 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.androidx.lifecycle:lifecycle-common-wasm-js | 2.8.4 | web | Apache-2.0 |
| org.jetbrains.androidx.lifecycle:lifecycle-runtime | 2.8.4 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.androidx.lifecycle:lifecycle-runtime-compose | 2.8.4 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.androidx.lifecycle:lifecycle-runtime-compose-desktop | 2.8.4 | desktop | Apache-2.0 |
| org.jetbrains.androidx.lifecycle:lifecycle-runtime-compose-uikitarm64 | 2.8.4 | ios | Apache-2.0 |
| org.jetbrains.androidx.lifecycle:lifecycle-runtime-compose-wasm-js | 2.8.4 | web | Apache-2.0 |
| org.jetbrains.androidx.lifecycle:lifecycle-runtime-wasm-js | 2.8.4 | web | Apache-2.0 |
| org.jetbrains.androidx.lifecycle:lifecycle-viewmodel | 2.8.4 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.androidx.lifecycle:lifecycle-viewmodel-wasm-js | 2.8.4 | web | Apache-2.0 |
| org.jetbrains.compose.animation:animation | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.animation:animation-core | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.animation:animation-core-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.animation:animation-core-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.animation:animation-core-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.animation:animation-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.animation:animation-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.animation:animation-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.annotation-internal:annotation | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.annotation-internal:annotation-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.collection-internal:collection | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.collection-internal:collection-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.components:components-resources | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.components:components-resources-android | 1.7.3 | android | Apache-2.0 |
| org.jetbrains.compose.components:components-resources-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.components:components-resources-iosArm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.components:components-resources-wasmJs | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.foundation:foundation | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.foundation:foundation-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.foundation:foundation-layout | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.foundation:foundation-layout-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.foundation:foundation-layout-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.foundation:foundation-layout-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.foundation:foundation-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.foundation:foundation-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.material:material-icons-core | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.material:material-icons-core-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.material:material-icons-core-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.material:material-icons-core-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.material:material-ripple | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.material:material-ripple-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.material:material-ripple-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.material:material-ripple-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.material3:material3 | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.material3:material3-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.material3:material3-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.material3:material3-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.runtime:runtime | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.runtime:runtime-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.runtime:runtime-saveable | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.runtime:runtime-saveable-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.runtime:runtime-saveable-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.runtime:runtime-saveable-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.runtime:runtime-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.runtime:runtime-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.ui:ui | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.ui:ui-geometry | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-geometry-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.ui:ui-geometry-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.ui:ui-geometry-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-graphics | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-graphics-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.ui:ui-graphics-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.ui:ui-graphics-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-text | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-text-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.ui:ui-text-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.ui:ui-text-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-uikit | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.ui:ui-uikit-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.ui:ui-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.ui:ui-unit | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-unit-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.ui:ui-unit-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.ui:ui-unit-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-util | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-util-desktop | 1.7.3 | desktop | Apache-2.0 |
| org.jetbrains.compose.ui:ui-util-uikitarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.compose.ui:ui-util-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.compose.ui:ui-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.kotlin:kotlin-stdlib | 2.0.21 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.kotlin:kotlin-stdlib-common | 2.0.21 | android | Apache-2.0 |
| org.jetbrains.kotlin:kotlin-stdlib-jdk7 | 1.9.10, 1.9.24 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlin:kotlin-stdlib-jdk8 | 1.9.10, 1.9.24 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlin:kotlin-stdlib-wasm-js | 2.0.21 | web | Apache-2.0 |
| org.jetbrains.kotlinx:atomicfu | 0.23.2, 0.25.0, 0.26.1 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.kotlinx:atomicfu-iosarm64 | 0.26.1 | ios | Apache-2.0 |
| org.jetbrains.kotlinx:atomicfu-jvm | 0.23.2 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlinx:atomicfu-wasm-js | 0.25.0 | web | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-browser | 0.1 | web | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-browser-wasm-js | 0.1 | web | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-coroutines-android | 1.9.0 | android | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-coroutines-bom | 1.9.0 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-coroutines-core | 1.9.0 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-coroutines-core-iosarm64 | 1.9.0 | ios | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-coroutines-core-jvm | 1.9.0 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-coroutines-core-wasm-js | 1.9.0 | web | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-coroutines-swing | 1.9.0 | desktop | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-datetime | 0.6.1 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-datetime-iosarm64 | 0.6.1 | ios | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-datetime-jvm | 0.6.1 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-datetime-wasm-js | 0.6.1 | web | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-io-bytestring | 0.5.4 | android, desktop, ios | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-io-bytestring-iosarm64 | 0.5.4 | ios | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-io-bytestring-jvm | 0.5.4 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-io-core | 0.5.4 | android, desktop, ios | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-io-core-iosarm64 | 0.5.4 | ios | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-io-core-jvm | 0.5.4 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-serialization-bom | 1.7.3 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-serialization-core | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-serialization-core-iosarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-serialization-core-jvm | 1.7.3 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-serialization-core-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-serialization-json | 1.7.3 | android, desktop, ios, web | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-serialization-json-iosarm64 | 1.7.3 | ios | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-serialization-json-jvm | 1.7.3 | android, desktop | Apache-2.0 |
| org.jetbrains.kotlinx:kotlinx-serialization-json-wasm-js | 1.7.3 | web | Apache-2.0 |
| org.jetbrains.skiko:skiko | 0.8.18 | desktop, ios, web | Apache-2.0 |
| org.jetbrains.skiko:skiko-awt | 0.8.18 | desktop | Apache-2.0 |
| org.jetbrains.skiko:skiko-iosarm64 | 0.8.18 | ios | Apache-2.0 |
| org.jetbrains.skiko:skiko-js-wasm-runtime | 0.8.18 | web | Apache-2.0 |
| org.jetbrains.skiko:skiko-wasm-js | 0.8.18 | web | Apache-2.0 |
| org.slf4j:slf4j-api | 2.0.16 | android, desktop | none declared in the cached POM |

**The licences.** Every library whose POM declares a licence declares Apache-2.0. (Rev 1 found one that did not, `org.signal:argon2`,
GPLv3; it is removed, §6.3.) `com.google.guava:listenablefuture` and `org.slf4j:slf4j-api` declare none in the cached POM
(`listenablefuture` is an empty marker artifact; SLF4J is MIT-licensed upstream, which was not checked against the artifact).
The Skia-based `skiko` native binaries and the Android platform libraries carry their own notices, which this record has not
collected: the licence inventory for the app bundles (also owed by `NP-SOUP-RUST-001` §5 for `unicode-ident`) has to carry them.

**Checked against §2a rule 5 (2026-10-17).** None of the 216 libraries declares a copyleft licence: 214 declare Apache-2.0 and two declare none (above). The check is of the declared identifiers only. The Rust core's 154 crates (`cargo metadata --locked`) declare MIT, Apache-2.0, BSD-3-Clause, Unlicense, Zlib or combinations of them, and the four with none are NeurOne's own workspace crates. No library in either inventory is GPL, AGPL, LGPL or MPL. Outside this inventory the Windows app declares `NSec.Cryptography` 24.10.0 (a cryptographic library, which §22 moves to the core), and the Swift and NuGet dependencies have no SOUP record yet (`OI-SOUPKT-01`).

## 6. Anomaly evaluation

IEC 62304 asks for the published anomaly list of each SOUP version in use to be evaluated for anomalies that could lead to a
hazardous situation. **None of that was done for these libraries.** What was done is below, and §6.2 says what was not.

### 6.1 What was done

The inventory (§5) and a reading of each library's declared licence. Nothing else.

### 6.2 What was not done

- **No vulnerability scan result yet.** The sandbox this record was written in cannot reach the OSV or GitHub advisory services, so no
  library was checked here. Rev 4 adds the scan (`.github/workflows/dependency-scan.yml`: OSV-Scanner v1.9.2 over the `gradle.lockfile`
  files and `common/Cargo.lock`, on a dependency change and weekly). **Until its first run is read and its date, database and
  result are written in this section, the absence of a finding is not the absence of an advisory** (`OI-SOUPKT-01`). `:app` and the
  libraries it alone brings are outside the scan (no lock file, §4). The Rust core was scanned with `cargo audit`
  (`NP-SOUP-RUST-001` §6.1), which is a different database.
- **No reading of any library's changelog or issue tracker** for the resolved version.
- **No pre-release remains** in the resolved set of any target (checked for alpha, beta, rc, snapshot, dev and milestone
  markers on 2026-10-17). Rev 1 had two direct ones, now removed (§2a).
- **No known-answer or behaviour test of a library beyond what the app's own tests exercise.** `ktor-network` is exercised by
  real-socket tests on the loopback address (`LanLinkTransportTest`); `kotlinx-serialization-json` by every store test;
  `okhttp` by none on a device. The sealing of the device token is the Rust core's and is tested there and through `EncryptedKeyValueStore`.
- **No run on a device** of the Android keystore wrapping or the pinned-TLS path, and no run of the iOS socket code (`OI-SOUPKT-01`). The `:app` module itself was not compiled in the sandbox (no Android SDK); CI does.
- **No check of the licence texts** against the declared identifiers, and no collection of the notices the Skia binaries carry.

### 6.3 A licence finding, resolved: org.signal:argon2

Rev 1 found that the Android app declared `org.signal:argon2` 13.1, whose own POM declares **GPLv3**, that it was on the release
runtime classpath, and that no source file used it: the only mentions were comments, and the `Argon2Provider` that
`UhdrKeyDerivation` asked for had no Android implementation. **Rev 2 removes the dependency and the comments that named it**, and
removes `Argon2Provider` with it: the UHDR key is derived by the shared core's `argon2` crate (MIT OR Apache-2.0, already linked for
the bundle key; `deriveUhdrKey`), which is also what the selection rules ask for (§2a, rule 3). The Swift app keeps its vendored
PHC argon2 until it is moved too (`NP-SOUP-NATIVE-001` §3). Nothing the Android build shipped used the removed library, so no behaviour changes.

## 7. Hazard reading

Only the libraries whose fault would reach UHDR-class data, SHDR or the stored records are read. The others (Compose, Material,
Skia, the icon set) can draw a wrong screen and no more.

| Library | Fault | Effect | Held by |
|---------|-------|--------|---------|
| kotlinx-serialization-json | Mis-parses or silently drops a field of a stored record | A consent withdrawal, a grant or a protocol is read back differently from how it was written | The store tests round-trip each record; the consent store treats an unreadable record as absent and a withdrawal time is only ever added (`SWR-MG-04`) |
| kotlinx-datetime | A wrong date or instant | A withdrawal is stamped with a wrong time, or a code's expiry is misjudged | The withdrawal times and the code's clock are tested with an injected clock; the core compares them as text |
| ktor-network | Delivers bytes to the wrong peer, drops or reorders them, or reads past a bound | A phone receives another phone's frames, or a frame is lost mid-bundle | Nothing is trusted from it: the core proves the peer holds the code, seals every frame and refuses a replayed, altered or reordered one; the loopback tests carry 300 kB both ways |
| okhttp | Accepts a certificate the pin should refuse | SHDR is uploaded to a party that is not NeurOne | The pin is the app's configuration, not the library's default; no test against a wrong certificate on a device (§6.2) |
| (none: the cryptography is the Rust core's) | A fault in AES-256-GCM sealing, Argon2id, SPAKE2, HMAC or HKDF | A stored value or a link is readable or alterable by another party | `NP-SOUP-RUST-001` §7 and the core's tests; the platform contributes only the Android Keystore wrapping of the data key (`AndroidSecretKeyStore`), which the app proves can keep, return and remove a key before it relies on it (`SWR-MG-06`). No device test (§6.2) |
| the rest (§5) | A fault in a support library | As the library that depends on it | Nothing; no advisory scan was run (§6.2) |

## 8. Keeping this record true

- A change to a dependency declaration or a plugin version regenerates the lock files (`gradle :core:resolveAndLockAll :shared:resolveAndLockAll :desktop:resolveAndLockAll :web:resolveAndLockAll --write-locks`, from `app/android`), §5 and the scan of §6.2 in the same change.
  Regenerate §5 with `gradle :app:dependencies --configuration releaseRuntimeClasspath` and `:shared:dependencies` for
  `desktopRuntimeClasspath`, `wasmJsRuntimeClasspath` and `iosArm64CompileKlibraries`, from `app/android`.
- Adding a direct dependency checks it against §2a first, and adds a row to §2 and, if its fault could reach UHDR-class data, SHDR or a stored record, a row to §7.
- This record does not extend `NP-SW-001` §9.4, which is the firmware register for SW-01 and SW-02, as `NP-SOUP-RUST-001` §8
  says of itself. If the app SOUP is to be registered in the software development plan, that is a change to the plan.
- The native apps (Swift package, NuGet) are `NP-SOUP-NATIVE-001`.
