package life.neurone.core.common

import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
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

    // The keys not yet sorted for migration are sealed too: until shown not to be, they are UHDR (CLAUDE.md section 5.1). They include
    // the cervical fault acknowledgement and the onboarding records, which are facts about the person.
    // @verifies SWR-MG-06
    @Test
    fun theKeysNotYetSortedForMigrationAreSealedAndStillReadAsWhatWasWritten() {
        val keys = EncryptedKeyValueStore.uhdrKeys()
        val unsorted = NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["unsorted"]!!.jsonArray.map { it.jsonPrimitive.content }
        assertTrue(unsorted.isNotEmpty() && keys.containsAll(unsorted), "every unsorted key is in the sealed set")

        val inner = InMemoryKeyValueStore()
        val s = EncryptedKeyValueStore(inner, InMemorySecretKeyStore(), keys)
        s.putBoolean("np.onboarding.bipa-accepted", true)
        s.putBoolean("np.setup.first-complete", true)
        s.putString("np.cvns.fault-ledger.last-acknowledged-session", "session-0042")
        s.putString("np.consumable.snooze-counts", "{\"sleeve\":2}")
        for (k in listOf("np.onboarding.bipa-accepted", "np.setup.first-complete", "np.cvns.fault-ledger.last-acknowledged-session", "np.consumable.snooze-counts")) {
            assertTrue(inner.getString(k)!!.startsWith(EncryptedKeyValueStore.PREFIX), "$k is sealed in the host's store")
        }
        assertFalse("session-0042" in inner.getString("np.cvns.fault-ledger.last-acknowledged-session")!!)
        assertTrue(s.getBoolean("np.onboarding.bipa-accepted", false))
        assertEquals("session-0042", s.getString("np.cvns.fault-ledger.last-acknowledged-session"))
        assertEquals("{\"sleeve\":2}", s.getString("np.consumable.snooze-counts"))

        // A key the apps do not store at all is left as it is.
        s.putString("np.not-a-listed-key", "x")
        assertEquals("x", inner.getString("np.not-a-listed-key"))
    }

    // The per-install keys are sealed too (SWR-MG-16): each is something the person's install says, and nothing shows it is free of
    // what that says about the person. They are read back as what was written.
    // @verifies SWR-MG-16#a
    @Test
    fun thePerInstallKeysAreSealedAndStillReadAsWhatWasWritten() {
        val perInstall = NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["perInstall"]!!.jsonArray.map { it.jsonPrimitive.content }
        val keys = EncryptedKeyValueStore.uhdrKeys()
        assertTrue(perInstall.isNotEmpty() && perInstall.all { it in keys }, "every per-install key is in the sealed set")
        val inner = InMemoryKeyValueStore()
        val s = EncryptedKeyValueStore(inner, InMemorySecretKeyStore(), keys)
        s.putString("ble.last-hub-id", "hub-1")
        s.putInt("np.analytics.launch-count", 7)
        s.putBoolean("np.warranty.consent-granted", true)
        s.putString("np.backup.folder", "/Users/a-person/Backups")
        for (k in listOf("ble.last-hub-id", "np.analytics.launch-count", "np.warranty.consent-granted", "np.backup.folder")) {
            assertTrue(inner.getString(k)!!.startsWith(EncryptedKeyValueStore.PREFIX), "$k is sealed in the host's store")
        }
        assertFalse("a-person" in inner.getString("np.backup.folder")!!, "the path is not in the stored value")
        assertEquals("hub-1", s.getString("ble.last-hub-id"))
        assertEquals(7, s.getInt("np.analytics.launch-count"))
        assertTrue(s.getBoolean("np.warranty.consent-granted"))
    }

    // A flag, a number and a text written before the store sealed them are Boolean, Int and String in the host's own store. All three
    // are found and sealed at start-up and read back as the type asked for; a key that is not there is not invented.
    // @verifies SWR-MG-06
    @Test
    fun valuesFromBeforeAreSealedAtStartUpWhateverTypeTheHostKeptThemAs() {
        val inner = InMemoryKeyValueStore().also {
            it.putBoolean("np.onboarding.age-confirmed", true)
            it.putBoolean("np.onboarding.bipa-shown", false)
            it.putInt("np.limits.global", 12)
            it.putString("np.session.history", "old history")
        }
        val s = EncryptedKeyValueStore(inner, InMemorySecretKeyStore(), EncryptedKeyValueStore.uhdrKeys())
        s.migrate()
        for (k in listOf("np.onboarding.age-confirmed", "np.onboarding.bipa-shown", "np.limits.global", "np.session.history")) {
            assertTrue(inner.getString(k)!!.startsWith(EncryptedKeyValueStore.PREFIX), "$k is sealed after start-up")
        }
        assertEquals(true, s.getBoolean("np.onboarding.age-confirmed", false))
        assertEquals(false, s.getBoolean("np.onboarding.bipa-shown", true), "a stored false is not the default")
        assertEquals(12, s.getInt("np.limits.global", 0))
        assertEquals("old history", s.getString("np.session.history"))
        assertNull(inner.getString("np.setup.first-complete"), "a key that is not there is not made")
        s.migrate()
        assertEquals(12, s.getInt("np.limits.global", 0), "a second start-up leaves sealed values alone")
    }

    // @verifies SWR-MG-06
    @Test
    fun aFlagThatAHostKeptAsOneOrZeroReadsAsAFlag() {
        val inner = InMemoryKeyValueStore().also { it.putString("np.setup.first-complete", "1"); it.putString("np.onboarding.consent-shown", "0") }
        val s = EncryptedKeyValueStore(inner, InMemorySecretKeyStore(), EncryptedKeyValueStore.uhdrKeys())
        s.migrate()
        assertEquals(true, s.getBoolean("np.setup.first-complete", false))
        assertEquals(false, s.getBoolean("np.onboarding.consent-shown", true))
    }

    /** A keystore that holds a key and, on this run, cannot be asked for it. */
    private class Unreachable(private val real: SecretKeyStore) : SecretKeyStore {
        var stores = 0
        var reachable = false
        override fun load(): ByteArray? = if (reachable) real.load() else throw SecretKeyUnavailableException("timed out")
        override fun store(key: ByteArray) { stores++; real.store(key) }
        override fun destroy() = real.destroy()
    }

    // @verifies SWR-MG-15#a
    @Test
    fun aKeystoreThatCannotBeAskedNeverGetsANewKeyAndTheRealOneStillOpensTheDataAfterwards() {
        val inner = InMemoryKeyValueStore()
        val real = InMemorySecretKeyStore()
        EncryptedKeyValueStore(inner, real, setOf(history)).putString(history, "history of the person")
        val sealed = inner.getString(history)
        val key = real.load()!!

        val unreachable = Unreachable(real)
        val later = EncryptedKeyValueStore(inner, unreachable, setOf(history))
        assertNull(later.getString(history), "absent while the keystore cannot be asked, never ciphertext")
        assertFailsWith<SecretKeyUnavailableException> { later.putString(history, "new") }
        assertEquals(0, unreachable.stores, "no key was made")
        assertEquals(sealed, inner.getString(history), "the sealed value was not replaced")
        assertTrue(key.contentEquals(real.load()!!), "the real key was not replaced")

        unreachable.reachable = true
        assertEquals("history of the person", later.getString(history), "it opens again when the keystore can be asked")
    }
}

