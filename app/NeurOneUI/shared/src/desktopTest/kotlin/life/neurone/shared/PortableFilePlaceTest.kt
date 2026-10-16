package life.neurone.shared

import kotlinx.coroutines.test.runTest
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.shared.migration.BackupKind
import life.neurone.shared.migration.BackupStep
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.shared.migration.FileTransfer
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.migration.MigrationBackupModel
import life.neurone.shared.migration.MigrationRestoreModel
import life.neurone.shared.migration.PortableFilePlace
import life.neurone.shared.migration.RestoreStep
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** A backup that is a file the person carries, from one phone's save dialog to another's open dialog (SWR-MG-02, SWR-MG-10). */
class PortableFilePlaceTest {
    /** Two dialogs over one "disk": what the first phone saves is what the second opens. */
    private class Disk(var cancel: Boolean = false) : FileTransfer {
        val files = linkedMapOf<String, ByteArray>()
        override suspend fun save(name: String, bytes: ByteArray): Boolean {
            if (cancel) return false
            files[name] = bytes.copyOf()
            return true
        }
        override suspend fun open(): ByteArray? = if (cancel) null else files.values.lastOrNull()?.copyOf()
    }

    // @verifies SWR-MG-10
    @Test fun a_place_that_is_a_file_cannot_promise_an_exclusive_create_so_it_carries_no_migration() {
        val place = PortableFilePlace(Disk()).place
        assertFalse(place.route.atomicClaim)
        assertFalse(MigrationBackupModel(place, InMemoryKeyValueStore()).canMigrate)
        val folder = MigrationPlace("folder", MemoryObjectStore(), ObjectStoreRoute("folder", atomicClaim = true))
        assertTrue(MigrationBackupModel(folder, InMemoryKeyValueStore()).canMigrate)
    }

    // @verifies SWR-MG-02
    // @verifies SWR-MG-03
    @Test fun a_health_data_backup_saved_on_one_phone_is_restored_from_the_file_on_another() = runTest {
        val disk = Disk()
        val old = InMemoryKeyValueStore().also { it.putString("np.session.history", "history of the old phone") }

        val here = PortableFilePlace(disk)
        val backup = MigrationBackupModel(here.place, old)
        backup.chooseKind(BackupKind.UHDR)
        backup.continueToMethod()
        backup.usePassphrase()
        backup.submitPassphrase("a long enough passphrase")
        assertTrue(backup.step.value is BackupStep.Done, backup.step.value.toString())
        assertTrue(here.saveBackup())
        assertEquals(1, disk.files.size)

        val there = PortableFilePlace(disk)
        assertTrue(there.openBackup())
        val fresh = InMemoryKeyValueStore()
        val restore = MigrationRestoreModel(there.place, fresh)
        restore.look()
        assertNotNull(restore.step.value as? RestoreStep.EnterSecret)
        restore.submitSecret("a long enough passphrase")
        assertTrue(restore.step.value is RestoreStep.Done, restore.step.value.toString())
        assertEquals("history of the old phone", fresh.getString("np.session.history"))
    }

    // @verifies SWR-MG-02
    @Test fun a_dialog_the_person_cancels_saves_and_opens_nothing() = runTest {
        val disk = Disk(cancel = true)
        val p = PortableFilePlace(disk)
        assertFalse(p.saveBackup(), "there is no backup to save yet")
        assertFalse(p.openBackup())
        val restore = MigrationRestoreModel(p.place, InMemoryKeyValueStore())
        restore.look()
        assertEquals("no_bundle", (restore.step.value as RestoreStep.Failed).reason)
    }
}
