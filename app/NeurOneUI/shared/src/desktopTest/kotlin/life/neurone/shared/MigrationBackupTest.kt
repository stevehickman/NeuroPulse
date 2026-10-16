package life.neurone.shared

import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleSecretResult
import life.neurone.core.migration.MigrationFlow
import life.neurone.core.migration.MigrationHost
import life.neurone.core.migration.ObjectStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.migration.toJson
import life.neurone.core.npps.NppsCore
import life.neurone.shared.migration.BackupKind
import life.neurone.shared.migration.BackupStep
import life.neurone.shared.migration.MessageGroup
import life.neurone.shared.migration.MigrationBackupModel
import life.neurone.shared.ui.migrationText
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** The backup screen's steps, without Compose: what it asks the core, and in what order (SWR-MG-02, SWR-MG-03). */
class MigrationBackupTest {
    private class MemoryStore(var failPut: Boolean = false) : ObjectStore {
        val objects = linkedMapOf<String, ByteArray>()
        override suspend fun get(name: String) = objects[name]?.copyOf()
        override suspend fun put(name: String, bytes: ByteArray) {
            if (failPut) error("put failed")
            objects[name] = bytes.copyOf()
        }
        override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean {
            if (name in objects) return false
            objects[name] = bytes.copyOf()
            return true
        }
        override suspend fun delete(name: String) { objects.remove(name) }
        override suspend fun list(prefix: String) = objects.keys.filter { it.startsWith(prefix) }
        override suspend fun freeSpace(): Long? = 1L shl 30
    }

    private val route = ObjectStoreRoute("test", atomicClaim = true)

    private fun carriedKeys(): List<String> =
        NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["carried"]!!.jsonObject.values
            .flatMap { v -> v.jsonArray.map { k -> k.jsonPrimitive.content } }

    private fun filled(): InMemoryKeyValueStore = InMemoryKeyValueStore().also { kv -> carriedKeys().forEach { kv.putString(it, "value-of-$it") } }

    private fun model(store: MemoryStore, kv: InMemoryKeyValueStore = filled()) =
        MigrationBackupModel(MigrationPlace("test place", store, route), kv)

    // @verifies SWR-MG-03
    @Test fun a_host_with_nowhere_to_put_a_backup_says_so_and_goes_no_further() {
        val m = MigrationBackupModel(null, InMemoryKeyValueStore())
        assertEquals(BackupStep.NoPlace, m.step.value)
        m.chooseKind(BackupKind.MIGRATE)
        assertEquals(BackupStep.NoPlace, m.step.value)
    }

    // @verifies SWR-MG-03
    @Test fun the_screen_says_what_each_kind_leaves_out_before_anything_is_made() {
        val store = MemoryStore()
        val m = model(store)
        m.chooseKind(BackupKind.UHDR)
        val uhdr = (m.step.value as BackupStep.Preview).preview
        assertTrue("protocols" in uhdr.omitted && "protocols" !in uhdr.carried)
        assertTrue("sessionHistory" in uhdr.carried)
        assertTrue("researchDeviceToken" in uhdr.neverMoved && "consumableCounters" in uhdr.notYetSorted)
        m.back()
        m.chooseKind(BackupKind.MIGRATE)
        val migrate = (m.step.value as BackupStep.Preview).preview
        assertTrue(migrate.omitted.isEmpty() && "protocols" in migrate.carried)
        assertTrue(store.objects.isEmpty(), "nothing is written until the person has chosen how to lock it")
    }

    // @verifies SWR-MG-02
    @Test fun a_recovery_code_is_shown_once_and_the_backup_opens_with_exactly_that_code() = runTest {
        val store = MemoryStore()
        val kv = filled()
        val m = model(store, kv)
        m.chooseKind(BackupKind.MIGRATE)
        m.continueToMethod()
        m.useRecoveryCode()
        val code = (m.step.value as BackupStep.ShowCode).code
        assertTrue(store.objects.isEmpty(), "the backup waits until the person says the code is saved")
        m.confirmCodeSaved()
        val done = m.step.value as BackupStep.Done
        assertTrue(done.bundleHash.isNotEmpty() && "protocols" in done.written)
        assertEquals(1, store.objects.size, "the bundle is the only object after a backup")

        val secret = (BundleKeyDerivation.recoveryCodeSecret(code) as BundleSecretResult.Ok).secret
        val fresh = InMemoryKeyValueStore()
        val noHost = object : MigrationHost {
            override suspend fun perform(effect: kotlinx.serialization.json.JsonObject) = error("no host effect expected")
        }
        val restored = MigrationFlow.run(
            "restore", buildJsonObject { put("route", route.toJson()); put("hostItems", JsonArray(emptyList())) }, noHost, secret, fresh, store,
        )
        assertEquals("true", restored["ok"]!!.jsonPrimitive.content, restored.toString())
        assertEquals(kv.getString("np.session.history"), fresh.getString("np.session.history"))
    }

    // @verifies SWR-MG-02
    @Test fun a_passphrase_that_is_too_short_is_refused_with_the_cores_reason_and_nothing_is_written() = runTest {
        val store = MemoryStore()
        val m = model(store)
        m.chooseKind(BackupKind.MIGRATE)
        m.continueToMethod()
        m.usePassphrase()
        m.submitPassphrase("short")
        val s = m.step.value as BackupStep.EnterPassphrase
        assertEquals("too_short", s.problem!!.reason)
        assertNotNull(s.problem!!.minChars)
        assertTrue(store.objects.isEmpty())
        m.submitPassphrase("a long enough passphrase")
        assertTrue(m.step.value is BackupStep.Done)
        assertEquals(1, store.objects.size)
    }

    // @verifies SWR-MG-11
    @Test fun a_place_that_cannot_be_written_ends_with_the_routes_reason_and_the_person_can_go_back() = runTest {
        val m = model(MemoryStore(failPut = true))
        m.chooseKind(BackupKind.UHDR)
        m.continueToMethod()
        m.usePassphrase()
        m.submitPassphrase("a long enough passphrase")
        assertEquals("route_unavailable", (m.step.value as BackupStep.Failed).reason)
        m.back()
        assertEquals(BackupStep.ChooseKind, m.step.value)
    }

    // @verifies SWR-MG-03
    @Test fun every_locale_key_the_core_names_has_a_resource_to_show() {
        val keys = MigrationFlow.messageKeys()
        var seen = 0
        for (group in MessageGroup.entries) {
            for ((code, key) in keys[group.wire]!!.jsonObject) {
                assertNotNull(migrationText(key.jsonPrimitive.content), "$code ($key) has no string resource")
                seen++
            }
        }
        assertTrue(seen >= 37, "the core names fewer keys than before: $seen")
        assertFalse(migrationText("MIGRATION_NOT_A_KEY") != null)
    }
}
