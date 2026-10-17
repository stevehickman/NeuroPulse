package life.neurone.core.migration

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleSecretResult
import java.nio.file.Files
import java.nio.file.Path
import kotlin.io.path.exists
import kotlin.io.path.readBytes
import kotlin.io.path.writeBytes
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The folder-backed stores on a real disk: the route for a local computer or a USB drive, and the app's files for the clear. */
class DirectoryStoresTests {
    private fun temp(): Path = Files.createTempDirectory("np-dir-store")
    private val secret get() = (BundleKeyDerivation.passphraseSecret("a long enough passphrase") as BundleSecretResult.Ok).secret
    private fun host() = StandardMigrationHost()

    // @verifies SWR-MG-07
    @Test
    fun ofManySimultaneousCreatesExactlyOneWinsAndItsContentIsWhole() = runBlocking {
        val store = DirectoryObjectStore(temp())
        val results = (1..24).map { i -> async(Dispatchers.Default) { store.createIfAbsent("claim.json", ByteArray(4096) { i.toByte() }) } }.awaitAll()
        assertEquals(1, results.count { it }, "the operating system arbitrates: one create succeeds")
        val bytes = store.get("claim.json")!!
        assertEquals(4096, bytes.size)
        assertTrue(bytes.all { it == bytes[0] }, "the claim is one writer's, whole")
        assertEquals(listOf("claim.json"), store.list(""), "no temporary file is left")
    }

    // @verifies SWR-MG-11
    @Test
    fun anObjectIsWrittenReplacedReadListedAndDeletedAndANameCannotLeaveTheFolder() = runTest {
        val dir = temp()
        val store = DirectoryObjectStore(dir)
        assertNull(store.get("a.bin"))
        store.put("a.bin", byteArrayOf(1, 2, 3))
        store.put("a.bin", byteArrayOf(9))
        assertEquals(listOf<Byte>(9), store.get("a.bin")!!.toList())
        store.put("b.bin", byteArrayOf())
        assertEquals(listOf("a.bin", "b.bin"), store.list(""))
        assertEquals(listOf("b.bin"), store.list("b"))
        store.delete("a.bin")
        store.delete("a.bin")
        assertNull(store.get("a.bin"))
        assertTrue((store.freeSpace() ?: 0) > 0)
        for (bad in listOf("../x", "a/b", ".hidden", "", "a\\b", "é")) assertFailsWith<IllegalArgumentException> { store.get(bad) }
        assertTrue(dir.resolve("b.bin").exists())
    }

    // @verifies SWR-MG-12
    @Test
    fun aFolderOnDiskPassesTheAccountCheckAndTheProbeLeavesItEmpty() = runTest {
        val dir = temp()
        val r = MigrationFlow.probe(DirectoryObjectStore(dir), 1_000_000, host())
        assertEquals("sufficient", r["result"]!!.jsonPrimitive.content)
        assertEquals(0, Files.list(dir).use { it.count() }, "the probe leaves nothing behind")
    }

    // @verifies SWR-MG-10
    // @verifies SWR-MG-07
    // @verifies SWR-MG-06
    @Test
    fun aMigrationOverAFolderOnDiskMovesThePhoneAndTheClearOverwritesAndDeletesRealFiles() = runTest {
        val folder = DirectoryObjectStore(temp())
        val route = buildJsonObject { put("id", "folder"); put("atomicClaim", true); put("direct", false); put("authenticatedPairing", true) }
        val items = buildJsonArray { }
        val oldKv = InMemoryKeyValueStore().also { it.putString("np.session.history", "history of the old phone") }
        val backup = MigrationFlow.run(
            "backup", buildJsonObject { put("kind", "migrate"); put("method", "passphrase"); put("route", route); put("hostItems", items) },
            host(), secret, oldKv, folder,
        )
        assertTrue(backup["ok"]!!.jsonPrimitive.boolean)

        val newKv = InMemoryKeyValueStore()
        val restore = MigrationFlow.run("restore", buildJsonObject { put("route", route); put("hostItems", items) }, host(), secret, newKv, folder)
        assertTrue(restore["ok"]!!.jsonPrimitive.boolean)
        assertEquals("history of the old phone", newKv.getString("np.session.history"))
        val second = MigrationFlow.run("restore", buildJsonObject { put("route", route); put("hostItems", items) }, host(), secret, InMemoryKeyValueStore(), folder)
        assertEquals("claimed", second["reason"]!!.jsonPrimitive.content)

        val dataDir = temp()
        dataDir.resolve("uhdr").also { Files.createDirectories(it) }.resolve("sessions.db").writeBytes(ByteArray(40_000) { 0x5A })
        dataDir.resolve("settings.db").writeBytes(ByteArray(0))
        val clear = MigrationFlow.run(
            "clear", buildJsonObject { put("bundleHash", backup["bundleHash"]!!.jsonPrimitive.content); put("canRemoveOsBond", true) },
            StandardMigrationHost(actions = object : ClearActions { override suspend fun run(step: String) = true }), null,
            oldKv, store = folder, files = DirectoryFileStore(dataDir),
        )
        assertTrue(clear["ok"]!!.jsonPrimitive.boolean, clear.toString())
        assertEquals(0, Files.walk(dataDir).use { s -> s.filter { Files.isRegularFile(it) }.count() }, "every file is gone")
    }

