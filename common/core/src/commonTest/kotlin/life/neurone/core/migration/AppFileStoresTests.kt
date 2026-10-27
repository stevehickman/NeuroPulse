package life.neurone.core.migration

import kotlinx.coroutines.test.runTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

/** A store of named byte arrays, to show what the wrappers pass on and what they refuse. */
private class Bytes(vararg files: Pair<String, Int>) : FileStore {
    val data = files.associate { (p, n) -> p to ByteArray(n) { 1 } }.toMutableMap()
    val calls = mutableListOf<String>()
    override suspend fun list() = data.map { StoredFile(it.key, it.value.size.toLong()) }.sortedBy { it.path }
    override suspend fun write(path: String, offset: Long, bytes: ByteArray) { calls += "write $path"; bytes.copyInto(data.getValue(path), offset.toInt()) }
    override suspend fun sync(path: String) { calls += "sync $path" }
    override suspend fun delete(path: String) { calls += "delete $path"; data.remove(path) }
}

class AppFileStoresTests {

    // @verifies SWR-MG-06
    @Test
    fun theUnionNamesEachFileByItsFolderAndSendsACallOnlyToThatFolder() = runTest {
        val a = Bytes("same.db" to 3)
        val b = Bytes("same.db" to 5, "deeper/x.db" to 2)
        val all = FolderUnionFileStore(mapOf("files" to a, "prefs" to b))
        assertEquals(listOf("files/same.db" to 3L, "prefs/deeper/x.db" to 2L, "prefs/same.db" to 5L), all.list().map { it.path to it.size })

        all.write("prefs/same.db", 1, byteArrayOf(9, 9))
        all.sync("prefs/same.db")
        all.delete("prefs/deeper/x.db")
        assertEquals(listOf("write same.db", "sync same.db", "delete deeper/x.db"), b.calls)
        assertEquals(emptyList(), a.calls, "the other folder was not touched")
        assertEquals(listOf<Byte>(1, 9, 9, 1, 1), b.data.getValue("same.db").toList())

        for (bad in listOf("none/x.db", "files", "files/", "x.db")) assertFailsWith<IllegalArgumentException>(bad) { all.delete(bad) }
        assertFailsWith<IllegalArgumentException> { FolderUnionFileStore(mapOf("a/b" to a)) }
    }

    // @verifies SWR-MG-06
    @Test
    fun aKeptPathIsNeitherListedNorReachableAndNothingElseIsHidden() = runTest {
        val inner = Bytes("backups/bundle" to 4, "backups/claim.json" to 1, "np-app.properties" to 6, "np-app.properties.tmp" to 2, "backups2/x" to 1)
        val store = KeepingFileStore(inner, listOf("backups"))
        assertEquals(listOf("backups2/x", "np-app.properties", "np-app.properties.tmp"), store.list().map { it.path }, "backups2 is another folder")

        for (kept in listOf("backups/bundle", "backups", "/backups/claim.json")) {
            assertFailsWith<IllegalArgumentException>(kept) { store.write(kept, 0, byteArrayOf(0)) }
            assertFailsWith<IllegalArgumentException>(kept) { store.sync(kept) }
            assertFailsWith<IllegalArgumentException>(kept) { store.delete(kept) }
        }
        assertEquals(emptyList(), inner.calls, "a refused call never reached the files")
        store.delete("np-app.properties")
        assertEquals(listOf("delete np-app.properties"), inner.calls)
        assertFailsWith<IllegalArgumentException> { KeepingFileStore(inner, listOf("/")) }
    }
}
