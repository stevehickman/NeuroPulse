package life.neurone.core.analytics

import kotlinx.coroutines.runBlocking
import life.neurone.core.common.GatedSecretKeyStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.ProtectedStorage
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * An erase or a withdrawal made while the presence gate is locked still reaches the analytics vendor, because the analytics ID and its
 * erasure queue are device-tier keys (the principal, 2026-11-21; `NP-APP-PRESENCE-001` §4, `REQ-PRES-05`, `REQ-SW03-19` (g)).
 * This is not a keystore that failed (`StorageProtection.LOCKED`): there the device tier is sealed under the same broken keystore.
 */
class AnalyticsLockedEraseTests {
    private val id = "0b1c2d3e4f5061728394a5b6c7d8e9f0"
    private val history = "np.session.history"

    private class Gated : GatedSecretKeyStore {
        private val real = InMemorySecretKeyStore()
        override var isUnlocked = false
            private set
        override val isEnabled = true
        override suspend fun unlock(reason: String): Boolean { isUnlocked = true; return true }
        override fun lock() { isUnlocked = false }
        override suspend fun enable(reason: String) = true
        override fun load(): ByteArray? { if (!isUnlocked) throw SecretKeyUnavailableException("locked"); return real.load() }
        override fun store(key: ByteArray) { if (!isUnlocked) throw SecretKeyUnavailableException("locked"); real.store(key) }
        override fun destroy() = real.destroy()
    }

    private class Relay : HttpTransport {
        val seen = mutableListOf<String>()
        override suspend fun post(request: HttpRequest): HttpResponse? {
            seen += request.body.decodeToString()
            return HttpResponse(200, emptyMap(), ByteArray(0))
        }
    }

    /** A backend that keeps its ID where the real one does, in the protected store. */
    private class Backend(private val store: life.neurone.core.common.KeyValueStore) : AnalyticsBackend {
        override val distinctId: String? get() = store.getString(PostHogAnalyticsBackend.ID_KEY)
        override fun configure() {}
        override fun reset() = store.remove(PostHogAnalyticsBackend.ID_KEY)
        override fun track(event: String, properties: Map<String, String>) {}
    }

    private fun lockedStore(): Triple<life.neurone.core.common.KeyValueStore, PresenceLock, InMemoryKeyValueStore> {
        val host = InMemoryKeyValueStore()
        val gated = Gated()
        val lock = PresenceLock(gated, { 0L })
        val storage = ProtectedStorage.openGated(
            host, InMemorySecretKeyStore(), gated, lock,
            protectedKeys = { setOf(history, PostHogAnalyticsBackend.ID_KEY, AnalyticsErasureQueue.KEY) },
            deviceTierKeys = { setOf(PostHogAnalyticsBackend.ID_KEY, AnalyticsErasureQueue.KEY) },
        )
        return Triple(storage.store, lock, host)
    }

    // @verifies REQ-SW03-19#g
    @Test fun anEraseWhileLockedReadsTheIdAndAsksTheRelayAndTheIdIsGone() = runBlocking {
        val (store, lock, host) = lockedStore()
        assertTrue(lock.unlock("Unlock"))
        store.putString(PostHogAnalyticsBackend.ID_KEY, id)
        store.putString(history, "the person's history")
        lock.lock()
        assertNull(store.getString(history), "the history is UHDR and stays unreadable while locked")

        val relay = Relay()
        val gate = ResearchAnalyticsGate(store, Backend(store), AnalyticsErasureQueue(store, relay) { "https://analytics.example.org" })
        assertEquals(0, gate.resetForErase(), "the relay took the ID")
        assertEquals(1, relay.seen.size)
        assertTrue(relay.seen.single().contains(id))
        assertNull(host.getString(PostHogAnalyticsBackend.ID_KEY), "the ID is forgotten")
    }

    // @verifies REQ-SW03-19#g
    @Test fun anIdQueuedByAnEarlierWithdrawalIsAskedAboutWhileLockedToo() = runBlocking {
        val (store, lock, _) = lockedStore()
        assertTrue(lock.unlock("Unlock"))
        val queue = AnalyticsErasureQueue(store, Relay().also { }) { "https://analytics.example.org" }
        queue.enqueue(id)
        lock.lock()

        val relay = Relay()
        val locked = AnalyticsErasureQueue(store, relay) { "https://analytics.example.org" }
        assertEquals(listOf(id), locked.pending, "the queue is readable while locked")
        locked.drain()
        assertEquals(1, relay.seen.size)
        assertEquals(emptyList(), locked.pending)
    }
}
