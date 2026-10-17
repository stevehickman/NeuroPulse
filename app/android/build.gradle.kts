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

// Dependency locking (OI-SOUPKT-01 item 3). Every version in this build is fixed, so locking pins nothing new; the
// lock files are the record of what actually resolved, the input of the vulnerability scan
// (.github/workflows/dependency-scan.yml) and the thing a changed transitive version shows up in review against.
// LENIENT: a library missing from a lock file does not fail a build, so a new dependency is never blocked by a stale
// lock; the scan job fails on drift instead. Regenerate: gradle resolveAndLockAll --write-locks
allprojects {
    dependencyLocking {
        lockAllConfigurations()
        lockMode.set(LockMode.LENIENT)
    }
    tasks.register("resolveAndLockAll") {
        notCompatibleWithConfigurationCache("resolves configurations at execution time")
        doLast {
            configurations.filter { it.isCanBeResolved }.forEach { runCatching { it.resolve() } }
        }
    }
}
