// :core — pure-JVM Kotlin module. NO Android plugin, NO android.* imports.
// This is the Android parallel of the NeurOneShared SPM package plus the
// platform-independent slices of app/ios/NeurOne. Everything here runs and
// tests on a plain JVM so privacy-critical logic is verifiable in CI without
// an Android SDK (ISC-2, ISC-3, ISC-4 in app/android/ISA.md).

plugins {
    kotlin("jvm") version "2.0.21"
    kotlin("plugin.serialization") version "2.0.21"
    `java-library`
}

java {
    toolchain { languageVersion.set(JavaLanguageVersion.of(17)) }
}

// ── Predefined protocol bundling ───────────────────────────────────────────
// The .npps library in protocols/predefined/ is the single source of truth for
// every runtime (web fetches it, iOS bundles it, this module packages it as a
// JVM resource). It is copied in at build time rather than transcribed into
// Kotlin: NPBundledProtocols.kt used to be a hand-maintained duplicate and had
// drifted — 8 of its 17 protocols were missing the conditions/references fields
// added in NP-NPPS-REF-001 Rev 2.
//
// JVM resources, not Android assets: :core is deliberately a pure-JVM module so
// privacy-critical logic tests without an Android SDK (ISC-2..4). Resources are
// readable by the classloader in both plain-JVM tests and the packaged APK.
val predefinedProtocolsDir = layout.projectDirectory.dir("../../../protocols/predefined")
val bundlePredefinedProtocols by tasks.registering(Sync::class) {
    description = "Copies protocols/predefined into :core resources."
    group = "build"
    from(predefinedProtocolsDir) { include("**/*.npps", "manifest.json") }
    into(layout.buildDirectory.dir("generated/npps/protocols/predefined"))
}

sourceSets.main {
    resources.srcDir(layout.buildDirectory.dir("generated/npps").map { it.asFile })
}

// Test data shared by every app lives in app/NeurOneShared/TestData, not under this module
// (hub-descriptor-golden.json is read by the Android, iOS and Windows tests).
sourceSets.test {
    resources.srcDir(layout.projectDirectory.dir("../../NeurOneShared/TestData"))
}

tasks.named("processResources") { dependsOn(bundlePredefinedProtocols) }

dependencies {
    // `api` (not `implementation`): `Json` is part of core's public API — it appears in the
    // default constructor arguments of SessionHistoryStore/ConsentStore — so consumers (:app)
    // need it on their compile classpath.
    api("org.jetbrains.kotlinx:kotlinx-serialization-json:1.7.3")

    testImplementation(kotlin("test"))
    testImplementation("org.junit.jupiter:junit-jupiter:5.10.2")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

// ── The shared NPPS core (OI-NPPS-CORE-01) ───────────────────────────────────
// common/npps-jni is the JNI binding of the Rust core. The tests that call it (NppsCoreTests)
// need the host build of it, so the test task builds it first and puts it on java.library.path.
// cargo must be on PATH; a missing toolchain fails the build rather than skipping the tests,
// because a skipped test is a parity check that silently stopped checking. Packaging the
// library for a device (cargo-ndk, four ABIs, :app jniLibs) is not wired yet and is the next
// step of the open item: it needs an NDK this module's CI does not have.
val repoCommon: java.io.File = layout.projectDirectory.dir("../../../common").asFile
val buildNppsCoreHost by tasks.registering(Exec::class) {
    group = "build"
    description = "Builds the host (JVM test) copy of the NPPS core's JNI library."
    workingDir = repoCommon
    commandLine("cargo", "build", "--release", "--locked", "-p", "neurone-npps-jni")
    inputs.dir(File(repoCommon, "npps-core")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.dir(File(repoCommon, "npps-jni")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.file(File(repoCommon, "npps/fields.json"))
    inputs.file(File(repoCommon, "Cargo.lock"))
    outputs.dir(File(repoCommon, "target/release"))
}

tasks.test {
    useJUnitPlatform()
    dependsOn(buildNppsCoreHost)
    systemProperty("java.library.path", File(repoCommon, "target/release").absolutePath)
}
