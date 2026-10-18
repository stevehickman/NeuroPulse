package life.neurone.shared

import androidx.compose.runtime.Composable
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.migration.ObjectStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.migration.BACKUP_FOLDER_KEY
import life.neurone.shared.migration.BackupFolderModel
import life.neurone.shared.migration.FolderPlaces
import life.neurone.shared.migration.FolderProblem
import life.neurone.shared.migration.MemoryObjectStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertSame
import kotlin.test.assertTrue

/** The chosen backup folder: remembered, refused when it must be, checked before a backup depends on it (SWR-MG-10; package 6zz). */
class BackupFolderTest {

    /** A host with folders held in memory: [missing] folders cannot be opened, [readOnly] ones take no file, [refused] ones are the app's own. */
    private class FakeFolders(
        override val defaultFolder: String = "/data/backups",
        var answer: String? = null,
        val missing: MutableSet<String> = mutableSetOf(),
        val readOnly: MutableSet<String> = mutableSetOf(),
        val refused: MutableSet<String> = mutableSetOf(),
    ) : FolderPlaces {
        val stores = mutableMapOf<String, ObjectStore>()
        var asked: String? = null

        override suspend fun choose(current: String): String? { asked = current; return answer }

        override fun placeAt(folder: String): MigrationPlace {
            require(folder !in missing) { "no such folder" }
            val store = stores.getOrPut(folder) { if (folder in readOnly) ReadOnly() else MemoryObjectStore() }
            return MigrationPlace(folder, store, ObjectStoreRoute("local-folder", atomicClaim = true))
        }

        override fun refusal(folder: String): FolderProblem? = if (folder in refused) FolderProblem.HoldsAppData else null
    }

    private class ReadOnly : ObjectStore {
        override suspend fun get(name: String): ByteArray? = null
        override suspend fun put(name: String, bytes: ByteArray) = error("read-only")
        override suspend fun createIfAbsent(name: String, bytes: ByteArray) = error("read-only")
        override suspend fun delete(name: String) {}
        override suspend fun list(prefix: String): List<String> = emptyList()
        override suspend fun freeSpace(): Long? = null
    }

    // @verifies SWR-MG-10
    @Test fun the_default_folder_is_used_until_one_is_chosen_and_a_choice_is_remembered() = runTest {
        val keys = InMemoryKeyValueStore()
        val folders = FakeFolders(answer = "/media/usb/backups")
        val model = BackupFolderModel(folders, keys)
        assertEquals("/data/backups", model.folder.value)
        assertEquals("/data/backups", model.place.value.label)

        model.choose()
        assertEquals("/data/backups", folders.asked, "the dialog starts where the backup is now")
        assertEquals("/media/usb/backups", model.folder.value)
        assertEquals("/media/usb/backups", model.place.value.label)
        assertEquals("/media/usb/backups", keys.getString(BACKUP_FOLDER_KEY))
        assertNull(model.problem.value)

        // The next run starts on the chosen folder.
        assertEquals("/media/usb/backups", BackupFolderModel(folders, keys).place.value.label)

        model.useDefault()
        assertEquals("/data/backups", model.folder.value)
        assertNull(keys.getString(BACKUP_FOLDER_KEY), "the default is the absence of a choice")
    }

    // @verifies SWR-MG-10
    @Test fun a_cancel_changes_nothing() = runTest {
        val keys = InMemoryKeyValueStore()
        val model = BackupFolderModel(FakeFolders(answer = null), keys)
        val before = model.place.value
        model.choose()
        assertSame(before, model.place.value)
        assertNull(keys.getString(BACKUP_FOLDER_KEY))
        assertNull(model.problem.value)
    }

    // @verifies SWR-MG-10
    @Test fun a_folder_the_host_refuses_is_not_used_and_the_reason_is_given() = runTest {
        val keys = InMemoryKeyValueStore()
        val folders = FakeFolders(answer = "/data", refused = mutableSetOf("/data"))
        val model = BackupFolderModel(folders, keys)
        model.choose()
        assertEquals(FolderProblem.HoldsAppData, model.problem.value)
        assertEquals("/data/backups", model.folder.value)
        assertNull(keys.getString(BACKUP_FOLDER_KEY))

        // The next try clears the notice.
        folders.answer = "/media/usb"
        model.choose()
        assertNull(model.problem.value)
        assertEquals("/media/usb", model.folder.value)
    }

    // @verifies SWR-MG-10
    @Test fun a_folder_that_takes_no_file_is_found_now_and_not_in_the_middle_of_a_backup() = runTest {
        val keys = InMemoryKeyValueStore()
        val folders = FakeFolders(answer = "/media/usb", readOnly = mutableSetOf("/media/usb"))
        val model = BackupFolderModel(folders, keys)
        model.choose()
        assertEquals(FolderProblem.NotWritable, model.problem.value)
        assertEquals("/data/backups", model.folder.value, "the earlier folder is still the one in use")
        assertNull(keys.getString(BACKUP_FOLDER_KEY))

        folders.answer = "/gone"
        folders.missing += "/gone"
        model.choose()
        assertEquals(FolderProblem.NotWritable, model.problem.value, "a folder that cannot be opened is the same trouble")
    }

    // @verifies SWR-MG-10
    @Test fun a_saved_folder_that_is_gone_starts_the_app_on_the_default_and_keeps_the_choice() = runTest {
        val keys = InMemoryKeyValueStore().also { it.putString(BACKUP_FOLDER_KEY, "/media/usb") }
        val folders = FakeFolders(missing = mutableSetOf("/media/usb"))
        val model = BackupFolderModel(folders, keys)
        assertEquals("/data/backups", model.folder.value)
        assertEquals(FolderProblem.NotWritable, model.problem.value, "the screen says why")
        assertEquals("/media/usb", keys.getString(BACKUP_FOLDER_KEY), "the drive coming back is enough")

        folders.missing.clear()
        assertEquals("/media/usb", BackupFolderModel(folders, keys).folder.value)
    }

    private fun services(kv: KeyValueStore, folders: FolderPlaces?, fixed: MigrationPlace?) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val folderPlaces = folders
            override val migrationPlace = fixed
            @Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    // @verifies SWR-MG-10
    @Test fun the_screens_place_follows_the_choice_on_a_host_with_folders_and_is_the_fixed_place_otherwise() = runTest {
        val folders = FakeFolders(answer = "/media/usb")
        val app = services(InMemoryKeyValueStore(), folders, fixed = null)
        assertEquals("/data/backups", app.backupPlace.value!!.label)
        app.backupFolder!!.choose()
        assertEquals("/media/usb", app.backupPlace.value!!.label)

        val fixed = MigrationPlace("fixed", MemoryObjectStore(), ObjectStoreRoute("x", atomicClaim = true))
        val plain = services(InMemoryKeyValueStore(), null, fixed)
        assertNull(plain.backupFolder)
        assertSame(fixed, plain.backupPlace.value)
        assertTrue(services(InMemoryKeyValueStore(), null, null).backupPlace.value == null)
    }
}
