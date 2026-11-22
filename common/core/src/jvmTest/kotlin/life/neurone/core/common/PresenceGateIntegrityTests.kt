package life.neurone.core.common

import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The presence gate survives its own first use and does not undo itself (`OI-PRIV-62`, `REQ-PRES-11`): its records stay readable before
 * anyone is present, a value sealed under the gated key is never opened ungated, the background timeout cannot be defeated by setting the
 * clock back, and the stores do not leave key copies behind on every write.
 */
class PresenceGateIntegrityTests {
    private val history = "np.session.history"
    private val consent = "np.consent.research"
    private val token = "np.shdr.device-token"
    private val enabledFlag = "np.presence.enabled"
    private val passphrase = "correct horse battery staple"

    /** The core's list, plus the gate's own records, as a caller that forgot to leave them out would pass it. */
    private fun everyKey(): Set<String> = EncryptedKeyValueStore.uhdrKeys() + PassphraseGatedSecretKeyStore.WRAP_KEY + enabledFlag
    private fun deviceTier(): Set<String> = EncryptedKeyValueStore.deviceTierKeys()

    /**
     * A gated key whose "on" flag is a boolean in the same host store the protected values live in, as `IosGatedSecretKeyStore` keeps
     * `np.presence.enabled` in the standard user defaults. The key itself is kept elsewhere (the Keychain), so it is not in [host].
     */
    private class FlagGated(private val host: KeyValueStore, private val real: SecretKeyStore = InMemorySecretKeyStore()) : GatedSecretKeyStore {
        override var isUnlocked = false
            private set
        override val isEnabled: Boolean get() = host.getBoolean("np.presence.enabled", false)
        override suspend fun enable(reason: String): Boolean { host.putBoolean("np.presence.enabled", true); return true }
        override suspend fun unlock(reason: String): Boolean { isUnlocked = true; return true }
        override fun lock() { isUnlocked = false }
        override fun load(): ByteArray? { if (!isUnlocked) throw SecretKeyUnavailableException("locked"); return real.load() }
        override fun store(key: ByteArray) { if (!isUnlocked) throw SecretKeyUnavailableException("locked"); real.store(key) }
        override fun destroy() { real.destroy(); host.remove("np.presence.enabled") }
        fun keyStore() = real
    }

    /** A keystore that counts its loads and keeps every array it handed out, to show the stores wipe their copies. */
    private class Watched(private val inner: SecretKeyStore) : SecretKeyStore by inner {
        val handedOut = mutableListOf<ByteArray>()
        override fun load(): ByteArray? = inner.load()?.also { handedOut += it }
        fun stillReadable() = handedOut.count { bytes -> bytes.any { it != 0.toByte() } }
    }

    private class Clock(var ms: Long = 0)

    /** The values an install held before the gate was on, sealed under the device key by the ordinary ungated store. */
    private fun ungatedInstall(host: KeyValueStore, device: SecretKeyStore) {
        val store = ProtectedStorage.open(host, device) { everyKey() }.store
        store.putString(history, "ten sessions")
        store.putBoolean(consent, true)
        store.putString(token, "device-token-value")
    }

    // @verifies REQ-PRES-11#a
    @Test fun a_passphrase_gate_turned_on_unlocks_again_after_a_restart() {
        val host = InMemoryKeyValueStore()
        val device = InMemorySecretKeyStore()
        ungatedInstall(host, device)
        assertEquals(PassphraseResult.OK, runBlocking { PassphraseGatedSecretKeyStore(host).setPassphrase(passphrase) })

        // Two cold starts over the same host store, each with a new key store, lock and storage, as a desktop restart builds them.
        repeat(2) { run ->
            val gated = PassphraseGatedSecretKeyStore(host)
            assertTrue(gated.isEnabled, "run $run: the gate is still on")
            val lock = PresenceLock(gated, { 0L })
            val store = ProtectedStorage.openGated(host, device, gated, lock, { everyKey() }, { deviceTier() }).store
            assertEquals(PassphraseResult.OK, runBlocking { lock.unlockWithPassphrase(passphrase) }, "run $run: the passphrase opens the record")
            assertEquals("ten sessions", store.getString(history), "run $run")
            assertTrue(store.getBoolean(consent, false), "run $run")
            store.putString(history, "eleven sessions")
            store.putString(history, "ten sessions")
            val record = host.getString(PassphraseGatedSecretKeyStore.WRAP_KEY)
            assertNotNull(record)
            assertFalse(record.startsWith(EncryptedKeyValueStore.PREFIX), "run $run: the wrap record is never sealed, least of all under the key it wraps")
            lock.lock()
        }
    }

