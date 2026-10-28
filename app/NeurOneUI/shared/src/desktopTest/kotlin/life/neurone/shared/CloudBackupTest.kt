package life.neurone.shared

import androidx.compose.runtime.Composable
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.test.runTest
import life.neurone.core.cloud.CloudStoreException
import life.neurone.core.cloudauth.AuthorizationLauncher
import life.neurone.core.cloudauth.CloudAuth
import life.neurone.core.cloudauth.CloudAuthConfig
import life.neurone.core.cloudauth.CloudService
import life.neurone.core.cloudauth.CredentialStore
import life.neurone.core.cloudauth.MicrosoftClientConfig
import life.neurone.core.cloudauth.OAuthClientConfig
import life.neurone.core.cloudauth.TargetField
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.ObjectStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.cloud.BACKUP_CLOUD_ACCOUNT_KEY
import life.neurone.shared.cloud.BACKUP_CLOUD_TARGET_KEY
import life.neurone.shared.cloud.CloudBackupModel
import life.neurone.shared.migration.BackupKind
import life.neurone.shared.migration.BackupStep
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.migration.WaveformBackup
import life.neurone.shared.migration.runBackup
import life.neurone.shared.session.UhdrFolder
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertSame
import kotlin.test.assertTrue

/** The backup kept in a cloud account the person signed in to (SWR-MG-21), over a real [CloudAuth] with a stand-in provider and service. */
class CloudBackupTest {
    private val google = OAuthClientConfig("g-client", "http://127.0.0.1:53682")
    private val microsoft = MicrosoftClientConfig("m-client", "app.test.oauth:/redirect")

    private class Fakes {
        val api = mutableListOf<HttpRequest>()
        val transport = object : HttpTransport {
            override suspend fun post(request: HttpRequest): HttpResponse? {
                val body = when {
                    request.url.endsWith("/token") ->
                        """{"access_token":"access-token-1","refresh_token":"refresh-token-1","expires_in":3600}"""
                    "revoke" in request.url -> "{}"
                    else -> {
                        api += request
                        if ("googleapis.com" in request.url) """{"files":[]}""" else """{"value":[]}"""
                    }
                }
                return HttpResponse(200, emptyMap(), body.encodeToByteArray())
            }
        }
        val launcher = object : AuthorizationLauncher {
            override suspend fun authorize(authorizationUrl: String, redirectUri: String): String =
                "$redirectUri?code=c&state=${Regex("[?&]state=([^&]+)").find(authorizationUrl)!!.groupValues[1]}"
        }
    }

    private fun auth(f: Fakes, kv: InMemoryKeyValueStore) =
        CloudAuth(f.transport, CredentialStore(kv), CloudAuthConfig(google = google, microsoft = microsoft), f.launcher)

    // @verifies SWR-MG-21#a
    @Test fun every_signed_in_account_is_listed_and_a_choice_makes_a_place_that_is_remembered() = runTest {
        val f = Fakes(); val kv = InMemoryKeyValueStore(); val a = auth(f, kv)
        val drive = a.signIn(CloudService.GOOGLE_DRIVE)
        val gcs = a.signIn(CloudService.GOOGLE_CLOUD_STORAGE)
        val s3 = a.addAwsKeys("AKIAEXAMPLE", "secret")
        val m = CloudBackupModel(a, f.transport, kv)
        m.refresh()
        assertEquals(setOf(drive.id, gcs.id, s3.id), m.destinations.value.map { it.accountId }.toSet())
        assertEquals(emptyList(), m.destinations.value.first { it.accountId == drive.id }.fields, "Drive needs nothing more")
        assertEquals(listOf(TargetField.GCS_BUCKET), m.destinations.value.first { it.accountId == gcs.id }.fields)
        assertEquals(listOf(TargetField.BUCKET, TargetField.REGION, TargetField.ENDPOINT), m.destinations.value.first { it.accountId == s3.id }.fields)
        assertNull(m.place.value, "this device until a choice is made")

        assertTrue(m.choose(drive.id))
        val place = assertNotNull(m.place.value)
        assertEquals("Google Drive", place.label)
        assertEquals(drive.id, kv.getString(BACKUP_CLOUD_ACCOUNT_KEY))
        assertNull(kv.getString(BACKUP_CLOUD_TARGET_KEY), "Drive keeps no target")
        place.store.list("")
        assertEquals("Bearer access-token-1", f.api.last().headers["Authorization"], "the place is a Drive store with the account's token")

        // A new run reads the choice and makes the place again.
        val later = CloudBackupModel(a, f.transport, kv)
        assertNull(later.place.value)
        later.refresh()
        assertEquals("Google Drive", later.place.value?.label)
    }

