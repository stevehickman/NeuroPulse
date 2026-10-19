package life.neurone.core.common

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * A run that cannot use the host's keystore must not downgrade what an earlier run sealed (desktop audit item 1, SWR-MG-13): it
 * neither reads the ciphertext as if it were the data nor writes plaintext over it.
 */
class ProtectedStorageTests {
    private val history = "np.session.history"
    private val consent = "np.consent.research"
    private val plain = "ble.last-hub-id"
    private val keys = setOf(history, consent)

    /** A keystore that is there on the run that sealed and is not there on this one. */
    private object BrokenKeystore : SecretKeyStore {
        override fun load(): ByteArray? = throw IllegalStateException("the credential tool timed out")
        override fun store(key: ByteArray) = throw IllegalStateException("the credential tool timed out")
        override fun destroy() = throw IllegalStateException("the credential tool timed out")
    }

    /** A host store that already holds data sealed by an earlier run, and the key that opens it. */
    private fun sealedByAnEarlierRun(): Triple<InMemoryKeyValueStore, InMemorySecretKeyStore, String> {
        val host = InMemoryKeyValueStore()
        val secrets = InMemorySecretKeyStore()
        val first = ProtectedStorage.open(host, secrets) { keys }
        first.store.putString(history, "history of the person")
        first.store.putBoolean(consent, true)
        return Triple(host, secrets, host.getString(history)!!)
    }

    // @verifies SWR-MG-13#a
    @Test
    fun withSealedDataOnDiskAndNoKeystoreTheRunIsLockedAndTheDataIsNeitherReadAsCiphertextNorOverwritten() {
        val (host, _, sealedBefore) = sealedByAnEarlierRun()
        val consentBefore = host.getString(consent)!!

        val locked = ProtectedStorage.open(host, null) { keys }

        assertEquals(StorageProtection.LOCKED, locked.state)
        assertNull(locked.sealing, "a locked run has no key to destroy")
        assertNull(locked.store.getString(history), "the ciphertext must not be read as the history")
        assertFalse(locked.store.getBoolean(consent, false), "consent reads as never asked, not as a garbled value")

        locked.store.putString(history, "written on the locked run")
        locked.store.putBoolean(consent, false)
        locked.store.remove(history)
        assertEquals(sealedBefore, host.getString(history), "the sealed history is byte for byte what it was")
        assertEquals(consentBefore, host.getString(consent), "so is the sealed consent")
    }

    // @verifies SWR-MG-13#a
    @Test
    fun whatIsDoneOnALockedRunIsHeldForThatRunAndTheSealedDataOpensAgainWhenTheKeystoreIsBack() {
        val (host, secrets, _) = sealedByAnEarlierRun()

        val locked = ProtectedStorage.open(host, null) { keys }
        locked.store.putString(history, "written on the locked run")
        assertEquals("written on the locked run", locked.store.getString(history), "the run can still work in memory")

        val later = ProtectedStorage.open(host, secrets) { keys }
        assertEquals(StorageProtection.SEALED, later.state)
        assertEquals("history of the person", later.store.getString(history), "the earlier state is intact")
        assertTrue(later.store.getBoolean(consent, false))
    }

    // @verifies SWR-MG-13#a
    @Test
    fun aKeystoreThatFailsOnThisRunLocksTheRunTooAndSealedAndOtherKeysBehaveAsBefore() {
        val (host, _, sealedBefore) = sealedByAnEarlierRun()

        val locked = ProtectedStorage.open(host, BrokenKeystore) { keys }

        assertEquals(StorageProtection.LOCKED, locked.state)
        locked.store.putString(plain, "hub-1")
        assertEquals("hub-1", host.getString(plain), "a key that is not protected still goes to the host")
        locked.store.putString(history, "x")
        assertEquals(sealedBefore, host.getString(history))
    }

    // @verifies SWR-MG-08
    @Test
    fun aKeystoreThatHasNoKeyIsStillBuiltSealedSoAnInterruptedClearCanResume() {
        // The clear destroys the key first. A clear interrupted after that leaves sealed values and no key, and the next launch
        // must still get the sealing store and its clear actions to finish the job, not a locked store without them.
        val (host, secrets, _) = sealedByAnEarlierRun()
        secrets.destroy()

        val open = ProtectedStorage.open(host, secrets) { keys }

        assertEquals(StorageProtection.SEALED, open.state)
        assertTrue(open.sealing != null, "the clear's key destruction needs the sealing store")
    }

    // @verifies SWR-MG-13#b
    @Test
    fun aHostThatNeverSealedAnythingAndHasNoKeystoreKeepsItsDataAsTheHostStoresIt() {
        val host = InMemoryKeyValueStore()

        val open = ProtectedStorage.open(host, null) { keys }

        assertEquals(StorageProtection.UNSEALED, open.state)
        open.store.putString(history, "history of the person")
        assertEquals("history of the person", host.getString(history), "as before: in the host's own store")
    }

    // @verifies SWR-MG-13#b
    @Test
    fun aListOfKeysThatCannotBeGotLeavesTheHostsStoreAsItIs() {
        val host = InMemoryKeyValueStore().also { it.putString(history, "h") }

        val open = ProtectedStorage.open(host, InMemorySecretKeyStore()) { throw IllegalStateException("no core binding") }

        assertEquals(StorageProtection.UNSEALED, open.state)
        assertEquals("h", open.store.getString(history))
    }

    // @verifies SWR-MG-13#a
    @Test
    fun aValueThatIsNotAStringIsNotASealedValue() {
        val host = InMemoryKeyValueStore().also { it.putInt(history, 7) }
        assertFalse(ProtectedStorage.holdsSealedValue(host, keys))
        assertTrue(ProtectedStorage.holdsSealedValue(sealedByAnEarlierRun().first, keys))
    }
}
