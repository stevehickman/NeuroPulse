// :app — Android application shell. Compose UI, BLE, Keystore-backed storage.
// Requires the Android SDK; the privacy-critical logic it wires up lives in
// :core, which builds and tests on any JVM.

plugins {
    id("com.android.application")
    kotlin("android")
    kotlin("plugin.compose")
}

// ── Localized strings (CLAUDE.md §17) ─────────────────────────────────────────
//
// res/values*/strings.xml is generated from locales/*.json, the one committed
// copy of every user-facing string, and is NOT checked in. It lands in the
// module build directory and is added as a res srcDir, so the Android source
// tree never holds a generated file at all — there is nothing to hand-edit and
// nothing to leave stale across a branch switch.
//
// The generator is a Bun script shared with the web and iOS builds; bun must be
// on PATH (the repository is bun-only — see the root .gitignore note on the
// absent npm lockfile). CI installs it via oven-sh/setup-bun.
val repoRoot: java.io.File = rootProject.projectDir.parentFile.parentFile
val generatedLocaleRes: Provider<Directory> = layout.buildDirectory.dir("generated/res/locales")

val syncLocales by tasks.registering(Exec::class) {
    group = "localization"
    description = "Generate res/values*/strings.xml from canonical locales/*.json"

    // Declared so Gradle can skip the task when nothing it reads has changed;
    // without these it re-runs on every build and invalidates resource merging
    // each time.
    inputs.dir(File(repoRoot, "locales")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.file(File(repoRoot, "scripts/sync-locales.ts"))
        .withPathSensitivity(PathSensitivity.RELATIVE)
    outputs.dir(generatedLocaleRes)

    workingDir = repoRoot
    commandLine(
        "bun",
        "scripts/sync-locales.ts",
        "--android-res=${generatedLocaleRes.get().asFile.absolutePath}",
    )
}

// ── The shared NPPS core, packaged (OI-NPPS-CORE-01) ──────────────────────────
//
// :core's HubDescriptorCompiler calls the Rust core through JNI (NppsCore), so the APK must carry
// libneurone_npps_jni.so for every ABI it installs on; without it the first descriptor compile
// throws UnsatisfiedLinkError and no protocol uploads. cargo-ndk builds the four ABIs into a
// directory that is added as a jniLibs source. Needs `cargo-ndk`, the four Rust Android targets
// and an NDK (ANDROID_NDK_HOME); android-ci.yml installs all three and checks the APK for the
// library. `-PskipNppsCoreNative` skips it for a build that never runs a compile (a UI-only
// iteration); such an APK cannot upload a protocol.
val nppsCoreJniLibs: Provider<Directory> = layout.buildDirectory.dir("generated/jniLibs/npps")

val buildNppsCoreNative by tasks.registering(Exec::class) {
    group = "build"
    description = "Builds libneurone_npps_jni.so for arm64-v8a, armeabi-v7a, x86_64 and x86 with cargo-ndk."
    val common = File(repoRoot, "common")
    workingDir = common
    commandLine(
        "cargo", "ndk",
        "-t", "arm64-v8a", "-t", "armeabi-v7a", "-t", "x86_64", "-t", "x86",
        "-o", nppsCoreJniLibs.get().asFile.absolutePath,
        "build", "--release", "--locked", "-p", "neurone-npps-jni",
    )
    inputs.dir(File(common, "npps-core")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.dir(File(common, "npps-jni")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.file(File(common, "npps/fields.json"))
    inputs.file(File(common, "Cargo.lock"))
    outputs.dir(nppsCoreJniLibs)
}

if (!project.hasProperty("skipNppsCoreNative")) {
    // Every task that merges or packages native libraries, for every variant.
    tasks.matching { it.name.matches(Regex("merge.*JniLibFolders")) }
        .configureEach { dependsOn(buildNppsCoreNative) }
}

// ── Release builds refuse the placeholder fleet certificate pins (privacy review Finding 9, REQ-SHDR-04) ──
//
// The pins in ShdrUploader are placeholders until the real fleet certificate exists. They fail closed, so a release built with
// them looks fine and uploads no SHDR record, ever. This stops assembleRelease and bundleRelease instead. Debug builds and the
// unit-test variants are untouched, so CI stays green while the placeholders are in the tree.
val checkReleaseFleetPins by tasks.registering(Exec::class) {
    group = "verification"
    description = "Fails if a fleet certificate pin is a placeholder (scripts/check-release-pins.ts --release)."
    workingDir = repoRoot
    commandLine("bun", "scripts/check-release-pins.ts", "--release")
}
tasks.matching { it.name == "assembleRelease" || it.name == "bundleRelease" }
    .configureEach { dependsOn(checkReleaseFleetPins) }

// preBuild is an ancestor of every variant task, resource merging included, so
// this single edge covers debug, release and the unit-test variants alike.
tasks.named("preBuild") { dependsOn(syncLocales) }

android {
    namespace = "life.neurone.app"
    // compileSdk tracks the newest installed SDK platform on the build machine (36).
    // targetSdk stays at 35 (runtime behavior contract); compileSdk ≥ targetSdk is standard.
    compileSdk = 36

    defaultConfig {
        // Production application ID — not a placeholder (parity with iOS ISC-7).
        applicationId = "life.neurone.app"
        minSdk = 29
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
        }
    }

    sourceSets["main"].res.srcDir(generatedLocaleRes)
    sourceSets["main"].jniLibs.srcDir(nppsCoreJniLibs)

    buildFeatures { compose = true }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
}

dependencies {
    implementation(project(":core"))
    implementation(project(":shared"))

    // Only what this module's own code imports. The screens are in :shared, which brings Compose; the cryptography is the
    // shared Rust core's (the SHDR token is sealed through it), so no crypto, biometric or Argon2 library is declared here.
    val composeBom = platform("androidx.compose:compose-bom:2024.12.01")
    implementation(composeBom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.activity:activity-compose:1.9.3")

    // SHDR fleet upload with SPKI certificate pinning.
    implementation("com.squareup.okhttp3:okhttp:4.12.0")

    testImplementation(kotlin("test"))
    testImplementation("junit:junit:4.13.2")
}