    // @verifies REQ-PRES-11#a
    @Test fun a_gate_whose_flag_shares_the_host_store_is_still_on_after_a_restart() {
        val host = InMemoryKeyValueStore()
        val device = InMemorySecretKeyStore()
        val keychain = InMemorySecretKeyStore()
        ungatedInstall(host, device)
        assertTrue(runBlocking { FlagGated(host, keychain).enable("Turn on") })

        repeat(2) { run ->
            val gated = FlagGated(host, keychain)
            assertTrue(gated.isEnabled, "run $run: the flag reads true, not sealed text read as false")
            val lock = PresenceLock(gated, { 0L })
            val store = ProtectedStorage.openGated(host, device, gated, lock, { everyKey() }, { deviceTier() }).store
            assertTrue(runBlocking { lock.unlock("Unlock") })
            assertEquals("ten sessions", store.getString(history), "run $run")
            store.putString(consent, "true")
            lock.lock()
        }
    }

    /** An install that was gated and unlocked once: the UHDR values are sealed under the gated key. Returns that key. */
    private fun gatedOnce(host: KeyValueStore, device: SecretKeyStore): FlagGated {
        ungatedInstall(host, device)
        val gated = FlagGated(host)
        runBlocking { gated.enable("Turn on") }
        val lock = PresenceLock(gated, { 0L })
        ProtectedStorage.openGated(host, device, gated, lock, { everyKey() }, { deviceTier() })
        assertTrue(runBlocking { lock.unlock("Unlock") })
        return gated
    }

    // @verifies REQ-PRES-11#b
    @Test fun a_lost_flag_does_not_open_the_gated_values_ungated_and_nothing_on_disk_changes() {
        val host = InMemoryKeyValueStore()
        val device = InMemorySecretKeyStore()
        gatedOnce(host, device)
        host.remove(enabledFlag)
        val before = host.getString(history)

        assertTrue(ProtectedStorage.holdsValueSealedElsewhere(host, device, EncryptedKeyValueStore.uhdrKeys()), "the sealed values say the gate is on")
        val run = ProtectedStorage.openOnGateHost(host, device) { EncryptedKeyValueStore.uhdrKeys() }
        assertEquals(StorageProtection.LOCKED, run.state, "fails closed, never SEALED under the device key alone")
        run.store.putString(history, "written while the flag was lost")
        assertEquals(before, host.getString(history), "the gated value is untouched on disk")
        assertEquals("written while the flag was lost", run.store.getString(history), "held in memory for the run")
    }

    // @verifies REQ-PRES-11#b
    @Test fun a_gated_value_with_no_device_key_at_all_also_fails_closed() {
        val host = InMemoryKeyValueStore()
        val device = InMemorySecretKeyStore()
        gatedOnce(host, device)
        host.remove(enabledFlag)
        val run = ProtectedStorage.openOnGateHost(host, InMemorySecretKeyStore()) { EncryptedKeyValueStore.uhdrKeys() }
        assertEquals(StorageProtection.LOCKED, run.state)
    }

    // @verifies REQ-PRES-11#b
    @Test fun values_a_part_way_clear_left_under_a_destroyed_key_do_not_hold_the_run() {
        val host = InMemoryKeyValueStore()
        val device = InMemorySecretKeyStore()
        val gated = gatedOnce(host, device)
        // The clear's first step destroyed both keys and the gate; it stopped before forgetting the values.
        device.destroy()
        gated.destroy()
        host.putString(EncryptedKeyValueStore.clearProgressKey(), "destroyEncryptionKey")
        assertFalse(ProtectedStorage.holdsValueSealedElsewhere(host, device, EncryptedKeyValueStore.uhdrKeys()))
        assertEquals(StorageProtection.SEALED, ProtectedStorage.openOnGateHost(host, device) { EncryptedKeyValueStore.uhdrKeys() }.state)
    }

    // @verifies REQ-PRES-11#b
    @Test fun a_host_that_does_not_offer_the_gate_reads_a_value_under_a_lost_key_as_absent_as_before() {
        val host = InMemoryKeyValueStore()
        ungatedInstall(host, InMemorySecretKeyStore())
        val run = ProtectedStorage.open(host, InMemorySecretKeyStore()) { everyKey() }
        assertEquals(StorageProtection.SEALED, run.state)
        assertNull(run.store.getString(history))
    }

    // @verifies REQ-PRES-11#b
    @Test fun an_install_sealed_only_under_the_device_key_opens_ungated_on_a_gate_host() {
        val host = InMemoryKeyValueStore()
        val device = InMemorySecretKeyStore()
        ungatedInstall(host, device)
        assertFalse(ProtectedStorage.holdsValueSealedElsewhere(host, device, EncryptedKeyValueStore.uhdrKeys()))
        val run = ProtectedStorage.openOnGateHost(host, device) { everyKey() }
        assertEquals(StorageProtection.SEALED, run.state)
        assertEquals("ten sessions", run.store.getString(history))
    }

