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
import life.neurone.core.data.RandomSource
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
    private class StepRandom(private var n: Int = 0) : RandomSource {
        override fun nextBytes(count: Int): ByteArray = ByteArray(count) { (n++ * 31 + 7).toByte() }
    }

    private fun temp(): Path = Files.createTempDirectory("np-dir-store")
    private val secret get() = (BundleKeyDerivation.passphraseSecret("a long enough passphrase") as BundleSecretResult.Ok).secret
    private fun host(seed: Int) = StandardMigrationHost(StepRandom(seed))

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
        val r = MigrationFlow.probe(DirectoryObjectStore(dir), 1_000_000, host(7))
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
            host(1), secret, oldKv, folder,
        )
        assertTrue(backup["ok"]!!.jsonPrimitive.boolean)

        val newKv = InMemoryKeyValueStore()
        val restore = MigrationFlow.run("restore", buildJsonObject { put("route", route); put("hostItems", items) }, host(50), secret, newKv, folder)
        assertTrue(restore["ok"]!!.jsonPrimitive.boolean)
        assertEquals("history of the old phone", newKv.getString("np.session.history"))
        val second = MigrationFlow.run("restore", buildJsonObject { put("route", route); put("hostItems", items) }, host(90), secret, InMemoryKeyValueStore(), folder)
        assertEquals("claimed", second["reason"]!!.jsonPrimitive.content)

        val dataDir = temp()
        dataDir.resolve("uhdr").also { Files.createDirectories(it) }.resolve("sessions.db").writeBytes(ByteArray(40_000) { 0x5A })
        dataDir.resolve("settings.db").writeBytes(ByteArray(0))
        val clear = MigrationFlow.run(
            "clear", buildJsonObject { put("bundleHash", backup["bundleHash"]!!.jsonPrimitive.content); put("canRemoveOsBond", true) },
            StandardMigrationHost(StepRandom(1), actions = object : ClearActions { override suspend fun run(step: String) = true }), null,
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
}