    // @verifies SWR-MG-22#c
    // @verifies SWR-MG-22#e
    @Test fun a_bucket_service_is_chosen_only_with_a_valid_name_which_is_remembered_with_the_choice() = runTest {
        val f = Fakes(); val kv = InMemoryKeyValueStore(); val a = auth(f, kv)
        val gcs = a.signIn(CloudService.GOOGLE_CLOUD_STORAGE)
        val m = CloudBackupModel(a, f.transport, kv)
        m.refresh()

        assertFalse(m.choose(gcs.id), "no bucket named")
        assertFalse(m.choose(gcs.id, mapOf(TargetField.GCS_BUCKET to "Bad Bucket/..")), "not a plain name")
        assertNull(m.selected.value); assertNull(kv.getString(BACKUP_CLOUD_ACCOUNT_KEY)); assertNull(m.place.value)

        assertTrue(m.choose(gcs.id, mapOf(TargetField.GCS_BUCKET to "neurone-backups")))
        assertEquals("Google Cloud Storage", assertNotNull(m.place.value).label)
        m.place.value!!.store.list("")
        assertTrue("neurone-backups" in f.api.last().url, f.api.last().url)

        val later = CloudBackupModel(a, f.transport, kv)
        later.refresh()
        assertEquals("neurone-backups", later.target.value?.values?.get(TargetField.GCS_BUCKET), "the name came back with the choice")
        assertNotNull(later.place.value)

        // A choice whose name is lost or unreadable does not stand: this device again, not a store at a guessed place.
        kv.putString(BACKUP_CLOUD_TARGET_KEY, "not json")
        val broken = CloudBackupModel(a, f.transport, kv)
        broken.refresh()
        assertNull(broken.selected.value); assertNull(broken.place.value); assertNull(kv.getString(BACKUP_CLOUD_ACCOUNT_KEY))

        val s3 = a.addAwsKeys("AKIAEXAMPLE", "secret")
        assertTrue(m.choose(s3.id, mapOf(TargetField.BUCKET to "my-backups", TargetField.REGION to "eu-west-2")))
        assertEquals("Amazon S3", m.place.value?.label)
        assertTrue(m.choose(s3.id, mapOf(TargetField.BUCKET to "my-backups", TargetField.REGION to "auto", TargetField.ENDPOINT to "acct.r2.example.com")))
        assertEquals("acct.r2.example.com", m.place.value?.label, "a store the person named is shown by its host")
        assertFalse(m.choose(s3.id, mapOf(TargetField.BUCKET to "my-backups", TargetField.REGION to "auto", TargetField.ENDPOINT to "https://evil.example")), "a refused name changes nothing")
        assertEquals("acct.r2.example.com", m.place.value?.label)
        assertFalse(m.place.value!!.route.atomicClaim, "no cloud route carries a migration")
        assertFalse(m.place.value!!.carriesRecordings)
    }

    // @verifies SWR-MG-21#c
    @Test fun a_cloud_place_carries_a_backup_and_never_a_migration_or_recordings() = runTest {
        val f = Fakes(); val kv = InMemoryKeyValueStore(); val a = auth(f, kv)
        val onedrive = a.signIn(CloudService.ONEDRIVE)
        val m = CloudBackupModel(a, f.transport, kv)
        m.choose(onedrive.id)
        val place = assertNotNull(m.place.value)
        assertFalse(place.route.atomicClaim, "no exclusive create is promised, so the core withholds it from a migration")
        assertFalse(place.carriesRecordings)
        assertEquals("cloud-onedrive", place.route.id)
    }

