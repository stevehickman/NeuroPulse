// NeurOne Android — Gradle multi-module project.
//
// :core — Kotlin Multiplatform, at common/core (every app uses it, so it is not under android/).
//         All privacy-critical logic (models, GATT parsing, consent, analytics gates,
//         session history, consumables, NPPS engine). Buildable and unit-testable without
//         the Android SDK.
// :app  — Android application shell (Compose UI, BLE, Keystore, uploads).
//         Requires the Android SDK; mirrors app/ios/NeurOne module layout.

pluginManagement {
    repositories {
        gradlePluginPortal()
        google()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "neurone-android"

include(":core")
project(":core").projectDir = file("../../common/core")
include(":app")

// Kotlin several apps use lives in common/ (CLAUDE.md section 20); :core depends on it.
include(":cloudstore")
project(":cloudstore").projectDir = file("../../common/cloudstore")

// The multiplatform UI lives beside the apps it serves, not under android/ (app/NeurOneUI/README.md).
include(":shared")
project(":shared").projectDir = file("../NeurOneUI/shared")

include(":desktop")
project(":desktop").projectDir = file("../NeurOneUI/desktop")
include(":web")
project(":web").projectDir = file("../NeurOneUI/web")