    // @verifies REQ-PRES-11#c
    @Test fun setting_the_clock_back_while_away_locks_it() {
        val mono = Clock(1_000)
        val wall = Clock(1_700_000_000_000)
        val gated = FlagGated(InMemoryKeyValueStore())
        val lock = PresenceLock(gated, { mono.ms }, wallMs = { wall.ms })
        assertTrue(runBlocking { lock.unlock("Unlock") })
        lock.onBackground()
        mono.ms += 1_000
        wall.ms -= 3_600_000
        lock.onForeground()
        assertEquals(PresenceState.LOCKED, lock.state.value, "a wall clock that went back is expired, never 'not yet'")
    }

    // @verifies REQ-PRES-11#c
    @Test fun a_monotonic_clock_that_went_back_is_expired_too() {
        val mono = Clock(10_000)
        val lock = PresenceLock(FlagGated(InMemoryKeyValueStore()), { mono.ms })
        assertTrue(runBlocking { lock.unlock("Unlock") })
        lock.onBackground()
        mono.ms -= 1
        lock.tick()
        assertEquals(PresenceState.LOCKED, lock.state.value)
    }

    // @verifies REQ-PRES-11#c
    @Test fun time_asleep_the_monotonic_clock_did_not_count_still_locks_by_the_wall_clock() {
        val mono = Clock(0)
        val wall = Clock(0)
        val lock = PresenceLock(FlagGated(InMemoryKeyValueStore()), { mono.ms }, wallMs = { wall.ms })
        assertTrue(runBlocking { lock.unlock("Unlock") })
        lock.onBackground()
        mono.ms += 1_000
        wall.ms += PresenceLock.BACKGROUND_TIMEOUT_MS
        lock.onForeground()
        assertEquals(PresenceState.LOCKED, lock.state.value)
    }

    // @verifies REQ-PRES-11#c
    @Test fun a_short_time_away_on_both_clocks_stays_unlocked() {
        val mono = Clock(0)
        val wall = Clock(0)
        val lock = PresenceLock(FlagGated(InMemoryKeyValueStore()), { mono.ms }, wallMs = { wall.ms })
        assertTrue(runBlocking { lock.unlock("Unlock") })
        lock.onBackground()
        mono.ms += PresenceLock.BACKGROUND_TIMEOUT_MS - 1
        wall.ms += PresenceLock.BACKGROUND_TIMEOUT_MS - 1
        lock.onForeground()
        assertEquals(PresenceState.UNLOCKED, lock.state.value)
    }

    // @verifies REQ-PRES-11#d
    @Test fun writes_while_unlocked_load_no_key_again_and_every_copy_handed_out_is_wiped() {
        val host = InMemoryKeyValueStore()
        val deviceReal = InMemorySecretKeyStore()
        ungatedInstall(host, deviceReal)
        val device = Watched(deviceReal)
        val keychain = InMemorySecretKeyStore()
        val gatedInner = FlagGated(host, keychain)
        runBlocking { gatedInner.enable("Turn on") }
        val gated = object : GatedSecretKeyStore by gatedInner {
            val handedOut = mutableListOf<ByteArray>()
            override fun load(): ByteArray? = gatedInner.load()?.also { handedOut += it }
        }
        val lock = PresenceLock(gated, { 0L })
        val store = ProtectedStorage.openGated(host, device, gated, lock, { everyKey() }, { deviceTier() }).store
        assertTrue(runBlocking { lock.unlock("Unlock") })
        assertEquals("ten sessions", store.getString(history), "moved to the gated key at the unlock")

        val deviceLoads = device.handedOut.size
        val gatedLoads = gated.handedOut.size
        repeat(5) { store.putString(history, "session $it") }
        assertEquals(deviceLoads, device.handedOut.size, "no write loads the earlier key again once the pass is done")
        assertEquals(gatedLoads, gated.handedOut.size, "nor the gated key")

        // Each store keeps one copy of its own key for the run (wiped at lock or clear); every other copy it was handed is zeros.
        assertTrue(device.stillReadable() <= 1, "device key copies still readable: ${device.stillReadable()}")
        assertTrue(gated.handedOut.count { b -> b.any { it != 0.toByte() } } <= 1, "gated key copies still readable")
    }

    // @verifies REQ-PRES-11#d
    @Test fun the_pass_runs_again_at_the_next_unlock() {
        val host = InMemoryKeyValueStore()
        val device = InMemorySecretKeyStore()
        ungatedInstall(host, device)
        val gated = FlagGated(host)
        runBlocking { gated.enable("Turn on") }
        val lock = PresenceLock(gated, { 0L })
        val store = ProtectedStorage.openGated(host, device, gated, lock, { everyKey() }, { deviceTier() }).store
        assertTrue(runBlocking { lock.unlock("Unlock") })
        lock.lock()
        // A value an earlier key sealed appears while locked (a restore, say); the next unlock moves it.
        EncryptedKeyValueStore(host, device, setOf(consent)).putString(consent, "restored")
        assertTrue(runBlocking { lock.unlock("Unlock") })
        store.putString(history, "eleven sessions")
        assertEquals("restored", EncryptedKeyValueStore(host, gated, setOf(consent)).getString(consent), "sealed under the gated key now")
    }
}
