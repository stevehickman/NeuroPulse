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
// points the JVM at it; a packaged app carries it in its resources (stageNativeLibs, OI-UI-KMP-02).
val repoCommon: java.io.File = rootProject.projectDir.parentFile.parentFile.resolve("common")
val buildNppsCoreHost = project(":core").tasks.named("buildNppsCoreHost")

// The hub link is btleplug behind JNI (common/btle-jni, OI-UI-KMP-03). Without its library the app still
// starts, with the hub link unavailable; a packaged app carries it per OS (stageNativeLibs, OI-UI-KMP-02).
val buildBtleJni by tasks.registering(Exec::class) {
    group = "build"
    description = "Builds the desktop Bluetooth LE JNI library (common/btle-jni)."
    workingDir = repoCommon
    commandLine("cargo", "build", "--release", "--locked", "-p", "neurone-btle-jni")
    inputs.dir(File(repoCommon, "btle-jni")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.file(File(repoCommon, "Cargo.lock"))
    outputs.dir(File(repoCommon, "target/release"))
}

// Packaging (OI-UI-KMP-02). Compose copies `common/` and the folder named for the build OS and architecture
// from appResources.rootDir into the app, and points compose.application.resources.dir at it; NativeLibrary
// loads from there. Each installer is built on a runner of its own OS and architecture (CI: desktop-package),
// so the staged folder holds the host build only. stageNativeLibs fails the build if either library is missing.
val nativeStage: java.io.File = layout.buildDirectory.dir("native-resources").get().asFile
val hostOs: String = System.getProperty("os.name").lowercase()
val hostArch: String = System.getProperty("os.arch").lowercase().let { if (it == "aarch64" || it == "arm64") "arm64" else "x64" }
val resourceDirName: String = when {
    "mac" in hostOs -> "macos-$hostArch"
    "win" in hostOs -> "windows-$hostArch"
    else -> "linux-$hostArch"
}

val stageNativeLibs by tasks.registering(Copy::class) {
    group = "build"
    description = "Stages the NPPS and Bluetooth JNI libraries for $resourceDirName into the app resources."
    dependsOn(buildNppsCoreHost, buildBtleJni)
    from(File(repoCommon, "target/release")) {
        include("libneurone_npps_jni.*", "neurone_npps_jni.dll", "libneurone_btle_jni.*", "neurone_btle_jni.dll")
        exclude("*.d", "*.rlib", "*.a", "*.lib", "*.exp", "*.pdb")
    }
    into(File(nativeStage, resourceDirName))
    doLast {
        val staged = File(nativeStage, resourceDirName).listFiles().orEmpty().map { it.name }
        for (lib in listOf("neurone_npps_jni", "neurone_btle_jni")) {
            check(staged.any { it.contains(lib) }) { "$lib was not built for $resourceDirName; staged: $staged" }
        }
    }
}

compose.desktop {
    application {
        mainClass = "life.neurone.desktop.MainKt"
        jvmArgs += "-Djava.library.path=${File(repoCommon, "target/release").absolutePath}"
        nativeDistributions {
            appResourcesRootDir.set(nativeStage)
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
// Every packaging and bundling task reads the staged libraries.
tasks.matching { it.name.startsWith("package") || it.name.startsWith("prepareAppResources") || it.name.startsWith("createDistributable") }
    .configureEach { dependsOn(stageNativeLibs) }

tasks.test {
    useJUnitPlatform()
    dependsOn(buildBtleJni)
    systemProperty("java.library.path", File(repoCommon, "target/release").absolutePath)
}
