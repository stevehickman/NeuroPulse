package life.neurone.shared

import kotlinx.coroutines.test.runTest
import life.neurone.core.migration.ObjectStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.shared.migration.StorageCheckModel
import life.neurone.shared.migration.StorageVerdict
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The capability check of a place, as the app asks it (SWR-MG-12). */
class StorageCheckTest {
    private class Store(
        val conditional: Boolean = true,
        val space: Long? = 1L shl 30,
        val failOn: String? = null,
    ) : ObjectStore {
        val objects = linkedMapOf<String, ByteArray>().also { it["user-data"] = byteArrayOf(1) }
        private fun check(call: String) { if (call == failOn) error("$call failed") }
        override suspend fun get(name: String): ByteArray? { check("get"); return objects[name]?.copyOf() }
        override suspend fun put(name: String, bytes: ByteArray) { check("put"); objects[name] = bytes.copyOf() }
        override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean {
            check("create")
            if (conditional && name in objects) return false
            objects[name] = bytes.copyOf()
            return true
        }
        override suspend fun delete(name: String) { check("delete"); objects.remove(name) }
        override suspend fun list(prefix: String): List<String> { check("list"); return objects.keys.filter { it.startsWith(prefix) } }
        override suspend fun freeSpace(): Long? { check("space"); return space }
    }

    private fun place(store: Store, atomic: Boolean = true) = MigrationPlace("test", store, ObjectStoreRoute("test", atomicClaim = atomic))

    // @verifies SWR-MG-12
    @Test fun a_place_that_does_everything_is_sufficient_and_nothing_of_the_check_is_left_behind() = runTest {
        val store = Store()
        val c = StorageCheckModel.check(place(store), 1_000)
        assertEquals(StorageVerdict.SUFFICIENT, c.verdict)
        assertNull(c.failedOperation)
        assertNull(c.operationKey)
        assertEquals("MIGRATION_PROBE_SUFFICIENT", c.messageKey)
        assertTrue(c.canMigrate && c.canBackUp)
        assertEquals(listOf("user-data"), store.objects.keys.toList())
    }

    // @verifies SWR-MG-12
    @Test fun a_place_with_no_conditional_create_is_listed_for_the_backup_only_and_the_message_names_the_operation() = runTest {
        val c = StorageCheckModel.check(place(Store(conditional = false)), 1_000)
        assertEquals(StorageVerdict.BACKUP_ONLY, c.verdict)
        assertEquals("createIfAbsentSecondRefused", c.failedOperation)
        assertEquals("MIGRATION_PROBE_BACKUP_ONLY", c.messageKey)
        assertEquals("MIGRATION_PROBE_OP_CREATE_IF_ABSENT_SECOND_REFUSED", c.operationKey)
        assertTrue(c.canBackUp)
        assertFalse(c.canMigrate)
    }

    // @verifies SWR-MG-12
    @Test fun a_place_with_too_little_room_is_not_sufficient_and_a_call_that_throws_is_a_failed_operation() = runTest {
        val small = StorageCheckModel.check(place(Store(space = 10)), 1_000)
        assertEquals(StorageVerdict.INSUFFICIENT, small.verdict)
        assertEquals("MIGRATION_PROBE_OP_SPACE", small.operationKey)
        assertFalse(small.canBackUp)

        val store = Store(failOn = "list")
        val broken = StorageCheckModel.check(place(store), 1_000)
        assertEquals(StorageVerdict.INSUFFICIENT, broken.verdict)
        assertEquals("list", broken.failedOperation)
        assertEquals(listOf("user-data"), store.objects.keys.toList())
    }

    // @verifies SWR-MG-12
    @Test fun a_place_the_route_withholds_from_a_migration_is_never_called_sufficient() = runTest {
        val c = StorageCheckModel.check(place(Store(), atomic = false), 1_000)
        assertEquals(StorageVerdict.BACKUP_ONLY, c.verdict)
        assertFalse(c.canMigrate)
    }
}
