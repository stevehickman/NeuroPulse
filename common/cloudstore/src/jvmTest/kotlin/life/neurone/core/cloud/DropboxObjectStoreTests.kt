package life.neurone.core.cloud

import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * A stand-in for the Dropbox app folder, written from the same recollection of the API as the adapter (`UC-116`): content calls carry
 * their arguments in `Dropbox-API-Arg`, `mode: add` with `autorename: false` fails with a 409 `path/conflict/file/...` when the name is taken,
 * a download of a missing file is a 409 `path/not_found/...`, delete of a missing file is a 409 `path_lookup/not_found/...`, listing pages by
 * `has_more` and `cursor`, and space usage is `used` and `allocation.allocated`. **It is not Dropbox**: a test passing against it shows the adapter
 * agrees with this fake, which was not checked against Dropbox's pages, and it serialises racing uploads.
 */
internal class FakeDropbox(private val pageSize: Int = 2) : HttpTransport {
    val files = LinkedHashMap<String, ByteArray>()
    val folders = mutableListOf("a folder")
    val seen = mutableListOf<HttpRequest>()
    var offline = false
    var failStatus: Int? = null
    var allocated: Long? = 1000L

    private fun r(code: Int, body: String = "") = HttpResponse(code, emptyMap(), body.encodeToByteArray())
    private fun err(summary: String) = r(409, """{"error_summary":"$summary","error":{}}""")
    private fun q(s: String) = "\"" + s.replace("\\", "\\\\").replace("\"", "\\\"") + "\""
    private fun arg(request: HttpRequest) = Json.parseToJsonElement(request.headers["Dropbox-API-Arg"] ?: error("no Dropbox-API-Arg")).jsonObject
    private fun name(path: String) = path.removePrefix("/")

    override suspend fun post(request: HttpRequest): HttpResponse? = synchronized(this) {
        if (offline) return null
        seen += request
        if (request.headers["Authorization"] != "Bearer tok") return r(401, """{"error_summary":"invalid_access_token/"}""")
        failStatus?.let { return r(it, "{}") }
        val path = request.url.substringAfter("//").substringAfter('/')
        when (path) {
            "2/files/upload" -> {
                val a = arg(request)
                val n = name(a["path"]!!.jsonPrimitive.content)
                assertTrue(request.headers["Content-Type"] == "application/octet-stream")
                assertEquals("false", a["autorename"]!!.jsonPrimitive.content)
                if (a["mode"]!!.jsonPrimitive.content == "add" && files.containsKey(n)) return err("path/conflict/file/..")
                files[n] = request.body.copyOf()
                r(200, """{"name":${q(n)}}""")
            }
            "2/files/download" -> files[name(arg(request)["path"]!!.jsonPrimitive.content)]?.let { HttpResponse(200, emptyMap(), it.copyOf()) } ?: err("path/not_found/..")
            "2/files/delete_v2" -> {
                val n = name(Json.parseToJsonElement(request.body.decodeToString()).jsonObject["path"]!!.jsonPrimitive.content)
                if (files.remove(n) != null) r(200, "{}") else err("path_lookup/not_found/..")
            }
            "2/files/list_folder", "2/files/list_folder/continue" -> {
                val body = Json.parseToJsonElement(request.body.decodeToString()).jsonObject
                val from = if (path.endsWith("/continue")) body["cursor"]!!.jsonPrimitive.content.toInt() else 0
                val all = folders.map { """{".tag":"folder","name":${q(it)}}""" } + files.keys.sorted().map { """{".tag":"file","name":${q(it)}}""" }
                val more = from + pageSize < all.size
                r(200, """{"entries":[${all.drop(from).take(pageSize).joinToString(",")}],"cursor":"${from + pageSize}","has_more":$more}""")
            }
            "2/users/get_space_usage" -> r(200, """{"used":100,"allocation":{".tag":"individual"${allocated?.let { ",\"allocated\":$it" } ?: ""}}}""")
            else -> r(404)
        }
    }
}

