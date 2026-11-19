package life.neurone.core.cloudauth

import kotlinx.coroutines.runBlocking
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.ProtectedStorage
import life.neurone.core.common.SecretKeyStore
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.common.StorageProtection
import life.neurone.core.platform.Digests
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The cloud credentials cannot be kept in the clear (`SWR-MG-19` (l), `OI-PRIV-14`): the vault is built only over storage that seals
 * its key or holds it in memory, and a Google client's issued "secret" is sent only by a desktop-type client (`SWR-MG-19` (m)).
 */
class CredentialStoreTests {
    private val keys = { setOf(CredentialStore.KEY) }

    /** A keystore that cannot be asked, as a locked or broken system credential store answers. */
    private class BrokenKeyStore : SecretKeyStore {
        override fun load(): ByteArray? = throw SecretKeyUnavailableException("locked")
        override fun store(key: ByteArray) = throw SecretKeyUnavailableException("locked")
        override fun destroy() = throw SecretKeyUnavailableException("locked")
    }

    /** A host store that records every write, so "nothing was written" is checked and not inferred from a read. */
    private class RecordingStore(private val inner: InMemoryKeyValueStore = InMemoryKeyValueStore()) : KeyValueStore by inner {
        val writes = mutableListOf<String>()
        override fun putString(key: String, value: String) { writes += key; inner.putString(key, value) }
        override fun putInt(key: String, value: Int) { writes += key; inner.putInt(key, value) }
        override fun putBoolean(key: String, value: Boolean) { writes += key; inner.putBoolean(key, value) }
    }

    private fun signIn(vault: CredentialStore): CloudAuth {
        val providers = FakeProviders()
        val auth = CloudAuth(providers, vault, CloudAuthConfig(google = OAuthClientConfig("g", GOOGLE_REDIRECT)), FakeBrowser(providers), CountingRandom(), Digests::sha256, { 1L })
        runBlocking { auth.signIn(CloudService.GOOGLE_DRIVE) }
        return auth
    }

    // @verifies SWR-MG-19#l
    @Test
    fun aHostWithNoKeystoreHasNoVaultAndNothingIsWrittenToItsStore() {
        val host = RecordingStore()
        val storage = ProtectedStorage.open(host, null, keys)
        assertEquals(StorageProtection.UNSEALED, storage.state)
        assertNull(CredentialStore.over(storage), "no vault over a store that would keep the credentials in the clear")
        assertTrue(host.writes.isEmpty())
    }

    // @verifies SWR-MG-19#l
    @Test
    fun aHostThatCannotGetTheListOfSealedKeysHasNoVault() {
        val storage = ProtectedStorage.open(InMemoryKeyValueStore(), InMemorySecretKeyStore()) { error("no core binding") }
        assertEquals(StorageProtection.UNSEALED, storage.state)
        assertNull(CredentialStore.over(storage))
    }

    // @verifies SWR-MG-19#l
    @Test
    fun sealedStorageThatDoesNotSealTheVaultsKeyHasNoVault() {
        val storage = ProtectedStorage.open(InMemoryKeyValueStore(), InMemorySecretKeyStore()) { setOf("np.some.other.key") }
        assertEquals(StorageProtection.SEALED, storage.state)
        assertNull(CredentialStore.over(storage), "sealed storage is not enough: this key must be on its list")
    }

    // @verifies SWR-MG-19#l
    @Test
    fun aDocumentLeftInTheClearIsRemovedOnARunWithNoVaultAndASealedOneIsLeftAlone() {
        val plain = InMemoryKeyValueStore().apply { putString(CredentialStore.KEY, """{"version":1,"accounts":[]}""") }
        assertNull(CredentialStore.over(ProtectedStorage.open(plain, null, keys)))
        assertNull(plain.getString(CredentialStore.KEY), "the plain document is gone")

        // A value an earlier run sealed, on a run that cannot learn which keys are sealed: unreadable here, and not this run's to remove.
        val sealedValue = EncryptedKeyValueStore.PREFIX + "abc"
        val sealed = InMemoryKeyValueStore().apply { putString(CredentialStore.KEY, sealedValue) }
        assertNull(CredentialStore.over(ProtectedStorage.open(sealed, null) { error("no core binding") }))
        assertEquals(sealedValue, sealed.getString(CredentialStore.KEY))
    }

    // @verifies SWR-MG-19#l
    @Test
    fun aLockedRunKeepsTheVaultInMemoryAndWritesNothingToTheHostsStore() {
        // An earlier run sealed an account; this run's keystore cannot be asked.
        val disk = InMemoryKeyValueStore()
        signIn(CredentialStore.over(ProtectedStorage.open(disk, InMemorySecretKeyStore(), keys))!!)
        val sealedBefore = assertNotNull(disk.getString(CredentialStore.KEY))

        val host = RecordingStore(disk)
        val storage = ProtectedStorage.open(host, BrokenKeyStore(), keys)
        assertEquals(StorageProtection.LOCKED, storage.state)
        val vault = assertNotNull(CredentialStore.over(storage))
        assertFalse(vault.durable, "what is signed in to now does not outlive the run")
        val auth = signIn(vault)
        assertEquals(1, runBlocking { auth.accounts() }.size, "the sealed account is not readable; the one signed in to now is")
        assertTrue(host.writes.isEmpty(), "nothing was written to the host's store")
        assertEquals(sealedBefore, disk.getString(CredentialStore.KEY), "the sealed document is as it was")

        vault.clear()
        assertEquals(sealedBefore, disk.getString(CredentialStore.KEY), "clearing the run's vault does not reach the disk")
    }

    // @verifies SWR-MG-19#l
    @Test
    fun aSealedRunKeepsTheVaultSealedAndSaysItIsDurable() {
        val raw = InMemoryKeyValueStore()
        val vault = assertNotNull(CredentialStore.over(sealedStorage(raw)))
        assertTrue(vault.durable)
        signIn(vault)
        val onDisk = assertNotNull(raw.getString(CredentialStore.KEY))
        assertTrue(onDisk.startsWith(EncryptedKeyValueStore.PREFIX))
        assertFalse("refresh-1" in onDisk || "access-1" in onDisk)
    }

    // @verifies SWR-MG-19#m
    @Test
    fun theIssuedSecretIsSentOnlyByAClientWhoseRedirectIsAComputersLoopbackAddress() = runBlocking {
        assertEquals("s", OAuthClientConfig("g", "http://127.0.0.1:9004", clientSecret = "s").secretForTokenCalls)
        assertNull(OAuthClientConfig("g", "https://app.example/oauth/google", clientSecret = "s").secretForTokenCalls, "an Android App Link")
        assertNull(OAuthClientConfig("g", "com.googleusercontent.apps.g:/oauth", clientSecret = "s").secretForTokenCalls, "an iOS scheme")
        assertNull(OAuthClientConfig("g", "http://127.0.0.1:9004").secretForTokenCalls)

        // Set by mistake beside a phone's redirect: the token call carries no secret.
        val rig = Rig(CloudAuthConfig(google = OAuthClientConfig("g", "https://app.example/oauth/google", clientSecret = "s3cret")))
        rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        assertFalse("client_secret" in rig.providers.form(rig.providers.seen.single()))
    }
}
