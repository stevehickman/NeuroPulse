package life.neurone.core.common

import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** An install that already holds sealed UHDR is moved to the gated key without losing it (REQ-PRES-07, with REQ-PRES-01 and -05). */
class PresenceMigrationTests {
    private val history = "np.session.history"
    private val consent = "np.consent.research"
    private val token = "np.shdr.device-token"
    private val all = setOf(history, consent, token)
    private val deviceTier = setOf(token)

    /** A key the "platform" releases only after [unlock]. */
    private class Gated : GatedSecretKeyStore {
        val real = InMemorySecretKeyStore()
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

    /** A host store whose write to [failOn] fails the first time, as a crash part-way through the pass would. */
    private class FlakyHost(var failOn: String? = null) : KeyValueStore by InMemoryKeyValueStore() {
        private val inner = InMemoryKeyValueStore()
        override fun getString(key: String) = inner.getString(key)
        override fun putString(key: String, value: String) {
            if (key == failOn) { failOn = null; throw SecretKeyUnavailableException("the write failed") }
            inner.putString(key, value)
        }
        override fun getInt(key: String, default: Int) = inner.getInt(key, default)
        override fun putInt(key: String, value: Int) = inner.putInt(key, value)
        override fun getBoolean(key: String, default: Boolean) = inner.getBoolean(key, default)
        override fun putBoolean(key: String, value: Boolean) = inner.putBoolean(key, value)
        override fun remove(key: String) = inner.remove(key)
    }

    /** An install from before the gate: every protected value sealed under [ungated]. */
    private fun oldInstall(host: KeyValueStore, ungated: SecretKeyStore) {
        val old = EncryptedKeyValueStore(host, ungated, all)
        old.putString(history, "ten sessions")
        old.putBoolean(consent, true)
        old.putString(token, "device-token-value")
    }

    private class Open(val storage: ProtectedStorage, val lock: PresenceLock, val gated: Gated)

    private fun openGated(host: KeyValueStore, ungated: SecretKeyStore, gated: Gated = Gated()): Open {
        val lock = PresenceLock(gated, { 0L })
        return Open(ProtectedStorage.openGated(host, ungated, gated, lock, { all }, { deviceTier }), lock, gated)
    }

    private fun Open.unlock() = runBlocking { lock.unlock("Unlock") }

    // @verifies REQ-PRES-07
    @Test fun the_first_unlock_moves_the_sealed_values_to_the_gated_key_and_loses_none() {
        val host = InMemoryKeyValueStore()
        val ungated = InMemorySecretKeyStore()
        oldInstall(host, ungated)

        val app = openGated(host, ungated)
        val store = app.storage.store
        // Locked: UHDR reads as absent, never as ciphertext; the device tier still reads (REQ-PRES-01 (a), -05).
        assertNull(store.getString(history))
        assertEquals("device-token-value", store.getString(token))

        assertTrue(app.unlock())
        assertEquals("ten sessions", store.getString(history))
        assertEquals(true, store.getBoolean(consent, false))

        // On disk the UHDR values now open under the gated key alone, and no longer under the earlier key.
        val gatedOnly = EncryptedKeyValueStore(host, app.gated.real.let { k -> object : SecretKeyStore by k {} }, all)
        assertEquals("ten sessions", gatedOnly.getString(history))
        assertNull(EncryptedKeyValueStore(host, ungated, all).getString(history), "the earlier key no longer opens the UHDR")
        assertEquals("device-token-value", EncryptedKeyValueStore(host, ungated, all).getString(token), "the device tier stays under the earlier key")
    }

