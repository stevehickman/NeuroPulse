package life.neurone.shared

import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.migration.ClearActions
import life.neurone.core.migration.ObjectStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.npps.NppsCore
import life.neurone.shared.migration.BackupKind
import life.neurone.shared.migration.BackupStep
import life.neurone.shared.migration.ClearStep
import life.neurone.shared.migration.MessageGroup
import life.neurone.shared.migration.MigrationBackupModel
import life.neurone.shared.migration.MigrationClearModel
import life.neurone.shared.migration.MigrationRestoreModel
import life.neurone.shared.migration.RestoreStep
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The restore and clear screens' steps, without Compose: what they ask the core, and in what order (SWR-MG-03 to SWR-MG-09). */
class MigrationRestoreClearTest {
    private class MemoryStore : ObjectStore {
        val objects = linkedMapOf<String, ByteArray>()
        override suspend fun get(name: String) = objects[name]?.copyOf()
        override suspend fun put(name: String, bytes: ByteArray) { objects[name] = bytes.copyOf() }
        override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean {
            if (name in objects) return false
            objects[name] = bytes.copyOf()
            return true
        }
        override suspend fun delete(name: String) { objects.remove(name) }
        override suspend fun list(prefix: String) = objects.keys.filter { it.startsWith(prefix) }
        override suspend fun freeSpace(): Long? = 1L shl 30
    }

    private class RecordingActions : ClearActions {
        val ran = mutableListOf<String>()
        override suspend fun run(step: String): Boolean { ran += step; return true }
    }

    private val route = ObjectStoreRoute("test", atomicClaim = true)
    private val passphrase = "a long enough passphrase"

    private fun place(store: ObjectStore) = MigrationPlace("test place", store, route)

    private fun filled(): InMemoryKeyValueStore {
        val carried = NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["carried"]!!.jsonObject
        return InMemoryKeyValueStore().also { kv ->
            carried.values.flatMap { v -> v.jsonArray.map { it.jsonPrimitive.content } }.forEach { kv.putString(it, "value-of-$it") }
        }
    }

    private suspend fun backUp(store: ObjectStore, kv: InMemoryKeyValueStore, secret: String = passphrase) {
        val m = MigrationBackupModel(place(store), kv)
        m.chooseKind(BackupKind.MIGRATE)
        m.continueToMethod()
        m.usePassphrase()
        m.submitPassphrase(secret)
        assertTrue(m.step.value is BackupStep.Done, "the backup was not made: ${m.step.value}")
    }

    private suspend fun restoreOnto(store: ObjectStore, kv: InMemoryKeyValueStore, secret: String = passphrase): MigrationRestoreModel {
        val m = MigrationRestoreModel(place(store), kv)
        m.look()
        m.submitSecret(secret)
        return m
    }

    // @verifies SWR-MG-05
    @Test fun the_restore_screen_reads_the_header_before_it_asks_for_anything() = runTest {
        val store = MemoryStore()
        backUp(store, filled())
        val m = MigrationRestoreModel(place(store), InMemoryKeyValueStore())
        assertEquals(RestoreStep.Looking, m.step.value)
        m.look()
        val s = m.step.value as RestoreStep.EnterSecret
        assertEquals(BackupKind.MIGRATE, s.info.kind)
        assertEquals(life.neurone.core.data.BundleKeyMethod.PASSPHRASE, s.info.method)
        assertNull(s.problem)
    }

    // @verifies SWR-MG-03
    // @verifies SWR-MG-08
    @Test fun a_restore_says_what_it_restored_and_what_it_could_not_and_confirms_last() = runTest {
        val store = MemoryStore()
        val old = filled()
        backUp(store, old)
        val fresh = InMemoryKeyValueStore()
        val m = restoreOnto(store, fresh)
        val r = (m.step.value as RestoreStep.Done).report
        assertTrue("protocols" in r.restored && "sessionHistory" in r.restored)
        assertTrue(r.confirmed)
        assertEquals(old.getString("np.session.history"), fresh.getString("np.session.history"))
        // No consent adapter is wired here, so the backup holds no withdrawal record: it is reported as missing. Grants are
        // never carried, and the person is told their earlier choices were not restored (SWR-MG-04).
        assertTrue("withdrawalState" in r.missing)
        assertTrue(r.consentNotRestored)
    }

