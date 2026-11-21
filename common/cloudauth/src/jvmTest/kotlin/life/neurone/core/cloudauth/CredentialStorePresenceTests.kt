package life.neurone.core.cloudauth

import kotlinx.coroutines.runBlocking
import life.neurone.core.common.GatedSecretKeyStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.ProtectedStorage
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.platform.Digests
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * The cloud credentials under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-19`). Every operation loads the whole vault and saves it
 * whole, so one run on a locked read ("absent", which reads as no accounts) used to replace every sealed account at the unlock.
 */
class CredentialStorePresenceTests {

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

    private class Install {
        val host = InMemoryKeyValueStore()
        private val deviceSecrets = InMemorySecretKeyStore()
        private val gated = Gated()

        inner class Run {
            val lock = PresenceLock(gated.also { it.lock() }, { 0L })
            private val storage = ProtectedStorage.openGated(
                host, deviceSecrets, gated, lock,
                protectedKeys = { setOf(CredentialStore.KEY) },
                deviceTierKeys = { emptySet() },
            )
            val auth: CloudAuth = run {
                val providers = FakeProviders()
                CloudAuth(
                    providers, CredentialStore.over(storage)!!, CloudAuthConfig(google = OAuthClientConfig("g", GOOGLE_REDIRECT)),
                    FakeBrowser(providers), CountingRandom(), Digests::sha256, { 1L },
                )
            }
            fun unlock() = runBlocking { assertTrue(lock.unlock("Unlock your health data")) }
        }
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_locked_run_neither_lists_nor_replaces_the_sealed_accounts() = runBlocking {
        val install = Install()
        val first = install.Run()
        first.unlock()
        val aws = first.auth.addAwsKeys("AKIAEXAMPLE", "secret")
        first.lock.lock()

        val run = install.Run() // a cold start: locked
        assertEquals(CloudAuthProblem.STORE_UNREADABLE, assertFailsWith<CloudAuthException> { run.auth.accounts() }.problem)
        assertFailsWith<CloudAuthException> { run.auth.markNeedsSignIn(aws.id) }
        assertFailsWith<CloudAuthException> { run.auth.addAwsKeys("AKIAOTHER", "other") }

        run.unlock()

        assertEquals(listOf(aws.id), run.auth.accounts().map { it.id }, "the sealed account is whole, with nothing added and nothing marked")
        assertTrue(!run.auth.accounts().single().needsSignIn)
    }

    // @verifies REQ-PRES-01#d
    @Test fun an_account_added_after_the_unlock_joins_the_sealed_one() = runBlocking {
        val install = Install()
        val first = install.Run()
        first.unlock()
        val one = first.auth.addAwsKeys("AKIAEXAMPLE", "secret")
        first.lock.lock()

        val run = install.Run()
        run.unlock()
        val two = run.auth.addAwsKeys("AKIAOTHER", "other")

        assertEquals(setOf(one.id, two.id), run.auth.accounts().map { it.id }.toSet())
    }
}
