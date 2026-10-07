// :shared — the app's UI and composition root, written once in Compose Multiplatform and compiled for
// Android, iOS, the macOS/Windows desktop apps (JVM) and the web (wasmJs). Everything in commonMain runs
// the same on every platform; what differs is the PlatformServices a host hands in
// (app/NeurOneUI/README.md). Depends on :core for every rule and store.

import org.jetbrains.kotlin.gradle.ExperimentalWasmDsl
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    kotlin("multiplatform")
    kotlin("plugin.compose")
    id("org.jetbrains.compose")
    id("com.android.library")
}

// ── Localized strings (CLAUDE.md §17) ─────────────────────────────────────────
// Compose Multiplatform resources read the same values*/strings.xml the Android app does. They are generated
// from locales/*.json into the build directory (never checked in) and handed to the resource plugin as a
// custom directory, so there is still exactly one committed copy of every string.
val repoRoot: java.io.File = rootProject.projectDir.parentFile.parentFile
val generatedLocaleRes: Provider<Directory> = layout.buildDirectory.dir("generated/composeResources/locales")

val syncLocales by tasks.registering(Exec::class) {
    group = "localization"
    description = "Generate Compose Multiplatform values*/strings.xml from canonical locales/*.json"
    inputs.dir(File(repoRoot, "locales")).withPathSensitivity(PathSensitivity.RELATIVE)
    inputs.file(File(repoRoot, "scripts/sync-locales.ts")).withPathSensitivity(PathSensitivity.RELATIVE)
    outputs.dir(generatedLocaleRes)
    // The script only writes; a directory left by an earlier run (or an earlier qualifier scheme) must not survive it.
    doFirst { delete(generatedLocaleRes) }
    workingDir = repoRoot
    commandLine("bun", "scripts/sync-locales.ts", "--compose-res=${generatedLocaleRes.get().asFile.absolutePath}")
}

kotlin {
    androidTarget {
        compilerOptions { jvmTarget.set(JvmTarget.JVM_17) }
    }
    jvm("desktop") {
        compilerOptions { jvmTarget.set(JvmTarget.JVM_17) }
    }
    @OptIn(ExperimentalWasmDsl::class)
    wasmJs { browser() }
    listOf(iosArm64(), iosSimulatorArm64(), iosX64()).forEach {
        it.binaries.framework {
            baseName = "NeurOneShared"
            isStatic = true
        }
    }

    sourceSets {
        commonMain.dependencies {
            api(project(":core"))
            implementation(compose.runtime)
            implementation(compose.foundation)
            implementation(compose.ui)
            implementation(compose.material3)
            implementation(compose.components.resources)
            implementation("org.jetbrains.compose.material:material-icons-core:1.7.3")
            api("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.9.0")
        }
        commonTest.dependencies {
            implementation(kotlin("test"))
        }
        val desktopTest by getting {
            dependencies {
                // Headless UI tests of the shared screens run on the JVM; the same composables run on every target.
                @OptIn(org.jetbrains.compose.ExperimentalComposeLibrary::class)
                implementation(compose.uiTest)
                implementation(compose.desktop.currentOs)
            }
        }
        val desktopMain by getting {
            dependencies {
                implementation("org.jetbrains.kotlinx:kotlinx-coroutines-swing:1.9.0")
            }
        }
    }
}

tasks.withType<Test>().configureEach { useJUnitPlatform() }

compose.resources {
    packageOfResClass = "life.neurone.shared.resources"
    generateResClass = always
    customDirectory(sourceSetName = "commonMain", directoryProvider = generatedLocaleRes)
}

// Every task of the resource plugin that reads the generated directory waits for the locale sync (the
// plugin's task names change between versions, so match on what they do rather than on one name).
tasks.matching {
    it.name.startsWith("convertXmlValueResources") || it.name.startsWith("prepareComposeResources") ||
        it.name.startsWith("generateResourceAccessors") || it.name.startsWith("generateActualResourceCollectors") ||
        it.name.startsWith("generateExpectResourceCollectors") || it.name.startsWith("copy") && it.name.contains("ComposeResources") ||
        it.name.startsWith("assemble") && it.name.contains("Resources")
}.configureEach { dependsOn(syncLocales) }

android {
    namespace = "life.neurone.shared"
    compileSdk = 36
    defaultConfig { minSdk = 29 }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
