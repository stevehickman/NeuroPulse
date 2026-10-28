package life.neurone.core.cloud

import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import java.net.URI
import java.net.URLDecoder
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * A stand-in for the Google Drive application data folder, written from the Drive API pages read 2026-10-28
 * (`docs/np_sw_cloudstore_001.md` §3.1): files have server-made IDs and non-unique names, `files.list` takes a `q`
 * (`name = '...'`, `'appDataFolder' in parents`), `orderBy` and a page token, a resumable session is started by a POST
 * (create) or PATCH (update) and answered with a `Location`, ranges are multiples of 256 KiB answered 308 until the last,
 * which answers 200 or 201, `alt=media` returns the bytes, delete answers 204. **It is not Drive**, and it lists a write at once;
 * whether Drive does is not stated on the pages read.
 */
internal class FakeDrive(private val pageSize: Int = 2) : HttpTransport {
    class F(val id: String, val name: String, var bytes: ByteArray, var modified: Int)
    val files = LinkedHashMap<String, F>()
    val seen = mutableListOf<HttpRequest>()
    var offline = false
    var failStatus: Int? = null
    var limit: String? = "1000"
    var usage = "400"
    private var clock = 0
    private var ids = 0
    private class Session(val id: String?, val name: String?, val data: java.io.ByteArrayOutputStream = java.io.ByteArrayOutputStream())
    private val sessions = HashMap<String, Session>()

    private fun r(code: Int, body: String = "", headers: Map<String, String> = emptyMap()) = HttpResponse(code, headers, body.encodeToByteArray())
    private fun dec(s: String) = URLDecoder.decode(s.replace("+", "%2B"), "UTF-8")
    private fun unq(s: String) = s.replace("\\'", "'").replace("\\\\", "\\")

    override suspend fun post(request: HttpRequest): HttpResponse? = synchronized(this) {
        if (offline) return null
        seen += request
        val uri = URI.create(request.url)
        val query = (uri.rawQuery ?: "").split('&').filter { it.isNotEmpty() }.associate { val kv = it.split('=', limit = 2); dec(kv[0]) to dec(if (kv.size > 1) kv[1] else "") }
        val path = uri.rawPath
        if (path.startsWith("/session/")) {
            val s = sessions[path] ?: return r(404)
            val m = Regex("bytes (\\d+)-(\\d+)/(\\d+)").matchEntire(request.headers["Content-Range"] ?: return r(400)) ?: return r(400)
            val (from, to, total) = m.destructured.toList().map { it.toInt() }
            if (from != s.data.size()) return r(416)
            if ((to + 1 - from) % 262_144 != 0 && to + 1 != total) return r(400)
            s.data.write(request.body)
            if (to + 1 < total) return r(308, "", mapOf("Range" to "bytes=0-$to"))
            return if (s.id == null) {
                val f = F("id${ids++}", s.name!!, s.data.toByteArray(), clock++); files[f.id] = f
                r(201, """{"id":"${f.id}","name":${JsonPrimitive(f.name)}}""")
            } else {
                files[s.id]!!.apply { bytes = s.data.toByteArray(); modified = clock++ }
                r(200, """{"id":"${s.id}"}""")
            }
        }
        if (request.headers["Authorization"] != "Bearer tok") return r(401, """{"error":{"code":401}}""")
        failStatus?.let { return r(it, """{"error":{"code":$it}}""") }
        when {
            path == "/upload/drive/v3/files" && request.method == "POST" -> {
                val meta = Json.parseToJsonElement(request.body.decodeToString()).jsonObject
                if (meta["parents"]!!.jsonArray.single().jsonPrimitive.content != "appDataFolder") return r(400)
                val loc = "/session/${sessions.size}"; sessions[loc] = Session(null, meta["name"]!!.jsonPrimitive.content)
                r(200, "", mapOf("Location" to "https://drive.test$loc"))
            }
            path.startsWith("/upload/drive/v3/files/") && request.method == "PATCH" -> {
                val id = path.substringAfterLast('/'); if (id !in files) return r(404)
                val loc = "/session/${sessions.size}"; sessions[loc] = Session(id, null)
                r(200, "", mapOf("Location" to "https://drive.test$loc"))
            }
            path == "/drive/v3/files" && request.method == "GET" -> {
                if (query["spaces"] != "appDataFolder" || "'appDataFolder' in parents" !in query["q"]!!) return r(400)
                val nm = Regex("^name = '((?:[^'\\\\]|\\\\.)*)' and ").find(query["q"]!!)?.groupValues?.get(1)?.let { unq(it) }
                var all = files.values.filter { nm == null || it.name == nm }
                if (query["orderBy"] == "modifiedTime desc") all = all.sortedByDescending { it.modified }
                val from = query["pageToken"]?.toInt() ?: 0
                val slice = all.drop(from).take(pageSize)
                val next = if (from + pageSize < all.size) ""","nextPageToken":"${from + pageSize}"""" else ""
                r(200, """{"files":[${slice.joinToString(",") { """{"id":"${it.id}","name":${JsonPrimitive(it.name)}}""" }}]$next}""")
            }
            path.startsWith("/drive/v3/files/") && request.method == "GET" ->
                files[path.substringAfterLast('/')]?.let { HttpResponse(200, emptyMap(), it.bytes.copyOf()) } ?: r(404)
            path.startsWith("/drive/v3/files/") && request.method == "DELETE" ->
                if (files.remove(path.substringAfterLast('/')) != null) r(204) else r(404)
            path == "/drive/v3/about" -> r(200, """{"storageQuota":{${limit?.let { "\"limit\":\"$it\"," } ?: ""}"usage":"$usage"}}""")
            else -> r(405)
        }
    }
}