    // @verifies REQ-PRES-07
    @Test fun a_pass_that_stops_part_way_loses_nothing_and_the_next_one_finishes_it() {
        val host = FlakyHost()
        val ungated = InMemorySecretKeyStore()
        oldInstall(host, ungated)
        val gated = Gated()

        host.failOn = consent
        val first = openGated(host, ungated, gated)
        first.unlock()
        // Whichever value the failure hit, both still read: one under each key.
        assertEquals("ten sessions", first.storage.store.getString(history))
        assertEquals(true, first.storage.store.getBoolean(consent, false))

        // A new run, the gated key unchanged: it finishes, and everything now opens under the gated key alone.
        val second = openGated(host, ungated, gated)
        gated.lock()
        second.unlock()
        assertEquals("ten sessions", second.storage.store.getString(history))
        assertEquals(true, second.storage.store.getBoolean(consent, false))
        val gatedOnly = EncryptedKeyValueStore(host, object : SecretKeyStore by gated.real {}, all)
        assertEquals("ten sessions", gatedOnly.getString(history))
        assertEquals("true", gatedOnly.getString(consent))
    }

    // @verifies REQ-PRES-01#b
    @Test fun a_withdrawal_made_while_locked_lands_on_top_of_the_moved_value() {
        val host = InMemoryKeyValueStore()
        val ungated = InMemorySecretKeyStore()
        oldInstall(host, ungated)

        val app = openGated(host, ungated)
        app.storage.store.putBoolean(consent, false)
        assertTrue(app.unlock())
        assertEquals(false, app.storage.store.getBoolean(consent, true), "the withdrawal survived the move of the old 'true'")
    }

    // @verifies REQ-PRES-07
    @Test fun a_keystore_that_cannot_be_asked_changes_nothing() {
        val host = InMemoryKeyValueStore()
        val ungated = object : SecretKeyStore {
            val real = InMemorySecretKeyStore()
            var down = false
            override fun load(): ByteArray? { if (down) throw SecretKeyUnavailableException("down"); return real.load() }
            override fun store(key: ByteArray) = real.store(key)
            override fun destroy() = real.destroy()
        }
        oldInstall(host, ungated)
        val before = host.getString(history)

        ungated.down = true
        val app = openGated(host, ungated)
        assertTrue(app.unlock())
        assertEquals(before, host.getString(history), "nothing was written")

        ungated.down = false
        val again = openGated(host, ungated, app.gated)
        app.gated.lock()
        again.unlock()
        assertEquals("ten sessions", again.storage.store.getString(history), "and it moves once the earlier key can be asked")
    }
}

/** UHDR protection is never made weaker (REQ-PRES-09, `CLAUDE.md` §5.1). */
class PresenceNeverWeakerTests {
    private val history = "np.session.history"
    private val token = "np.shdr.device-token"

    private class DownGated : GatedSecretKeyStore {
        val real = InMemorySecretKeyStore()
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

    // @verifies REQ-PRES-09#a
    @Test fun the_key_interfaces_have_no_way_to_turn_the_gate_off() {
        val offered = listOf(GatedSecretKeyStore::class.java, PassphraseKey::class.java).flatMap { c -> c.methods.map { it.name } }.toSet()
        assertTrue(offered.none { it.contains("disable", ignoreCase = true) || it.contains("turnOff", ignoreCase = true) }, "found: $offered")
    }

    // @verifies REQ-PRES-09#b
    @Test fun a_gated_install_that_cannot_open_its_tiers_writes_nothing_in_the_clear() {
        val host = InMemoryKeyValueStore()
        // The earlier keystore cannot be asked, so the device tier cannot be built.
        val broken = object : SecretKeyStore {
            override fun load(): ByteArray? = throw SecretKeyUnavailableException("down")
            override fun store(key: ByteArray) = throw SecretKeyUnavailableException("down")
            override fun destroy() {}
        }
        host.putString(token, "plain-left-over")
        val gated = DownGated()
        val lock = PresenceLock(gated, { 0L })
        val storage = ProtectedStorage.openGated(host, broken, gated, lock, { setOf(history, token) }, { setOf(token) })
        assertEquals(StorageProtection.LOCKED, storage.state, "the run is held in memory, not opened ungated")
        storage.store.putString(history, "a new session")
        assertNull(host.getString(history), "nothing was written to disk, in the clear or sealed")
        assertEquals("a new session", storage.store.getString(history), "and it still reads for this run")
    }
}
