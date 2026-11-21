package life.neurone.shared

import kotlinx.coroutines.test.runTest
import life.neurone.core.cloudauth.AuthorizationLauncher
import life.neurone.core.cloudauth.CloudAuth
import life.neurone.core.cloudauth.CloudAuthConfig
import life.neurone.core.cloudauth.CloudService
import life.neurone.core.cloudauth.CredentialStore
import life.neurone.core.cloudauth.OAuthClientConfig
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import life.neurone.shared.cloud.BACKUP_CLOUD_ACCOUNT_KEY
import life.neurone.shared.cloud.CloudBackupModel
import life.neurone.shared.migration.BACKUP_FOLDER_KEY
import life.neurone.shared.migration.BackupFolderModel
import life.neurone.shared.migration.FolderPlaces
import life.neurone.shared.migration.FolderProblem
import life.neurone.shared.migration.MemoryObjectStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The chosen backup place under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-19`). The choice is sealed: a locked read of it is "absent",
 * which read as the default place, so an automatic backup went there instead of where the person chose and the model never looked again.
 */
class BackupChoicePresenceTest {

    private class Folders(override val defaultFolder: String? = "/data/backups") : FolderPlaces {
        override suspend fun choose(current: String): String? = null
        override fun placeAt(folder: String) = MigrationPlace(folder, MemoryObjectStore(), ObjectStoreRoute("local-folder", atomicClaim = true))
        override fun refusal(folder: String): FolderProblem? = null
    }

    // @verifies REQ-PRES-01#d
    @Test fun the_chosen_folder_is_read_at_the_unlock_and_never_taken_for_the_default_while_locked() {
        val keys = InMemoryKeyValueStore().apply { putString(BACKUP_FOLDER_KEY, "/usb/backups") }
        var unlocked = false
        val model = BackupFolderModel(Folders(), keys) { unlocked }

        assertNull(model.folder.value, "locked, no folder and no place: nothing is written to the default by mistake")
        assertNull(model.place.value)

        unlocked = true
        model.reload()

        assertEquals("/usb/backups", model.folder.value, "the person's choice, not the default")
        assertEquals("/usb/backups", model.place.value!!.label)

        model.forget()
        assertNull(model.folder.value)
        assertNull(model.place.value)
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_folder_cannot_be_chosen_while_locked_and_the_sealed_choice_survives() = runTest {
        val keys = InMemoryKeyValueStore().apply { putString(BACKUP_FOLDER_KEY, "/usb/backups") }
        val model = BackupFolderModel(Folders(), keys) { false }

        model.useDefault()

        assertEquals("/usb/backups", keys.getString(BACKUP_FOLDER_KEY))
    }

    private class Fakes {
        val transport = object : HttpTransport {
            override suspend fun post(request: HttpRequest): HttpResponse? {
                val body = if (request.url.endsWith("/token")) {
                    """{"access_token":"access-token-1","refresh_token":"refresh-token-1","expires_in":3600}"""
                } else if ("revoke" in request.url) "{}" else """{"files":[]}"""
                return HttpResponse(200, emptyMap(), body.encodeToByteArray())
            }
        }
        val launcher = object : AuthorizationLauncher {
            override suspend fun authorize(authorizationUrl: String, redirectUri: String): String =
                "$redirectUri?code=c&state=${Regex("[?&]state=([^&]+)").find(authorizationUrl)!!.groupValues[1]}"
        }
    }

    // @verifies REQ-PRES-01#d
    @Test fun the_chosen_account_is_read_at_the_unlock_and_the_place_made_from_it() = runTest {
        val f = Fakes()
        val auth = CloudAuth(f.transport, CredentialStore.inMemory(), CloudAuthConfig(google = OAuthClientConfig("g-client", "http://127.0.0.1:53682")), f.launcher)
        val drive = auth.signIn(CloudService.GOOGLE_DRIVE)
        val keys = InMemoryKeyValueStore().apply { putString(BACKUP_CLOUD_ACCOUNT_KEY, drive.id) }
        var unlocked = false
        val model = CloudBackupModel(auth, f.transport, keys) { unlocked }

        assertNull(model.selected.value, "locked, nothing is chosen: not \"this device\" as a decision, only nothing known")
        model.refresh()
        assertNull(model.place.value)
        assertTrue(!model.choose(drive.id), "a choice is not made while locked")
        assertEquals(drive.id, keys.getString(BACKUP_CLOUD_ACCOUNT_KEY), "the sealed choice is untouched")

        unlocked = true
        model.reload()

        assertEquals(drive.id, model.selected.value)
        assertEquals("Google Drive", assertNotNull(model.place.value).label)

        model.forget()
        assertNull(model.selected.value)
        assertNull(model.place.value)
    }
}
