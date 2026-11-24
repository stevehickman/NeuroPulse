package life.neurone.core.common

import kotlinx.coroutines.runBlocking
import life.neurone.core.data.RandomSource
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The passphrase fallback of the presence gate (REQ-PRES-06, -01, -02, -10): wrap, unlock, wrong passphrase, change, and what is on disk. */
class PassphraseGatedSecretKeyStoreTests {
    private val good = "correct horse battery staple"

    private var counter = 0
    private val random = RandomSource { n -> ByteArray(n) { (++counter and 0xff).toByte() } }

    /** The device's keystore: one per test, shared by every run of the store in it, as the host's keystore is. */
    private val device = InMemorySecretKeyStore()

    private fun store(host: KeyValueStore = InMemoryKeyValueStore()) = PassphraseGatedSecretKeyStore(host, { device }, random) to host

    // @verifies REQ-PRES-06
    @Test fun a_passphrase_sets_the_gate_up_and_opens_it_again_on_a_new_run() = runBlocking {
        val (first, host) = store()
        assertFalse(first.isEnabled)
        assertEquals(PassphraseResult.OK, first.setPassphrase(good))
        assertTrue(first.isEnabled)
        val key = first.load()!!
        assertEquals(32, key.size)

        val second = PassphraseGatedSecretKeyStore(host, { device }, random)
        assertTrue(second.isEnabled)
        assertFalse(second.isUnlocked, "a new run starts locked")
        assertFailsWith<SecretKeyUnavailableException> { second.load() }
        assertEquals(PassphraseResult.OK, second.unlockWithPassphrase(good))
        assertContentEquals(key, second.load())
    }

    // @verifies REQ-PRES-01#c
    @Test fun a_wrong_passphrase_is_wrong_and_never_no_key() = runBlocking {
        val (first, host) = store()
        first.setPassphrase(good)
        val second = PassphraseGatedSecretKeyStore(host, { device }, random)
        assertEquals(PassphraseResult.WRONG, second.unlockWithPassphrase("not the passphrase at all"))
        assertFalse(second.isUnlocked)
        assertFailsWith<SecretKeyUnavailableException> { second.load() }
    }

    // @verifies REQ-PRES-06
    @Test fun a_short_passphrase_is_refused_and_nothing_is_written() = runBlocking {
        val (s, host) = store()
        assertEquals(PassphraseResult.TOO_SHORT, s.setPassphrase("short"))
        assertNull(host.getString(PassphraseGatedSecretKeyStore.WRAP_KEY))
        assertFalse(s.isEnabled)
    }

    // @verifies REQ-PRES-06
    @Test fun changing_the_passphrase_keeps_the_same_data_key() = runBlocking {
        val (first, host) = store()
        first.setPassphrase(good)
        val key = first.load()!!
        val before = host.getString(PassphraseGatedSecretKeyStore.WRAP_KEY)

        assertEquals(PassphraseResult.WRONG, first.changePassphrase("not the passphrase at all", "another long passphrase"))
        assertEquals(before, host.getString(PassphraseGatedSecretKeyStore.WRAP_KEY), "a refused change writes nothing")
        assertEquals(PassphraseResult.OK, first.changePassphrase(good, "another long passphrase"))
        assertNotEquals(before, host.getString(PassphraseGatedSecretKeyStore.WRAP_KEY))

        val second = PassphraseGatedSecretKeyStore(host, { device }, random)
        assertEquals(PassphraseResult.WRONG, second.unlockWithPassphrase(good), "the old passphrase no longer opens it")
        assertEquals(PassphraseResult.OK, second.unlockWithPassphrase("another long passphrase"))
        assertContentEquals(key, second.load(), "so what was sealed under it still opens")
    }

    // @verifies REQ-PRES-02
    @Test fun neither_the_passphrase_nor_the_data_key_is_on_disk() = runBlocking {
        val (s, host) = store()
        s.setPassphrase(good)
        val stored = host.getString(PassphraseGatedSecretKeyStore.WRAP_KEY)!!
        assertFalse(stored.contains(good))
        val keyHex = s.load()!!.joinToString("") { (it.toInt() and 0xff).toString(16).padStart(2, '0') }
        assertFalse(stored.contains(keyHex))
    }

    // @verifies REQ-PRES-01#a
    @Test fun locking_forgets_the_key_and_destroying_removes_the_record() = runBlocking {
        val (s, host) = store()
        s.setPassphrase(good)
        s.lock()
        assertFailsWith<SecretKeyUnavailableException> { s.load() }
        assertEquals(PassphraseResult.FAILED, s.setPassphrase("a different long passphrase"), "no second set-up over an existing one")
        s.destroy()
        assertNull(host.getString(PassphraseGatedSecretKeyStore.WRAP_KEY))
        assertFalse(s.isEnabled)
    }

