package life.neurone.core.common

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The app's UHDR-class state, sealed under a key the platform keeps, and unreadable once the key is destroyed (SWR-MG-06). */
class EncryptedKeyValueStoreTests {
    private val history = "np.session.history"
    private val limits = "np.limits.global"
    private val plain = "ble.last-hub-id"

    private fun store(inner: KeyValueStore = InMemoryKeyValueStore(), secrets: SecretKeyStore = InMemorySecretKeyStore()) =
        Triple(inner, secrets, EncryptedKeyValueStore(inner, secrets, setOf(history, limits)))

    // @verifies SWR-MG-06
    @Test
    fun aProtectedValueIsSealedInTheStoreAndReadBackWhole() {
        val (inner, _, s) = store()
        s.putString(history, "history of the person")
        val raw = inner.getString(history)!!
        assertTrue(raw.startsWith(EncryptedKeyValueStore.PREFIX))
        assertFalse("history of the person" in raw, "the text is in the stored value")
        assertEquals("history of the person", s.getString(history))
    }

    // @verifies SWR-MG-06
    @Test
    fun aKeyThatIsNotProtectedIsLeftAsItIs() {
        val (inner, _, s) = store()
        s.putString(plain, "hub-1")
        s.putBoolean("np.onboarding.age-confirmed", true)
        assertEquals("hub-1", inner.getString(plain))
        assertTrue(inner.getBoolean("np.onboarding.age-confirmed"))
    }

    // @verifies SWR-MG-06
    @Test
    fun numbersAndFlagsUnderAProtectedKeyAreSealedToo() {
        val (inner, _, s) = store()
        s.putInt(limits, 42)
        assertTrue(inner.getString(limits)!!.startsWith(EncryptedKeyValueStore.PREFIX))
        assertEquals(42, s.getInt(limits, 0))
        s.putBoolean(history, true)
        assertTrue(s.getBoolean(history, false))
        assertEquals(7, s.getInt("np.absent-and-protected-is-not-listed", 7))
    }

    // @verifies SWR-MG-06
    @Test
    fun aSealedValueMovedUnderAnotherNameDoesNotOpen() {
        val (inner, _, s) = store()
        s.putString(history, "secret")
        inner.putString(limits, inner.getString(history)!!)
        assertNull(s.getString(limits))
    }

    // @verifies SWR-MG-06
    @Test
    fun aValueFromBeforeTheStoreExistedIsReadAsItIsAndSealedByMigrate() {
        val inner = InMemoryKeyValueStore().also { it.putString(history, "written before") }
        val (_, secrets, s) = store(inner)
        assertEquals("written before", s.getString(history))
        assertNull(secrets.load(), "no key is made until a value is sealed")
        s.migrate()
        assertTrue(inner.getString(history)!!.startsWith(EncryptedKeyValueStore.PREFIX))
        assertEquals("written before", s.getString(history))
        assertNotEquals(null, secrets.load())
    }

    // @verifies SWR-MG-06
    @Test
    fun theKeyOutlivesTheStoreObjectSoTheDataReadsAfterARestart() {
        val inner = InMemoryKeyValueStore()
        val secrets = InMemorySecretKeyStore()
        EncryptedKeyValueStore(inner, secrets, setOf(history)).putString(history, "kept")
        assertEquals("kept", EncryptedKeyValueStore(inner, secrets, setOf(history)).getString(history))
    }

    // @verifies SWR-MG-06
    @Test
    fun afterTheKeyIsDestroyedWhatItSealedCannotBeReadAndTheAppStartsAgainUnderANewKey() {
        val (inner, secrets, s) = store()
        s.putString(history, "old data")
        val sealedBefore = inner.getString(history)!!
        s.destroyKey()
        assertNull(secrets.load(), "the keystore no longer holds the key")
        assertNull(s.getString(history), "the data is unreadable without it")
        // A new write makes a new key; the old sealed value stays unreadable even if it is put back.
        s.putString(limits, "new data")
        assertEquals("new data", s.getString(limits))
        inner.putString(history, sealedBefore)
        assertNull(s.getString(history), "the old value does not open under the new key")
    }

    // @verifies SWR-MG-06
    @Test
    fun theSealedSetTheAppUsesCoversTheSessionHistoryAndTheShdrDeviceToken() {
        val keys = EncryptedKeyValueStore.uhdrKeys() + EncryptedKeyValueStore.SHDR_DEVICE_TOKEN_KEY
        val inner = InMemoryKeyValueStore()
        val s = EncryptedKeyValueStore(inner, InMemorySecretKeyStore(), keys)
        val token = "cd".repeat(32)
        s.putString(EncryptedKeyValueStore.SHDR_DEVICE_TOKEN_KEY, token)
        assertTrue(inner.getString(EncryptedKeyValueStore.SHDR_DEVICE_TOKEN_KEY)!!.startsWith(EncryptedKeyValueStore.PREFIX))
        assertFalse(token in inner.getString(EncryptedKeyValueStore.SHDR_DEVICE_TOKEN_KEY)!!, "the token is in the stored value")
        assertEquals(token, s.getString(EncryptedKeyValueStore.SHDR_DEVICE_TOKEN_KEY))
        assertTrue(history in keys, "UHDR-class state stays sealed")
    }
}
