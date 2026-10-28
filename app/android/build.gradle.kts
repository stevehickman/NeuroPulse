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

// Dependency locking (OI-SOUPKT-01 item 3). Only the classpaths that ship are locked: the runtime classpaths of the
// production targets and the native targets' compile klibs. Test classpaths, the Kotlin and Android build tooling
// (compiler, AGP's unified test platform, lint) never reach a user's device, and locking them put their advisories
// (netty, protobuf, an old guava) into the scan as if they were product SOUP. Every version in this build is fixed, so
// locking pins little; the lock files are the record of what resolved, the input of the vulnerability scan
// (.github/workflows/dependency-scan.yml) and the thing a changed transitive version shows up in review against.
// LENIENT: a library missing from a lock file does not fail a build, so a new dependency is never blocked by a stale
// lock; the scan job fails on drift instead. Regenerate (delete the old files first):
//   gradle :core:resolveAndLockAll :cloudstore:resolveAndLockAll :shared:resolveAndLockAll :desktop:resolveAndLockAll :web:resolveAndLockAll --write-locks
allprojects {
    dependencyLocking {
        lockAllConfigurations()
        lockMode.set(LockMode.LENIENT)
    }
    tasks.register("resolveAndLockAll") {
        notCompatibleWithConfigurationCache("resolves configurations at execution time")
        doLast {
            configurations
                .filter { it.isCanBeResolved }
                .filter { c ->
                    val shipped = c.name.endsWith("RuntimeClasspath", ignoreCase = true) || c.name.endsWith("CompileKlibraries")
                    shipped && !c.name.contains("test", ignoreCase = true)
                }
                .forEach { runCatching { it.resolve() } }
        }
    }
}
