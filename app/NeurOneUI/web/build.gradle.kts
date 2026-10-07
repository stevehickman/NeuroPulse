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

kotlin {
    @OptIn(ExperimentalWasmDsl::class)
    wasmJs {
        moduleName = "neurone-web"
        browser {
            commonWebpackConfig { outputFileName = "neurone-web.js" }
        }
        binaries.executable()
    }
    sourceSets {
        wasmJsMain.dependencies {
            implementation(project(":shared"))
            implementation(compose.runtime)
            implementation(compose.foundation)
            implementation(compose.material3)
            implementation(compose.components.resources)
        }
    }
}
