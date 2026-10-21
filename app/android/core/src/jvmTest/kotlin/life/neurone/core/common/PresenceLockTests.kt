package life.neurone.core.common

import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** UHDR is readable only while the person's presence stands (REQ-PRES-01 to -03, -05, -08): the lock, the two tiers, and the rule that comes before the gate. */
class PresenceLockTests {

    /** A key the "platform" releases only after [unlock] says yes. */
    private class Gated(var allow: Boolean = true) : GatedSecretKeyStore {
        private val real = InMemorySecretKeyStore()
        override var isUnlocked = false
            private set
        var prompts = 0
        var stored = 0
        override suspend fun unlock(reason: String): Boolean { prompts++; if (allow) isUnlocked = true; return allow }
        override fun lock() { isUnlocked = false }
        override var isEnabled = true
        override suspend fun enable(reason: String): Boolean { isEnabled = true; return true }
        override fun load(): ByteArray? { if (!isUnlocked) throw SecretKeyUnavailableException("locked"); return real.load() }
        override fun store(key: ByteArray) { if (!isUnlocked) throw SecretKeyUnavailableException("locked"); stored++; real.store(key) }
        override fun destroy() = real.destroy()
    }

    private class Clock(var ms: Long = 0)

    private val history = "np.session.history"
    private val consent = "np.consent.research"
    private val token = "np.shdr.device-token"
    private val plain = "np.not.sealed"

    private class Rig(val gated: Gated = Gated(), val clock: Clock = Clock()) {
        val host = InMemoryKeyValueStore()
        val deviceSecrets = InMemorySecretKeyStore()
        val lock = PresenceLock(gated, { clock.ms })
        val storage = ProtectedStorage.openGated(
            host, deviceSecrets, gated, lock,
            protectedKeys = { setOf("np.session.history", "np.consent.research", "np.shdr.device-token") },
            deviceTierKeys = { setOf("np.shdr.device-token") },
        )
        val store get() = storage.store
        fun unlock() = runBlocking { lock.unlock("Unlock your health data") }
    }

    // @verifies REQ-PRES-03
    @Test fun a_cold_start_is_locked_and_asks() {
        val r = Rig()
        assertEquals(PresenceState.LOCKED, r.lock.state.value)
        assertEquals(0, r.gated.prompts)
        assertTrue(r.unlock())
        assertEquals(PresenceState.UNLOCKED, r.lock.state.value)
    }

    // @verifies REQ-PRES-01#a
    @Test fun a_refused_prompt_leaves_it_locked() {
        val r = Rig(Gated(allow = false))
        assertFalse(r.unlock())
        assertEquals(PresenceState.LOCKED, r.lock.state.value)
    }

    // @verifies REQ-PRES-01#a
    @Test fun uhdr_reads_as_absent_while_locked_never_as_ciphertext() {
        val r = Rig()
        r.unlock()
        r.store.putString(history, "the person's history")
        assertTrue(r.host.getString(history)!!.startsWith(EncryptedKeyValueStore.PREFIX))
        r.lock.lock()
        assertNull(r.store.getString(history))
        assertEquals(0, r.store.getInt(history, 0))
        assertTrue(r.unlock())
        assertEquals("the person's history", r.store.getString(history))
    }

    // @verifies REQ-PRES-05
    @Test fun the_device_tier_stays_readable_locked_and_everything_else_does_not() {
        val r = Rig()
        r.store.putString(token, "device-token-value")
        r.unlock()
        r.store.putString(history, "h")
        r.lock.lock()
        assertEquals("device-token-value", r.store.getString(token))
        assertNull(r.store.getString(history))
        assertTrue(r.host.getString(token)!!.startsWith(EncryptedKeyValueStore.PREFIX), "the device tier is still sealed at rest")
    }

    // @verifies REQ-PRES-01#b
    @Test fun a_write_while_locked_changes_nothing_on_disk_and_is_applied_at_unlock() {
        val r = Rig()
        r.unlock()
        r.store.putString(consent, "granted")
        r.lock.lock()
        val onDisk = r.host.getString(consent)
        r.store.putString(consent, "withdrawn")
        assertEquals(onDisk, r.host.getString(consent), "disk is untouched while locked")
        assertEquals("withdrawn", r.store.getString(consent), "the app reads its own write")
        r.unlock()
        assertEquals("withdrawn", r.store.getString(consent), "a withdrawal made locked survives the unlock")
        assertTrue(r.host.getString(consent)!!.startsWith(EncryptedKeyValueStore.PREFIX))
        assertTrue(r.host.getString(consent) != onDisk)
    }

