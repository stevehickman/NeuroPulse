package life.neurone.core.common

import kotlinx.coroutines.runBlocking
import life.neurone.core.data.RandomSource
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The passphrase fallback of the presence gate (REQ-PRES-06, -01, -02): wrap, unlock, wrong passphrase, change, and what is on disk. */
class PassphraseGatedSecretKeyStoreTests {
    private val good = "correct horse battery staple"

    private var counter = 0
    private val random = RandomSource { n -> ByteArray(n) { (++counter and 0xff).toByte() } }

    private fun store(host: KeyValueStore = InMemoryKeyValueStore()) = PassphraseGatedSecretKeyStore(host, random) to host

    // @verifies REQ-PRES-06
    @Test fun a_passphrase_sets_the_gate_up_and_opens_it_again_on_a_new_run() = runBlocking {
        val (first, host) = store()
        assertFalse(first.isEnabled)
        assertEquals(PassphraseResult.OK, first.setPassphrase(good))
        assertTrue(first.isEnabled)
        val key = first.load()!!
        assertEquals(32, key.size)

        val second = PassphraseGatedSecretKeyStore(host, random)
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
        val second = PassphraseGatedSecretKeyStore(host, random)
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

        val second = PassphraseGatedSecretKeyStore(host, random)
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
}