    // @verifies SWR-MG-21#d
    @Test fun the_recordings_stay_off_a_place_that_does_not_carry_them() = runTest {
        class Folder : UhdrFolder {
            override val store: ObjectStore = MemoryObjectStore().also { s -> kotlinx.coroutines.runBlocking { s.put("session_1.edf", ByteArray(70_000) { it.toByte() }) } }
            override fun protect(name: String) {}
        }
        val keys = InMemoryKeyValueStore()
        val secret = BundleKeyDerivation.newRecoveryCode(SystemRandomSource).secret
        val route = ObjectStoreRoute("t", atomicClaim = false)
        val withRecordings = runBackup(MigrationPlace("local", MemoryObjectStore(), route), keys, emptyList(), BackupKind.UHDR, BundleKeyMethod.RECOVERY_CODE, secret, waveforms = WaveformBackup(Folder()))
        assertNotNull(assertIs<BackupStep.Done>(withRecordings).waveforms, "a place that carries them gets them")
        val cloud = MigrationPlace("cloud", MemoryObjectStore(), route, carriesRecordings = false)
        val without = runBackup(cloud, keys, emptyList(), BackupKind.UHDR, BundleKeyMethod.RECOVERY_CODE, secret, waveforms = WaveformBackup(Folder()))
        assertNull(assertIs<BackupStep.Done>(without).waveforms, "a cloud place gets the bundle alone")
        assertTrue(cloud.store.list("wf-").isEmpty())
    }

    // @verifies SWR-MG-21#a
    @Test fun an_account_that_is_not_listed_or_whose_sign_in_was_ended_cannot_be_chosen() = runTest {
        val f = Fakes(); val kv = InMemoryKeyValueStore(); val a = auth(f, kv)
        val drive = a.signIn(CloudService.GOOGLE_DRIVE)
        val m = CloudBackupModel(a, f.transport, kv)
        m.choose("no-such-account")
        assertNull(m.selected.value)
        a.markNeedsSignIn(drive.id)
        m.choose(drive.id)
        assertNull(m.selected.value, "sign in again first")
        assertNull(kv.getString(BACKUP_CLOUD_ACCOUNT_KEY))
        assertTrue(m.destinations.value.single().needsSignIn)
    }

    // @verifies SWR-MG-21#e
    @Test fun signing_out_of_the_chosen_account_returns_to_this_device_but_an_ended_sign_in_keeps_the_cloud_place_and_fails_visibly() = runTest {
        val f = Fakes(); val kv = InMemoryKeyValueStore(); val a = auth(f, kv)
        val drive = a.signIn(CloudService.GOOGLE_DRIVE)
        val m = CloudBackupModel(a, f.transport, kv)
        m.choose(drive.id)

        // The provider ends the sign-in: the place stays, and using it fails with the sign-in status. The backup does not move itself to this device.
        a.markNeedsSignIn(drive.id)
        m.refresh()
        val place = assertNotNull(m.place.value)
        assertEquals(drive.id, m.selected.value)
        val e = assertFailsWith<CloudStoreException> { place.store.list("") }
        assertEquals(401, e.status)

        // Signing out removes the account, and the choice goes with it.
        a.signOut(drive.id)
        m.refresh()
        assertNull(m.selected.value)
        assertNull(m.place.value)
        assertNull(kv.getString(BACKUP_CLOUD_ACCOUNT_KEY))
    }

    // @verifies SWR-MG-21#a
    @Test fun the_apps_backup_place_is_the_chosen_cloud_place_and_this_devices_when_none_is_chosen() = runTest {
        val f = Fakes(); val kv = InMemoryKeyValueStore()
        val local = MigrationPlace("this device", MemoryObjectStore(), ObjectStoreRoute("host", atomicClaim = true))
        val services = AppServices(
            platform = object : PlatformServices {
                override val keyValueStore = kv
                override val bleCentral = UnavailableBleCentral()
                override val protocolSigner = Ed25519ProtocolSigner()
                override val analyticsBackend = NoOpAnalyticsBackend()
                override val httpTransport: HttpTransport? = f.transport
                override val authorizationLauncher: AuthorizationLauncher? = f.launcher
                override val migrationPlace: MigrationPlace? = local
                @Composable
                override fun rememberBleConnectAction(): () -> Unit = {}
            },
            scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
            cloudAuthConfig = CloudAuthConfig(google = google),
        )
        val cloud = assertNotNull(services.cloudBackup)
        assertSame(local, services.backupPlace.value)

        val account = services.cloudAuth!!.signIn(CloudService.GOOGLE_DRIVE)
        cloud.choose(account.id)
        assertEquals("Google Drive", services.backupPlace.value?.label)

        cloud.useDevice()
        assertSame(local, services.backupPlace.value, "back to this device")

        // Signing out through the accounts screen's model drops a chosen account too.
        cloud.choose(account.id)
        services.cloudAccounts!!.refresh()
        services.cloudAccounts!!.signOut(CloudService.GOOGLE_DRIVE)
        assertSame(local, services.backupPlace.value)
        assertNull(cloud.selected.value)
    }
}
