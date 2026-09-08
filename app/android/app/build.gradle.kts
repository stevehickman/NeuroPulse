// :app — Android application shell. Compose UI, BLE, Keystore-backed storage.
// Requires the Android SDK; the privacy-critical logic it wires up lives in
// :core, which builds and tests on any JVM.

plugins {
    id("com.android.application") version "8.9.3"
    kotlin("android") version "2.0.21"
    kotlin("plugin.compose") version "2.0.21"
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

    buildFeatures { compose = true }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
}

dependencies {
    implementation(project(":core"))

    val composeBom = platform("androidx.compose:compose-bom:2024.12.01")
    implementation(composeBom)
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.activity:activity-compose:1.9.3")
    implementation("androidx.navigation:navigation-compose:2.8.5")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.7")

    // Keystore-backed storage for the opaque SHDR warranty token.
    implementation("androidx.security:security-crypto:1.1.0-alpha06")
    // BiometricPrompt with device-credential fallback (UHDR key credential).
    implementation("androidx.biometric:biometric:1.2.0-alpha05")

    // SHDR fleet upload with SPKI certificate pinning.
    implementation("com.squareup.okhttp3:okhttp:4.12.0")

    // Argon2id for UHDR key derivation (SOUP — NP-SW-001 table entry required
    // before beta; parallel of the iOS vendored PHC argon2).
    implementation("org.signal:argon2:13.1")

    testImplementation(kotlin("test"))
    testImplementation("junit:junit:4.13.2")
}
