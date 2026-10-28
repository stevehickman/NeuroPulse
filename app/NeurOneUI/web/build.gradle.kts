// :web — the browser app. The UI is :shared, compiled to WebAssembly; this module is the host: a page, a
// canvas, localStorage, and the PlatformServices that go with them.
//
// Run:     gradle :web:wasmJsBrowserDevelopmentRun
// Release: gradle :web:wasmJsBrowserDistribution
//
// This build sits beside the React app in app/web; it does not replace it. When the shared UI covers the
// same screens, the React UI is the thing to retire (app/NeurOneUI/README.md).

import org.jetbrains.kotlin.gradle.ExperimentalWasmDsl

plugins {
    kotlin("multiplatform")
    kotlin("plugin.compose")
    id("org.jetbrains.compose")
}

// The shared NPPS core, as WebAssembly (common/npps-ffi built for wasm32), is served beside the page; Main.kt loads it
// before composing anything that reads a protocol (OI-UI-KMP-01).
val nppsWasmDir = layout.buildDirectory.dir("generated/nppsWasm")
val copyNppsWasm by tasks.registering(Copy::class) {
    dependsOn(project(":core").tasks.named("buildNppsWasm"))
    from(rootProject.projectDir.parentFile.parentFile.resolve("common/generated/neurone_npps.wasm"))
    into(nppsWasmDir)
}

kotlin {
    @OptIn(ExperimentalWasmDsl::class)
    wasmJs {
        moduleName = "neurone-web"
        browser {
            commonWebpackConfig {
                outputFileName = "neurone-web.js"
                // The page's Content-Security-Policy allows no eval(). webpack's development default (`eval-source-map`) wraps every
                // module in eval(), which the policy refuses, so the dev build would load nothing. A plain source map needs no eval.
                devtool = "source-map"
            }
        }
        binaries.executable()
    }
    sourceSets {
        wasmJsMain {
            resources.srcDir(nppsWasmDir)
        }
        wasmJsMain.dependencies {
            implementation(project(":shared"))
            // Sign-in to the cloud providers (SWR-MG-19): the sign-in window and the page it returns to.
            implementation(project(":cloudauth"))
            implementation(compose.runtime)
            implementation(compose.foundation)
            implementation(compose.material3)
            implementation(compose.components.resources)
        }
    }
}

tasks.matching { it.name == "wasmJsProcessResources" }.configureEach { dependsOn(copyNppsWasm) }