    // @verifies REQ-PRES-03
    @Test fun the_lock_opens_with_the_passphrase_and_a_non_passphrase_key_refuses() = runBlocking {
        val (s, host) = store()
        s.setPassphrase(good)
        s.lock()
        val lock = PresenceLock(s, { 0L })
        assertEquals(PassphraseResult.WRONG, lock.unlockWithPassphrase("not the passphrase at all"))
        assertEquals(PresenceState.LOCKED, lock.state.value)
        assertEquals(PassphraseResult.OK, lock.unlockWithPassphrase(good))
        assertEquals(PresenceState.UNLOCKED, lock.state.value)
        assertNull(PresenceLock(object : GatedSecretKeyStore by s {}, { 0L }).passphraseKey)
    }

    // @verifies REQ-PRES-10#a
    @Test fun the_record_is_sealed_under_the_device_key_and_a_copy_without_it_opens_nothing() = runBlocking {
        val (s, host) = store()
        s.setPassphrase(good)
        val stored = host.getString(PassphraseGatedSecretKeyStore.WRAP_KEY)!!
        assertTrue(stored.startsWith(PassphraseGatedSecretKeyStore.RECORD_PREFIX))
        for (field in listOf("kdf", "salt", "argon2", "sealed")) assertFalse(stored.contains(field), "the record shows no '$field'")

        // The host's store copied to another machine: the same text, another keystore. The right passphrase opens nothing, and the
        // answer is FAILED, not WRONG, so the copy cannot be used to test a guess either.
        val copy = InMemoryKeyValueStore().also { it.putString(PassphraseGatedSecretKeyStore.WRAP_KEY, stored) }
        val elsewhere = InMemorySecretKeyStore().also { it.store(ByteArray(32) { 7 }) }
        val thief = PassphraseGatedSecretKeyStore(copy, { elsewhere }, random)
        assertEquals(PassphraseResult.FAILED, thief.unlockWithPassphrase(good))
        assertEquals(PassphraseResult.FAILED, thief.unlockWithPassphrase("not the passphrase at all"))
        assertEquals(PassphraseResult.FAILED, PassphraseGatedSecretKeyStore(copy, { InMemorySecretKeyStore() }, random).unlockWithPassphrase(good))
        assertFalse(thief.isUnlocked)
    }

    // @verifies REQ-PRES-10#a
    @Test fun with_no_keystore_nothing_is_set_up_and_a_keystore_that_cannot_be_asked_opens_nothing() = runBlocking {
        val host = InMemoryKeyValueStore()
        assertEquals(PassphraseResult.FAILED, PassphraseGatedSecretKeyStore(host, { null }, random).setPassphrase(good))
        assertNull(host.getString(PassphraseGatedSecretKeyStore.WRAP_KEY), "never written under the passphrase alone")

        val (s, _) = store(host)
        assertEquals(PassphraseResult.OK, s.setPassphrase(good))
        val key = s.load()!!
        val unreachable = object : SecretKeyStore {
            override fun load(): ByteArray? = throw SecretKeyUnavailableException("the keychain is locked")
            override fun store(key: ByteArray) = throw SecretKeyUnavailableException("the keychain is locked")
            override fun destroy() {}
        }
        assertEquals(PassphraseResult.FAILED, PassphraseGatedSecretKeyStore(host, { unreachable }, random).unlockWithPassphrase(good))
        assertNotNull(device.load(), "a keystore that could not be asked has no key made over it")
        val again = PassphraseGatedSecretKeyStore(host, { device }, random)
        assertEquals(PassphraseResult.OK, again.unlockWithPassphrase(good), "the record is untouched and opens once the keystore answers")
        assertContentEquals(key, again.load())
    }

    // @verifies REQ-PRES-10#b
    @Test fun a_record_in_the_clear_is_refused_not_read() = runBlocking {
        // The earlier form: the record as plain JSON, put where a set-up gate's record is.
        val (s, host) = store()
        s.setPassphrase(good)
        val plain = """{"kdf":{"method":"passphrase"},"sealed":"00"}"""
        host.putString(PassphraseGatedSecretKeyStore.WRAP_KEY, plain)
        val second = PassphraseGatedSecretKeyStore(host, { device }, random)
        assertEquals(PassphraseResult.FAILED, second.unlockWithPassphrase(good))
        assertEquals(PassphraseResult.FAILED, second.changePassphrase(good, "another long passphrase"))
        assertEquals(plain, host.getString(PassphraseGatedSecretKeyStore.WRAP_KEY), "nothing is rewritten")
    }
}
