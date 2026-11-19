package life.neurone.core.session

import kotlinx.coroutines.runBlocking
import kotlinx.datetime.LocalDate
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.GatedSecretKeyStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.ProtectedStorage
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.protocol.PersistedKeys
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The session history under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-10` (1)): built locked, it never takes "absent" for "empty". */
class SessionHistoryPresenceTests {

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

    /** One install: the host's store and keys outlive an "app run", which is a new lock (locked, as at a cold start) and new storage. */
    private class Install {
        val host = InMemoryKeyValueStore()
        private val deviceSecrets = InMemorySecretKeyStore()
        private val gated = Gated()

        inner class Run {
            val lock = PresenceLock(gated.also { it.lock() }, { 0L })
            val kv = ProtectedStorage.openGated(
                host, deviceSecrets, gated, lock,
                protectedKeys = { setOf(KEY, "np.shdr.device-token") },
                deviceTierKeys = { setOf("np.shdr.device-token") },
            ).store
            val history = SessionHistoryStore(kv, lock = lock)
            fun unlock() = runBlocking { assertTrue(lock.unlock("Unlock your health data")) }
        }
    }

    private fun session(name: String, day: Int = 1) = CompletedSessionSummary(
        protocolName = name,
        completedAt = LocalDate(2026, 7, day),
        durationSeconds = 1200,
        averageCoherenceScore = 7.3f,
        rmssdMilliseconds = 42,
        impedancePassCount = 8,
    )

    /** An install whose sealed history already holds [names], left as an earlier run would leave it. */
    private fun installWith(vararg names: String): Install = Install().also { install ->
        val run = install.Run()
        run.unlock()
        names.forEachIndexed { i, name -> run.history.record(session(name, day = i + 1)) }
        run.lock.lock()
    }

    // The defect: a store built locked read "absent" as an empty list, and its next record() replaced the sealed history.
    // @verifies REQ-PRES-01#d
    @Test fun a_session_recorded_by_a_store_built_locked_joins_the_sealed_history_at_unlock() {
        val install = installWith("earlier one", "earlier two")
        val run = install.Run() // a cold start: locked
        assertTrue(run.history.records.isEmpty())

        run.history.record(session("finished while locked", day = 9))
        assertTrue(run.history.records.isEmpty(), "nothing is shown while locked")

        run.unlock()
        assertEquals(listOf("finished while locked", "earlier two", "earlier one"), run.history.records.map { it.protocolName })

        // And it is what is sealed, not only what is in memory: the next run reads all three.
        run.lock.lock()
        val next = install.Run()
        next.unlock()
        assertEquals(listOf("finished while locked", "earlier two", "earlier one"), next.history.records.map { it.protocolName })
        assertEquals(3, next.history.records.map { it.insertionIndex }.toSet().size, "each record has its own place in the order")
    }

    // @verifies REQ-PRES-01#b
    @Test fun a_session_recorded_while_locked_changes_nothing_on_disk_until_the_unlock() {
        val install = installWith("earlier")
        val sealed = install.host.getString(KEY)!!
        assertTrue(sealed.startsWith(EncryptedKeyValueStore.PREFIX))

        val run = install.Run()
        run.history.record(session("finished while locked"))
        assertEquals(sealed, install.host.getString(KEY), "the sealed history is byte for byte as it was")
        assertNull(run.kv.getString(KEY), "and no write is held for the store to apply over it")
    }

    // @verifies REQ-PRES-01#d
    @Test fun the_history_leaves_memory_when_the_lock_closes_and_is_read_again_when_it_opens() {
        val install = installWith("earlier")
        val run = install.Run()
        run.unlock()
        assertEquals(1, run.history.records.size)

        run.lock.lock()
        assertTrue(run.history.records.isEmpty())

        run.unlock()
        assertEquals(listOf("earlier"), run.history.records.map { it.protocolName })
    }

    // @verifies REQ-PRES-01#d
    @Test fun sessions_from_several_locked_spells_all_arrive_in_the_order_they_finished() {
        val install = installWith("earlier")
        val run = install.Run()
        run.history.record(session("first", day = 5))
        run.history.record(session("second", day = 5))
        run.unlock()
        run.lock.lock()
        run.history.record(session("third", day = 5))
        run.unlock()
        assertEquals(listOf("third", "second", "first", "earlier"), run.history.records.map { it.protocolName })
    }

    // A removal acts at once and needs no presence; a session still waiting must not bring the history back at the unlock.
    // @verifies REQ-PRES-01#b
    @Test fun clearing_the_history_while_locked_removes_the_sealed_value_and_what_was_waiting() {
        val install = installWith("earlier")
        val run = install.Run()
        run.history.record(session("finished while locked"))
        run.history.clearHistory()
        assertNull(install.host.getString(KEY))
        run.unlock()
        assertTrue(run.history.records.isEmpty())
        assertNull(install.host.getString(KEY))
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_store_with_no_lock_behaves_as_it_always_did() {
        val kv = InMemoryKeyValueStore()
        SessionHistoryStore(kv).record(session("one"))
        val again = SessionHistoryStore(kv)
        again.record(session("two", day = 2))
        assertEquals(listOf("two", "one"), again.records.map { it.protocolName })
        assertFalse(kv.getString(KEY).isNullOrEmpty())
    }

    private companion object {
        const val KEY = PersistedKeys.SESSION_HISTORY_KEY
    }
}
