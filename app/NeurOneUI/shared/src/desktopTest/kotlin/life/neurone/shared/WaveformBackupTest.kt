package life.neurone.shared

import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.ObjectStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.npps.NppsCore
import life.neurone.shared.migration.BackupKind
import life.neurone.shared.migration.BackupStep
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.migration.WaveformBackup
import life.neurone.shared.migration.WaveformBackupResult
import life.neurone.shared.migration.WaveformRestoreResult
import life.neurone.shared.migration.runBackup
import life.neurone.shared.session.UhdrFolder
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The recordings in a health-data backup: sealed once each beside the bundle, named by nothing, and put back by a restore (`REQ-SW03-29`). */
class WaveformBackupTest {

    private class Folder(val protectedNames: MutableList<String> = mutableListOf()) : UhdrFolder {
        override val store: ObjectStore = MemoryObjectStore()
        override fun protect(name: String) { protectedNames += name }
    }

    private val route = ObjectStoreRoute("test", atomicClaim = false)
    private fun place(store: ObjectStore = MemoryObjectStore()) = MigrationPlace("test", store, route)
    private fun secret(): BundleSecret = BundleKeyDerivation.newRecoveryCode(SystemRandomSource).secret
    private fun recording(id: Int, size: Int = 70_000) = ByteArray(size) { ((it + id) * 7 % 251).toByte() }
    private suspend fun Folder.add(id: Int, size: Int = 70_000): ByteArray = recording(id, size).also { store.put("session_$id.edf", it) }

    // @verifies REQ-SW03-29#a
    @Test fun each_recording_is_sealed_once_and_a_later_backup_writes_only_the_new_ones() = runTest {
        val folder = Folder(); folder.add(1); folder.add(2)
        val backup = WaveformBackup(folder)
        val place = place()
        val s = secret()
        val first = backup.backUp(place, BundleKeyMethod.RECOVERY_CODE, s)
        assertIs<WaveformBackupResult.Done>(first)
        assertEquals(2, first.written); assertEquals(2, first.total)
        val before = place.store.list("wf-")
        assertEquals(3, before.size, "two recordings and the index")

        folder.add(3)
        val second = backup.backUp(place, BundleKeyMethod.RECOVERY_CODE, s) as WaveformBackupResult.Done
        assertEquals(1, second.written); assertEquals(3, second.total)
        val after = place.store.list("wf-")
        assertEquals(4, after.size)
        assertTrue(before.all { it in after }, "the files already written are not rewritten under new names")
        val unchanged = backup.backUp(place, BundleKeyMethod.RECOVERY_CODE, s) as WaveformBackupResult.Done
        assertEquals(0, unchanged.written)
    }

    // @verifies REQ-SW03-29#b
    @Test fun the_folder_shows_no_recording_name_and_holds_none_of_the_waveform_in_the_clear() = runTest {
        val folder = Folder(); val data = folder.add(4242)
        val place = place()
        WaveformBackup(folder).backUp(place, BundleKeyMethod.RECOVERY_CODE, secret())
        val names = place.store.list("")
        assertTrue(names.none { "session" in it || "4242" in it || it.endsWith(".edf") }, names.toString())
        for (name in names) {
            val bytes = place.store.get(name)!!
            val window = data.copyOfRange(1000, 1032)
            assertFalse(contains(bytes, window), "$name holds waveform bytes in the clear")
        }
    }

    // @verifies REQ-SW03-29#c
    @Test fun a_restore_puts_each_recording_back_unchanged_and_leaves_one_the_app_already_holds() = runTest {
        val source = Folder(); val one = source.add(1); source.add(2); val three = source.add(3, 10)
        val place = place(); val s = secret()
        WaveformBackup(source).backUp(place, BundleKeyMethod.RECOVERY_CODE, s)

        val fresh = Folder()
        val newer = ByteArray(5) { 9 }
        fresh.store.put("session_2.edf", newer)
        val result = WaveformBackup(fresh).restore(place, s)
        assertIs<WaveformRestoreResult.Done>(result)
        assertEquals(2, result.restored); assertEquals(1, result.alreadyHere); assertEquals(0, result.unreadable)
        assertContentEquals(one, fresh.store.get("session_1.edf"))
        assertContentEquals(three, fresh.store.get("session_3.edf"))
        assertContentEquals(newer, fresh.store.get("session_2.edf"), "a recording the app holds is not overwritten")
        assertEquals(listOf("session_1.edf", "session_3.edf"), fresh.protectedNames.sorted(), "each restored file is protected as it lands")
    }

