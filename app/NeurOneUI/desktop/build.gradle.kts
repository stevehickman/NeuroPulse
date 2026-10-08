// :desktop — the macOS and Windows app (and Linux, for development). The UI is :shared, compiled for the
// JVM; this module is the host: a window, file-backed storage, and the PlatformServices that go with them.
//
// Run:   gradle :desktop:run
// Package: gradle :desktop:packageDmg (on macOS) · :desktop:packageMsi (on Windows) · :desktop:packageDeb

import org.jetbrains.compose.desktop.application.dsl.TargetFormat

plugins {
    kotlin("jvm")
    kotlin("plugin.compose")
    id("org.jetbrains.compose")
}

dependencies {
    implementation(project(":shared"))
    implementation(compose.desktop.currentOs)
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-swing:1.9.0")
    testImplementation(kotlin("test"))
}

// The shared NPPS core is a native library on the JVM (common/npps-jni). `run` builds the host copy and
// points the JVM at it; a packaged app must carry the library for its own OS (OI-UI-KMP-02).
val repoCommon: java.io.File = rootProject.projectDir.parentFile.parentFile.resolve("common")
val buildNppsCoreHost = project(":core").tasks.named("buildNppsCoreHost")

// The hub link is btleplug behind JNI (common/btle-jni, OI-UI-KMP-03). Without its library the app still
// starts, with the hub link unavailable; a packaged app must carry it per OS (OI-UI-KMP-02).
val buildBtleJni by tasks.registering(Exec::class) {
    group = "build"
    description = "Builds the desktop Bluetooth LE JNI library (common/btle-jni)."
    workingDir = repoCommon
    commandLine("cargo", "build", "--release", "--locked", "-p", "neurone-btle-jni")
    inputs.dir(File(repoCommon, "btle-jni")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.file(File(repoCommon, "Cargo.lock"))
    outputs.dir(File(repoCommon, "target/release"))
}

compose.desktop {
    application {
        mainClass = "life.neurone.desktop.MainKt"
        jvmArgs += "-Djava.library.path=${File(repoCommon, "target/release").absolutePath}"
        nativeDistributions {
            targetFormats(TargetFormat.Dmg, TargetFormat.Msi, TargetFormat.Deb)
            packageName = "NeurOne"
            packageVersion = "1.0.0"
            macOS {
                // btleplug reaches CoreBluetooth, which refuses the process without this key (OI-UI-KMP-03).
                infoPlist {
                    extraKeysRawXml = "<key>NSBluetoothAlwaysUsageDescription</key>" +
                        "<string>NeurOne uses Bluetooth to communicate with your NeurOne hub in real time.</string>"
                }
            }
        }
    }
}

tasks.matching { it.name == "run" }.configureEach { dependsOn(buildNppsCoreHost, buildBtleJni) }

tasks.test {
    useJUnitPlatform()
    dependsOn(buildBtleJni)
    systemProperty("java.library.path", File(repoCommon, "target/release").absolutePath)
}
