package life.neurone.core.common

import kotlinx.coroutines.runBlocking
import kotlin.test.assertTrue

/**
 * One install under the presence gate, for the store tests of `OI-PRIV-19`. The host's store and the device keys outlive an "app run";
 * a [Run] is a cold start: a new lock (locked), and new storage opened gated over the same sealed values.
 */
class PresenceInstall(private val sealedKeys: () -> Set<String>) {

    private class Gated : GatedSecretKeyStore {
        private val real = InMemorySecretKeyStore()
        override var isUnlocked = false
            private set
        override suspend fun unlock(reason: String): Boolean { isUnlocked = true; return true }
        override fun lock() { isUnlocked = false }
        override var isEnabled = true
        override suspend fun enable(reason: String): Boolean = true
        override fun load(): ByteArray? { if (!isUnlocked) throw SecretKeyUnavailableException("locked"); return real.load() }
        override fun store(key: ByteArray) { if (!isUnlocked) throw SecretKeyUnavailableException("locked"); real.store(key) }
        override fun destroy() = real.destroy()
    }

    val host = InMemoryKeyValueStore()
    private val deviceSecrets = InMemorySecretKeyStore()
    private val gated = Gated()

    inner class Run {
        val lock = PresenceLock(gated.also { it.lock() }, { 0L })
        val kv: KeyValueStore = ProtectedStorage.openGated(
            host, deviceSecrets, gated, lock,
            protectedKeys = sealedKeys,
            deviceTierKeys = { emptySet() },
        ).store

        fun unlock() = runBlocking { assertTrue(lock.unlock("Unlock your health data")) }
    }
}