    // @verifies SWR-MG-02
    // @verifies SWR-MG-07
    @Test fun a_secret_that_does_not_open_the_backup_can_be_tried_again_and_leaves_no_claim() = runTest {
        val store = MemoryStore()
        backUp(store, filled())
        val fresh = InMemoryKeyValueStore()
        val m = MigrationRestoreModel(place(store), fresh)
        m.look()
        m.submitSecret("another long enough passphrase")
        val wrong = (m.step.value as RestoreStep.EnterSecret).problem!!
        assertEquals("authentication_failed", wrong.reason)
        assertEquals(MessageGroup.REASONS, wrong.group)
        assertNull(fresh.getString("np.session.history"), "nothing is applied for a wrong secret")
        m.submitSecret("short")
        assertEquals("too_short", (m.step.value as RestoreStep.EnterSecret).problem!!.reason)
        m.submitSecret(passphrase)
        assertTrue(m.step.value is RestoreStep.Done, "the claim was not taken by the wrong tries")
    }

    // @verifies SWR-MG-07
    @Test fun a_second_phone_cannot_restore_a_backup_that_is_already_claimed() = runTest {
        val store = MemoryStore()
        backUp(store, filled())
        assertTrue(restoreOnto(store, InMemoryKeyValueStore()).step.value is RestoreStep.Done)
        val second = restoreOnto(store, InMemoryKeyValueStore())
        assertEquals("claimed", (second.step.value as RestoreStep.Failed).reason)
    }

    // @verifies SWR-MG-05
    @Test fun a_place_with_no_backup_or_no_place_at_all_says_so() = runTest {
        val none = MigrationRestoreModel(place(MemoryStore()), InMemoryKeyValueStore())
        none.look()
        assertEquals("no_bundle", (none.step.value as RestoreStep.Failed).reason)
        assertEquals(RestoreStep.NoPlace, MigrationRestoreModel(null, InMemoryKeyValueStore()).step.value)
    }

    // @verifies SWR-MG-06
    @Test fun a_host_that_cannot_do_the_platforms_clear_steps_says_this_device_cannot_clear_itself() = runTest {
        val store = MemoryStore()
        backUp(store, filled())
        assertEquals(ClearStep.NotAvailable, MigrationClearModel(place(store), InMemoryKeyValueStore(), null).step.value)
        assertEquals(ClearStep.NotAvailable, MigrationClearModel(null, InMemoryKeyValueStore(), RecordingActions()).step.value)
    }

    // @verifies SWR-MG-08
    @Test fun the_old_phone_is_not_cleared_until_the_new_phone_has_confirmed_the_backup() = runTest {
        val store = MemoryStore()
        val old = filled()
        val actions = RecordingActions()
        backUp(store, old)
        val m = MigrationClearModel(place(store), old, actions)
        m.look()
        assertTrue(m.step.value is ClearStep.Confirm)
        m.clear()
        assertEquals("no_confirmation", (m.step.value as ClearStep.Failed).reason)
        assertEquals("value-of-np.session.history", old.getString("np.session.history"), "nothing is erased without the confirmation")
        assertTrue(actions.ran.isEmpty())
    }

    // @verifies SWR-MG-06
    // @verifies SWR-MG-08
    // @verifies SWR-MG-09
    @Test fun after_the_new_phone_confirms_the_clear_runs_the_platforms_steps_and_erases_the_old_phones_data() = runTest {
        val store = MemoryStore()
        val old = filled()
        old.putString("ble.last-hub-id", "hub-1")
        val actions = RecordingActions()
        backUp(store, old)
        assertTrue((restoreOnto(store, InMemoryKeyValueStore()).step.value as RestoreStep.Done).report.confirmed)

        val m = MigrationClearModel(place(store), old, actions)
        m.look()
        m.clear()
        val done = m.step.value as ClearStep.Done
        assertTrue(done.removeBluetoothBond, "the person is told to remove the pairing in the system settings")
        assertEquals(listOf("destroyEncryptionKey", "forgetPairingRecord"), actions.ran)
        assertNull(old.getString("np.session.history"))
        assertNull(old.getString("ble.last-hub-id"))
        assertNotNull(old.getString("np.migration.cleared"))
    }

    // @verifies SWR-MG-08
    @Test fun a_confirmation_of_another_backup_does_not_clear_the_phone() = runTest {
        val store = MemoryStore()
        val old = filled()
        backUp(store, old)
        assertTrue(restoreOnto(store, InMemoryKeyValueStore()).step.value is RestoreStep.Done)
        // The old phone makes a newer backup: the confirmation on the place is of the earlier one.
        backUp(store, old, "yet another long passphrase")
        val actions = RecordingActions()
        val m = MigrationClearModel(place(store), old, actions)
        m.look()
        m.clear()
        assertEquals("other_bundle", (m.step.value as ClearStep.Failed).reason)
        assertEquals("value-of-np.session.history", old.getString("np.session.history"))
        assertTrue(actions.ran.isEmpty())
    }
}
