// :cloudstore — Kotlin Multiplatform module in common/ (CLAUDE.md §20). The cloud object-store seam and its thin
// provider adapters (SWR-MG-11, GitHub #566, docs/np_sw_cloudstore_001.md §4b). Code several apps use lives in
// common/, not in one app's tree: the Android app, the shared Compose app (desktop, iOS, web) and any later host
// reach it through :core, which depends on this module (not the other way round: common/ imports nothing outside common/).
//
// Same targets as :core, no Android plugin and no android.* imports, so the jvmTest task runs on a plain JVM.
// It holds the three interfaces the adapters need (HttpTransport with HttpRequest and HttpResponse, and ObjectStore)
// under their original package names, so no caller changed.

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
                api("org.jetbrains.kotlinx:kotlinx-serialization-json:1.7.3")
            }
        }
        commonTest {
            dependencies {
                implementation(kotlin("test"))
            }
        }
        jvmTest {
            dependencies {
                implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.9.0")
                implementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.9.0")
                implementation("org.junit.jupiter:junit-jupiter:5.10.2")
                runtimeOnly("org.junit.platform:junit-platform-launcher")
            }
        }
    }
}

tasks.named<Test>("jvmTest") {
    useJUnitPlatform()
}