private fun db(fake: FakeDropbox) =
    DropboxObjectStore(fake, AccessToken { "tok" }, timeoutSeconds = 30, apiUrl = "https://api.test/2", contentUrl = "https://content.test/2")

class DropboxObjectStoreContractTests : ObjectStoreContract() {
    override fun newStore(): ObjectStore = db(FakeDropbox())
}

class DropboxObjectStoreTests {
    // @verifies SWR-MG-11#calls
    @Test
    fun aCreateIsAnAddWithNoAutorenameAndAPutAnOverwrite() = runBlocking {
        val fake = FakeDropbox()
        val s = db(fake)
        assertEquals(true, s.createIfAbsent("np-migration-claim.json", byteArrayOf(1)))
        assertEquals(false, s.createIfAbsent("np-migration-claim.json", byteArrayOf(2)))
        s.put("np-migration-claim.json", byteArrayOf(3))
        val args = fake.seen.filter { it.url.endsWith("/files/upload") }.map { it.headers["Dropbox-API-Arg"]!! }
        assertTrue("\"mode\":\"add\"" in args[0] && "\"autorename\":false" in args[0], args[0])
        assertTrue("\"mode\":\"overwrite\"" in args[2], args[2])
        assertEquals("https://content.test/2/files/upload", fake.seen.first().url)
        assertContentEquals(byteArrayOf(3), s.get("np-migration-claim.json"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun theArgumentHeaderIsAsciiAndNamesDropboxRefusesAreStoredEncodedAndListedDecoded() = runBlocking {
        val fake = FakeDropbox(pageSize = 100)
        val s = db(fake)
        s.put("dir/é:b?%", byteArrayOf(1))
        assertEquals(setOf("dir%2Fé%3Ab%3F%25"), fake.files.keys)
        assertTrue(fake.seen.last().headers["Dropbox-API-Arg"]!!.all { it.code in 0x20..0x7E }, "non-ASCII is written as an escape")
        assertEquals(listOf("dir/é:b?%"), s.list("dir/"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun listSkipsFoldersFiltersByPrefixAndFollowsTheCursor() = runBlocking {
        val fake = FakeDropbox(pageSize = 1)
        val s = db(fake)
        listOf("np-1", "np-2", "other", "np-3").forEach { s.put(it, byteArrayOf(1)) }
        assertEquals(listOf("np-1", "np-2", "np-3"), s.list("np-"))
        assertTrue(fake.seen.count { it.url.endsWith("/files/list_folder/continue") } >= 2)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun freeSpaceIsAllocatedMinusUsedOrNull() = runBlocking {
        val fake = FakeDropbox()
        assertEquals(900L, db(fake).freeSpace())
        assertEquals(0, fake.seen.last().body.size, "the call takes no body")
        fake.allocated = null
        assertNull(db(fake).freeSpace())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun anObjectLargerThanOneCallIsRefusedBeforeAnythingIsSent() = runBlocking {
        val fake = FakeDropbox()
        assertFailsWith<CloudStoreException> { db(fake).put("x", ByteArray(DropboxObjectStore.MAX_UPLOAD_BYTES + 1)) }
        assertTrue(fake.seen.isEmpty())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun noAnswerAndErrorsThrowInsteadOfReadingAsAbsentOrRefused() = runBlocking {
        val fake = FakeDropbox()
        val s = db(fake)
        s.put("x", byteArrayOf(1))
        fake.failStatus = 500
        for (call in listOf<suspend () -> Unit>({ s.get("x") }, { s.put("x", byteArrayOf(1)) }, { s.createIfAbsent("x", byteArrayOf(1)) }, { s.delete("x") }, { s.list("") }, { s.freeSpace() })) {
            assertEquals(500, assertFailsWith<CloudStoreException> { call() }.status)
        }
        fake.failStatus = null
        fake.offline = true
        assertFailsWith<CloudStoreException> { s.get("x") }
        Unit
    }
}