    // @verifies REQ-PRES-01#b
    @Test fun the_latest_write_wins_and_a_flag_or_a_number_written_locked_reads_as_its_type() {
        val r = Rig()
        r.store.putBoolean(consent, true)
        r.store.putBoolean(consent, false)
        r.store.putInt(history, 7)
        assertFalse(r.store.getBoolean(consent, true))
        assertEquals(7, r.store.getInt(history, 0))
        r.unlock()
        assertFalse(r.store.getBoolean(consent, true))
        assertEquals(7, r.store.getInt(history, 0))
    }

    // @verifies REQ-PRES-01#c
    @Test fun a_locked_key_is_never_taken_for_no_key_and_no_new_key_is_made() {
        val r = Rig()
        r.store.putString(history, "written locked")
        assertEquals(0, r.gated.stored, "the gated keystore was asked for nothing and given nothing")
        r.unlock()
        assertEquals("written locked", r.store.getString(history))
        assertEquals(1, r.gated.stored)
    }

    // @verifies REQ-PRES-01#a
    @Test fun locking_drops_the_key_held_in_memory() {
        val r = Rig()
        r.unlock()
        r.store.putString(history, "h")
        r.lock.lock()
        assertFailsWith<SecretKeyUnavailableException> { r.gated.load() }
        assertNull(r.store.getString(history), "no cached key opens it")
    }

    // @verifies REQ-PRES-03
    @Test fun it_locks_after_the_background_timeout_and_not_before() {
        val r = Rig()
        r.unlock()
        r.lock.onBackground()
        r.clock.ms += PresenceLock.BACKGROUND_TIMEOUT_MS - 1
        r.lock.onForeground()
        assertEquals(PresenceState.UNLOCKED, r.lock.state.value)
        r.lock.onBackground()
        r.clock.ms += PresenceLock.BACKGROUND_TIMEOUT_MS
        r.lock.onForeground()
        assertEquals(PresenceState.LOCKED, r.lock.state.value)
    }

    // @verifies REQ-PRES-03
    @Test fun a_host_timer_locks_without_waiting_for_the_app_to_return() {
        val r = Rig()
        r.unlock()
        r.lock.onBackground()
        r.clock.ms += PresenceLock.BACKGROUND_TIMEOUT_MS
        r.lock.tick()
        assertEquals(PresenceState.LOCKED, r.lock.state.value)
    }

    // @verifies REQ-PRES-03
    @Test fun the_timeout_is_five_minutes_as_the_principal_accepted() {
        assertEquals(300_000L, PresenceLock.BACKGROUND_TIMEOUT_MS)
    }

    // @verifies REQ-PRES-01#b
    @Test fun a_removal_acts_at_once_and_drops_a_held_write() {
        val r = Rig()
        r.unlock()
        r.store.putString(history, "h")
        r.lock.lock()
        r.store.putString(history, "newer")
        r.store.remove(history)
        assertNull(r.store.getString(history))
        r.unlock()
        assertNull(r.store.getString(history))
    }

    // @verifies REQ-PRES-02
    @Test fun a_clear_destroys_both_tiers_keys() {
        val r = Rig()
        r.unlock()
        r.store.putString(token, "t")
        r.store.putString(history, "h")
        r.storage.sealing!!.destroyKey()
        assertNull(r.deviceSecrets.load())
        assertNull(r.gated.load())
    }

    // @verifies REQ-PRES-08
    @Test fun the_gate_is_not_turned_on_before_a_backup_exists() {
        assertEquals(PresenceEnablement.Decision.BackupRequired, PresenceEnablement.decide(hasRecoverableBackup = false))
        assertEquals(PresenceEnablement.Decision.Allowed, PresenceEnablement.decide(hasRecoverableBackup = true))
    }

    // @verifies REQ-PRES-05
    @Test fun the_device_tier_the_core_names_is_the_three_the_principal_accepted() {
        assertEquals(setOf("np.shdr.device-token", "np.warranty.consent-granted", "ble.last-hub-id"), EncryptedKeyValueStore.deviceTierKeys())
        assertTrue(EncryptedKeyValueStore.deviceTierKeys().all { it in EncryptedKeyValueStore.uhdrKeys() })
    }
}
