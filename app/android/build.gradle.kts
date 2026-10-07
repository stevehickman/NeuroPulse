// Plugin versions, once. Every module applies these by id with no version of its own, so Kotlin, Compose
// and AGP are the same version in :core, :shared (Compose Multiplatform), :app (Android), :desktop and
// :web — a module that named its own version would load a second copy of the plugin.
plugins {
    kotlin("multiplatform") version "2.0.21" apply false
    kotlin("android") version "2.0.21" apply false
    kotlin("plugin.serialization") version "2.0.21" apply false
    kotlin("plugin.compose") version "2.0.21" apply false
    id("org.jetbrains.compose") version "1.7.3" apply false
    id("com.android.application") version "8.9.3" apply false
    id("com.android.library") version "8.9.3" apply false
}