    // @verifies REQ-SW03-29#c
    @Test fun a_secret_that_does_not_open_the_recordings_restores_none_and_says_so() = runTest {
        val source = Folder(); source.add(1)
        val place = place()
        WaveformBackup(source).backUp(place, BundleKeyMethod.RECOVERY_CODE, secret())
        val fresh = Folder()
        assertEquals(WaveformRestoreResult.WrongSecret, WaveformBackup(fresh).restore(place, secret()))
        assertNull(fresh.store.get("session_1.edf"))
        assertEquals(WaveformRestoreResult.NoneInBackup, WaveformBackup(fresh).restore(place(), secret()))
    }

    // @verifies REQ-SW03-29#a
    @Test fun a_backup_under_a_new_secret_starts_a_new_set_and_removes_the_files_the_old_one_made() = runTest {
        val folder = Folder(); folder.add(1); folder.add(2)
        val place = place(); val old = secret(); val new = secret()
        val backup = WaveformBackup(folder)
        backup.backUp(place, BundleKeyMethod.RECOVERY_CODE, old)
        val oldFiles = place.store.list("wf-").toSet()
        val made = backup.backUp(place, BundleKeyMethod.RECOVERY_CODE, new) as WaveformBackupResult.Done
        assertEquals(2, made.written)
        val files = place.store.list("wf-")
        assertEquals(3, files.size)
        assertTrue(files.none { it in oldFiles && it != WaveformBackup.INDEX_NAME }, "the old set's files are gone")
        assertIs<WaveformRestoreResult.Done>(WaveformBackup(Folder()).restore(place, new))
        assertEquals(WaveformRestoreResult.WrongSecret, WaveformBackup(Folder()).restore(place, old))
    }

    // @verifies REQ-SW03-29#a
    @Test fun nothing_is_written_when_there_are_no_recordings() = runTest {
        val place = place()
        assertEquals(WaveformBackupResult.NothingToBackUp, WaveformBackup(Folder()).backUp(place, BundleKeyMethod.RECOVERY_CODE, secret()))
        assertTrue(place.store.list("").isEmpty())
    }

    // @verifies REQ-SW03-29#e
    @Test fun a_folder_that_will_not_take_the_files_is_reported_as_failed() = runTest {
        val folder = Folder(); folder.add(1)
        val broken = object : ObjectStore by MemoryObjectStore() {
            override suspend fun put(name: String, bytes: ByteArray) = error("no space")
        }
        assertEquals(WaveformBackupResult.Failed, WaveformBackup(folder).backUp(place(broken), BundleKeyMethod.RECOVERY_CODE, secret()))
    }

    private fun carriedKeys(): List<String> =
        NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["carried"]!!.jsonObject.values
            .flatMap { v -> v.jsonArray.map { k -> k.jsonPrimitive.content } }

    // @verifies REQ-SW03-29#d
    // @verifies REQ-SW03-29#e
    @Test fun only_a_health_data_backup_carries_recordings_and_one_that_fails_does_not_fail_the_rest() = runTest {
        val folder = Folder(); folder.add(1)
        val keys = InMemoryKeyValueStore().also { kv -> carriedKeys().forEach { kv.putString(it, "v-$it") } }
        val s = secret()
        val migrate = runBackup(MigrationPlace("m", MemoryObjectStore(), ObjectStoreRoute("t", atomicClaim = true)), keys, emptyList(), BackupKind.MIGRATE, BundleKeyMethod.RECOVERY_CODE, s, waveforms = WaveformBackup(folder))
        assertIs<BackupStep.Done>(migrate)
        assertNull(migrate.waveforms, "a device-move bundle carries no recordings")

        val uhdrPlace = place()
        val uhdr = runBackup(uhdrPlace, keys, emptyList(), BackupKind.UHDR, BundleKeyMethod.RECOVERY_CODE, s, waveforms = WaveformBackup(folder))
        assertIs<BackupStep.Done>(uhdr)
        assertIs<WaveformBackupResult.Done>(uhdr.waveforms)

        val none = runBackup(place(), keys, emptyList(), BackupKind.UHDR, BundleKeyMethod.RECOVERY_CODE, s)
        assertIs<BackupStep.Done>(none)
        assertNull(none.waveforms, "a host that keeps no recordings backs up none")
    }

    private fun contains(haystack: ByteArray, needle: ByteArray): Boolean {
        outer@ for (i in 0..haystack.size - needle.size) {
            for (j in needle.indices) if (haystack[i + j] != needle[j]) continue@outer
            return true
        }
        return false
    }
}