private fun gd(fake: FakeDrive, chunk: Int = 262_144) =
    GoogleDriveObjectStore(fake, AccessToken { "tok" }, timeoutSeconds = 30, baseUrl = "https://drive.test", chunkBytes = chunk)

class GoogleDriveObjectStoreContractTests : ObjectStoreContract() {
    override val claimsAreAtomic = false
    override fun newStore(): ObjectStore = gd(FakeDrive())
}

class GoogleDriveObjectStoreTests {
    // @verifies SWR-MG-11#calls
    @Test
    fun aCreateIsRefusedWithAThrowSoTheAccountCheckFailsTheClaimStep() = runBlocking {
        val fake = FakeDrive()
        val e = assertFailsWith<CloudStoreException> { gd(fake).createIfAbsent("np-migration-claim.json", byteArrayOf(1)) }
        assertTrue("SWR-MG-07" in e.message!!)
        assertTrue(fake.seen.isEmpty(), "nothing is sent for a claim Drive cannot hold")
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aLargeObjectGoesInRangesToTheSessionWithoutAToken() = runBlocking {
        val fake = FakeDrive()
        val bytes = ByteArray(262_144 * 2 + 50) { (it * 7).toByte() }
        gd(fake).put("np-migration-bundle.npbk", bytes)
        assertContentEquals(bytes, fake.files.values.single().bytes)
        val puts = fake.seen.filter { it.method == "PUT" }
        assertEquals(listOf("bytes 0-262143/${bytes.size}", "bytes 262144-524287/${bytes.size}", "bytes 524288-${bytes.size - 1}/${bytes.size}"), puts.map { it.headers["Content-Range"] })
        assertTrue(puts.all { r -> r.headers.keys.none { it.equals("authorization", true) } })
        assertTrue(fake.seen.first { it.method == "POST" }.url.startsWith("https://drive.test/upload/drive/v3/files?uploadType=resumable"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aSecondPutUpdatesTheSameFileInsteadOfMakingAnother() = runBlocking {
        val fake = FakeDrive()
        val s = gd(fake)
        s.put("x", byteArrayOf(1)); s.put("x", byteArrayOf(2))
        assertEquals(1, fake.files.size)
        assertEquals("PATCH", fake.seen.last { it.url.contains("uploadType=resumable") && it.method != "PUT" }.method)
        assertContentEquals(byteArrayOf(2), s.get("x"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun withDuplicateNamesAReadTakesTheNewestAndADeleteRemovesAll() = runBlocking {
        val fake = FakeDrive()
        fake.files["a"] = FakeDrive.F("a", "dup", byteArrayOf(1), 1)
        fake.files["b"] = FakeDrive.F("b", "dup", byteArrayOf(2), 5)
        fake.files["c"] = FakeDrive.F("c", "dup", byteArrayOf(3), 3)
        val s = gd(fake)
        assertContentEquals(byteArrayOf(2), s.get("dup"))
        s.put("dup", byteArrayOf(9))
        assertContentEquals(byteArrayOf(9), fake.files["b"]!!.bytes)
        s.delete("dup")
        assertTrue(fake.files.isEmpty())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aNameWithAnApostropheAndBackslashIsEscapedInTheQuery() = runBlocking {
        val fake = FakeDrive()
        val s = gd(fake)
        s.put("quinn's\\paper", byteArrayOf(1))
        assertContentEquals(byteArrayOf(1), s.get("quinn's\\paper"))
        assertNull(s.get("quinn"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun listReadsTheWholeFolderPagedAndFiltersByPrefixOnTheClient() = runBlocking {
        val fake = FakeDrive(pageSize = 1)
        val s = gd(fake)
        listOf("np-1", "np-2", "other", "np-3").forEach { s.put(it, byteArrayOf(1)) }
        fake.files["dup"] = FakeDrive.F("dup", "np-1", byteArrayOf(1), 99)
        assertEquals(listOf("np-1", "np-2", "np-3"), s.list("np-"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun freeSpaceIsTheLimitLessTheUsageOrNullWhenUnlimited() = runBlocking {
        val fake = FakeDrive()
        assertEquals(600L, gd(fake).freeSpace())
        fake.limit = null
        assertNull(gd(fake).freeSpace())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun anEmptyObjectAndABadChunkSizeAreRefused() = runBlocking {
        assertFailsWith<CloudStoreException> { gd(FakeDrive()).put("x", ByteArray(0)) }
        assertFailsWith<IllegalArgumentException> { gd(FakeDrive(), chunk = 1000) }
        Unit
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun noAnswerAndErrorsThrowInsteadOfReadingAsAbsent() = runBlocking {
        val fake = FakeDrive()
        val s = gd(fake)
        fake.offline = true
        assertFailsWith<CloudStoreException> { s.get("x") }
        assertFailsWith<CloudStoreException> { s.put("x", byteArrayOf(1)) }
        fake.offline = false
        fake.failStatus = 403
        assertEquals(403, assertFailsWith<CloudStoreException> { s.get("x") }.status)
        assertFailsWith<CloudStoreException> { s.list("") }
        assertFailsWith<CloudStoreException> { s.delete("x") }
        Unit
    }
}