    // @verifies SWR-MG-06
    @Test
    fun theFileStoreOverwritesInPlaceAndWillNotLeaveItsFolder() = runTest {
        val dir = temp()
        val files = DirectoryFileStore(dir)
        dir.resolve("x.db").writeBytes(ByteArray(10) { 1 })
        assertEquals(listOf("x.db" to 10L), files.list().map { it.path to it.size })
        files.write("x.db", 4, byteArrayOf(7, 7))
        files.sync("x.db")
        assertEquals(listOf<Byte>(1, 1, 1, 1, 7, 7, 1, 1, 1, 1), dir.resolve("x.db").readBytes().toList())
        files.delete("x.db")
        files.delete("x.db")
        assertFalse(dir.resolve("x.db").exists())
        for (bad in listOf("../x.db", "/etc/passwd", "")) assertFailsWith<IllegalArgumentException> { files.delete(bad) }
    }

    // The host's own files, as the Android and desktop apps assemble them (appFileStore): the clear overwrites and deletes the data
    // folders' files, including the backing file of the key-value store and its leftovers, and leaves the backup it is clearing
    // against, which on a computer is inside the app's own data folder.
    // @verifies SWR-MG-06
    @Test
    fun theClearTakesTheHostsDataFilesAndLeavesTheBackupItClearsAgainst() = runTest {
        val data = temp()
        data.resolve("np-app.properties").writeBytes(ByteArray(300) { 0x41 })
        data.resolve("np-app.properties.tmp").writeBytes(ByteArray(10) { 0x42 })
        data.resolve("logs").also { Files.createDirectories(it) }.resolve("session.log").writeBytes(ByteArray(70_000) { 0x43 })
        val place = data.resolve("backups")
        val folder = DirectoryObjectStore(place)
        val prefs = temp().also { it.resolve("np-app.xml").writeBytes(ByteArray(120) { 0x44 }); it.resolve("np-app.xml.bak").writeBytes(ByteArray(80) { 0x45 }) }

        val oldKv = InMemoryKeyValueStore().also { it.putString("np.session.history", "history of the old phone") }
        val route = buildJsonObject { put("id", "local-folder"); put("atomicClaim", true) }
        val backup = MigrationFlow.run(
            "backup", buildJsonObject { put("kind", "migrate"); put("method", "passphrase"); put("route", route); put("hostItems", buildJsonArray { }) },
            host(), secret, oldKv, folder,
        )
        assertTrue(backup["ok"]!!.jsonPrimitive.boolean)
        val restore = MigrationFlow.run(
            "restore", buildJsonObject { put("route", route); put("hostItems", buildJsonArray { }) }, host(), secret, InMemoryKeyValueStore(), folder,
        )
        assertTrue(restore["ok"]!!.jsonPrimitive.boolean)
        fun placeContents() = place.toFile().list()!!.sorted().associateWith { place.resolve(it).readBytes().toList() }
        val before = placeContents()
        assertTrue(before.isNotEmpty())

        val files = appFileStore(listOf(AppFolder("data", data, kept = listOf("backups")), AppFolder("shared_prefs", prefs)))
        assertEquals(
            listOf("data/logs/session.log", "data/np-app.properties", "data/np-app.properties.tmp", "shared_prefs/np-app.xml", "shared_prefs/np-app.xml.bak"),
            files.list().map { it.path },
        )
        val clear = MigrationFlow.run(
            "clear", buildJsonObject { put("bundleHash", backup["bundleHash"]!!.jsonPrimitive.content); put("canRemoveOsBond", true) },
            StandardMigrationHost(actions = object : ClearActions { override suspend fun run(step: String) = true }), null,
            oldKv, store = folder, files = files,
        )
        assertTrue(clear["ok"]!!.jsonPrimitive.boolean, clear.toString())
        assertEquals(emptyList(), files.list(), "every data file is gone")
        assertEquals(0, Files.walk(prefs).use { s -> s.filter { Files.isRegularFile(it) }.count() })
        assertEquals(before, placeContents(), "the backup and the new phone's confirmation are still there, unchanged")
    }
}
