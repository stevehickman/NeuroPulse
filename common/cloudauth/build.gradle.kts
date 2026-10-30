// :cloudauth — Kotlin Multiplatform module in common/ (CLAUDE.md §20). Sign-in and credential storage for the cloud object
// stores of :cloudstore (SWR-MG-11, GitHub #566, docs/np_sw_cloudstore_001.md §4c). It obtains the person's own OAuth tokens for
// Google and Microsoft, holds the Apple web-authentication token for CloudKit and the AWS keys the person hands over, keeps them
// sealed through :core's protected key-value store, refreshes them, and hands the adapters an `AccessToken`,
// `AwsCredentialsProvider` or `CloudKitCredentials`. NeurOne holds no credential of its own and no client secret.
//
// It sits above :core and :cloudstore (common/ imports nothing outside common/): :core depends on :cloudstore, this module on both,
// and neither depends on it. Same targets as :cloudstore, no Android plugin and no android.* imports, so jvmTest runs on a plain JVM.
// The hashing and the random bytes the PKCE exchange needs are the Rust core's (CLAUDE.md §22), reached through :core, so the JVM
// tests build the host copy of the core's JNI library first, as :core's own tests do.

import org.jetbrains.kotlin.gradle.ExperimentalWasmDsl
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    kotlin("multiplatform")
    kotlin("plugin.serialization")
}

kotlin {
    jvm {
        compilerOptions { jvmTarget.set(JvmTarget.JVM_17) }
    }
    @OptIn(ExperimentalWasmDsl::class)
    wasmJs {
        browser()
        nodejs()
    }
    iosArm64()
    iosSimulatorArm64()

    sourceSets {
        commonMain {
            dependencies {
                api(project(":cloudstore"))
                api(project(":core"))
                api("org.jetbrains.kotlinx:kotlinx-serialization-json:1.7.3")
                implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.9.0")
            }
        }
        commonTest {
            dependencies {
                implementation(kotlin("test"))
            }
        }
        jvmTest {
            dependencies {
                implementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.9.0")
                implementation("org.junit.jupiter:junit-jupiter:5.10.2")
                runtimeOnly("org.junit.platform:junit-platform-launcher")
                // The shared object-store contract (:cloudstore's tests), for the opt-in run of the S3 adapter against a live server (LiveS3ContractTests).
                implementation(project(path = ":cloudstore", configuration = "testClasses"))
            }
        }
    }
}

val repoCommon: java.io.File = layout.projectDirectory.dir("..").asFile

tasks.named<Test>("jvmTest") {
    useJUnitPlatform()
    dependsOn(":core:buildNppsCoreHost")
    systemProperty("java.library.path", File(repoCommon, "target/release").absolutePath)
}
