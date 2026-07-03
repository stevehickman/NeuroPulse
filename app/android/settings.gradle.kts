// NeuroPulse Android — Gradle multi-module project.
//
// :core — pure-JVM Kotlin. All privacy-critical logic (models, GATT parsing,
//         consent, analytics gates, session history, consumables, NPPS engine).
//         Buildable and unit-testable without the Android SDK.
// :app  — Android application shell (Compose UI, BLE, Keystore, uploads).
//         Requires the Android SDK; mirrors app/ios/NeuroPulse module layout.

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

rootProject.name = "neuropulse-android"

include(":core")
include(":app")
