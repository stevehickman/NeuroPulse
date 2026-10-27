package life.neurone.core.cloud

import kotlinx.coroutines.runBlocking
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
 * A stand-in for the OneDrive app folder through Microsoft Graph, written from the Graph pages read 2026-10-27
 * (`docs/np_sw_cloudstore_001.md` §3.1): createUploadSession answers an `uploadUrl`, ranges are multiples of 320 KiB and the
 * intermediate ones answer 202, the upload URL refuses an `Authorization` header with 401, `conflictBehavior: fail` answers 409
 * `nameAlreadyExists` when the name is taken, a content GET answers 302 to a URL that wants no token, delete answers 204, and
 * children are paged by `@odata.nextLink` and include folders. **It is not OneDrive**; whether two sessions racing for one name
 * are arbitrated atomically is not documented, and this fake serialises them.
 */
internal class FakeOneDrive(private val pageSize: Int = 2) : HttpTransport {
    val files = LinkedHashMap<String, ByteArray>()
    val folders = mutableListOf("a folder")
    val seen = mutableListOf<HttpRequest>()
    var offline = false
    var failStatus: Int? = null
    var remaining: Long? = 1234L
    private class Session(val name: String, val behavior: String) { val data = java.io.ByteArrayOutputStream() }
    private val sessions = HashMap<String, Session>()

    private fun r(code: Int, body: String = "", headers: Map<String, String> = emptyMap()) = HttpResponse(code, headers, body.encodeToByteArray())
    private fun conflict() = r(409, """{"error":{"code":"nameAlreadyExists","message":"x"}}""")
    private fun dec(s: String) = URLDecoder.decode(s.replace("+", "%2B"), "UTF-8")
    private fun q(s: String) = "\"" + s.replace("\\", "\\\\").replace("\"", "\\\"") + "\""

    override suspend fun post(request: HttpRequest): HttpResponse? = synchronized(this) {
        if (offline) return null
        seen += request
        val uri = URI.create(request.url)
        val path = uri.rawPath
        if (path.startsWith("/upload/")) {
            if (request.headers.keys.any { it.equals("authorization", true) }) return r(401, """{"error":{"code":"unauthenticated"}}""")
            val s = sessions[path] ?: return r(404)
            val m = Regex("bytes (\\d+)-(\\d+)/(\\d+)").matchEntire(request.headers["Content-Range"] ?: return r(400)) ?: return r(400)
            val (from, to, total) = m.destructured.toList().map { it.toInt() }
            val len = to - from + 1
            if (from != s.data.size() || len != request.body.size) return r(416)
            if (len % 327_680 != 0 && to + 1 != total) return r(400, """{"error":{"code":"invalidRange"}}""")
            s.data.write(request.body)
            if (to + 1 < total) return r(202, """{"nextExpectedRanges":["${to + 1}-"]}""")
            if (s.behavior == "fail" && files.containsKey(s.name)) return conflict()
            val existed = files.containsKey(s.name)
            files[s.name] = s.data.toByteArray()
            return r(if (existed) 200 else 201, """{"name":${q(s.name)}}""")
        }
        if (request.headers["Authorization"] != "Bearer tok" && !path.startsWith("/dl/")) return r(401, """{"error":{"code":"InvalidAuthenticationToken"}}""")
        failStatus?.let { return r(it, """{"error":{"code":"x"}}""") }
        if (path.startsWith("/dl/")) {
            if (request.headers.keys.any { it.equals("authorization", true) }) return r(400)
            return files[dec(path.removePrefix("/dl/"))]?.let { HttpResponse(200, emptyMap(), it.copyOf()) } ?: r(404)
        }
        if (path == "/v1.0/me/drive" && request.method == "GET") {
            return r(200, """{"quota":{"total":10,"used":1${remaining?.let { ",\"remaining\":$it" } ?: ""}}}""")
        }
        if (path == "/v1.0/me/drive/special/approot/children" && request.method == "GET") {
            val all = folders.map { """{"name":${q(it)},"folder":{}}""" } + files.keys.sorted().map { """{"name":${q(it)},"file":{}}""" }
            val from = (uri.rawQuery ?: "").split('&').firstOrNull { it.startsWith("skip=") }?.removePrefix("skip=")?.toInt() ?: 0
            val slice = all.drop(from).take(pageSize)
            val next = if (from + pageSize < all.size) ""","@odata.nextLink":"https://graph.test/v1.0/me/drive/special/approot/children?skip=${from + pageSize}"""" else ""
            return r(200, """{"value":[${slice.joinToString(",")}]$next}""")
        }
        val head = "/v1.0/me/drive/special/approot:/"
        if (!path.startsWith(head)) return r(404)
        val rest = path.removePrefix(head)
        when {
            request.method == "POST" && rest.endsWith(":/createUploadSession") -> {
                val name = dec(rest.removeSuffix(":/createUploadSession"))
                val body = request.body.decodeToString()
                val behavior = Regex("conflictBehavior\":\"(\\w+)").find(body)!!.groupValues[1]
                if (!body.contains("\"name\":${q(name)}")) return r(400)
                if (behavior == "fail" && files.containsKey(name)) return conflict()
                val id = "/upload/${sessions.size}"
                sessions[id] = Session(name, behavior)
                r(200, """{"uploadUrl":"https://up.test$id"}""")
            }
            request.method == "GET" && rest.endsWith(":/content") -> {
                val name = dec(rest.removeSuffix(":/content"))
                if (files.containsKey(name)) r(302, "", mapOf("Location" to "https://dl.test/dl/${java.net.URLEncoder.encode(name, "UTF-8").replace("+", "%20")}")) else r(404)
            }
            request.method == "DELETE" -> if (files.remove(dec(rest)) != null) r(204) else r(404)
            else -> r(405)
        }
    }
}

private fun od(fake: FakeOneDrive, chunk: Int = 327_680) =
    OneDriveObjectStore(fake, AccessToken { "tok" }, timeoutSeconds = 30, baseUrl = "https://graph.test/v1.0", chunkBytes = chunk)

class OneDriveObjectStoreContractTests : ObjectStoreContract() {
    override fun newStore(): ObjectStore = od(FakeOneDrive())
}

class OneDriveObjectStoreTests {
    // @verifies SWR-MG-11#calls
    @Test
    fun aBundleLargerThanOneRangeGoesInRangesToTheUploadUrlWithoutAToken() = runBlocking {
        val fake = FakeOneDrive()
        val bytes = ByteArray(327_680 * 2 + 100) { (it * 7).toByte() }
        od(fake).put("np-migration-bundle.npbk", bytes)
        assertContentEquals(bytes, fake.files["np-migration-bundle.npbk"])
        val puts = fake.seen.filter { it.method == "PUT" }
        assertEquals(listOf("bytes 0-327679/${bytes.size}", "bytes 327680-655359/${bytes.size}", "bytes 655360-${bytes.size - 1}/${bytes.size}"), puts.map { it.headers["Content-Range"] })
        assertTrue(puts.all { r -> r.headers.keys.none { it.equals("authorization", true) } }, "the upload URL takes no token")
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aCreateOpensASessionThatFailsOnAConflictAndAPutOneThatReplaces() = runBlocking {
        val fake = FakeOneDrive()
        val s = od(fake)
        s.createIfAbsent("np-migration-claim.json", byteArrayOf(1))
        s.put("np-migration-claim.json", byteArrayOf(2))
        val bodies = fake.seen.filter { it.url.endsWith(":/createUploadSession") }.map { it.body.decodeToString() }
        assertTrue("\"@microsoft.graph.conflictBehavior\":\"fail\"" in bodies[0])
        assertTrue("\"@microsoft.graph.conflictBehavior\":\"replace\"" in bodies[1])
        assertEquals("https://graph.test/v1.0/me/drive/special/approot:/np-migration-claim.json:/createUploadSession", fake.seen.first().url)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aConflictFoundAtTheLastRangeIsReadAsExists() = runBlocking {
        // The name is free when the session opens and taken before the last byte range lands.
        val fake = FakeOneDrive()
        val racing = object : HttpTransport {
            override suspend fun post(request: HttpRequest): HttpResponse? {
                if (request.method == "PUT") fake.files["x"] = byteArrayOf(9)
                return fake.post(request)
            }
        }
        val s = OneDriveObjectStore(racing, AccessToken { "tok" }, 30, "https://graph.test/v1.0")
        assertEquals(false, s.createIfAbsent("x", byteArrayOf(1)))
        assertContentEquals(byteArrayOf(9), fake.files["x"])
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aReadFollowsTheRedirectWithoutAToken() = runBlocking {
        val fake = FakeOneDrive()
        val s = od(fake)
        s.put("dir/a b", byteArrayOf(5, 6))
        assertContentEquals(byteArrayOf(5, 6), s.get("dir/a b"))
        assertTrue(fake.seen.last().headers.keys.none { it.equals("authorization", true) })
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun namesOneDriveRefusesAreStoredEncodedAndListedDecoded() = runBlocking {
        val fake = FakeOneDrive(pageSize = 100)
        val s = od(fake)
        s.put("dir/a:b?%", byteArrayOf(1))
        assertEquals(setOf("dir%2Fa%3Ab%3F%25"), fake.files.keys)
        assertEquals(listOf("dir/a:b?%"), s.list("dir/"))
        assertEquals("%20x%2E", OneDriveObjectStore.fileName(" x."))
        assertEquals("a b", OneDriveObjectStore.fileName("a b"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun listSkipsFoldersAndFollowsTheNextLink() = runBlocking {
        val fake = FakeOneDrive(pageSize = 1)
        val s = od(fake)
        listOf("np-1", "np-2", "np-3").forEach { s.put(it, byteArrayOf(1)) }
        assertEquals(listOf("np-1", "np-2", "np-3"), s.list("np-"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun freeSpaceIsTheQuotaRemainingOrNull() = runBlocking {
        val fake = FakeOneDrive()
        assertEquals(1234L, od(fake).freeSpace())
        fake.remaining = null
        assertNull(od(fake).freeSpace())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun anEmptyObjectAndABadChunkSizeAreRefused() = runBlocking {
        assertFailsWith<CloudStoreException> { od(FakeOneDrive()).put("x", ByteArray(0)) }
        assertFailsWith<IllegalArgumentException> { od(FakeOneDrive(), chunk = 1000) }
        Unit
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun noAnswerAndErrorsThrowInsteadOfReadingAsAbsentOrRefused() = runBlocking {
        val fake = FakeOneDrive()
        val s = od(fake)
        fake.offline = true
        assertFailsWith<CloudStoreException> { s.get("x") }
        assertFailsWith<CloudStoreException> { s.createIfAbsent("x", byteArrayOf(1)) }
        fake.offline = false
        fake.failStatus = 429
        assertEquals(429, assertFailsWith<CloudStoreException> { s.createIfAbsent("x", byteArrayOf(1)) }.status)
        assertEquals(429, assertFailsWith<CloudStoreException> { s.list("") }.status)
        fake.failStatus = 409 // a 409 that is not nameAlreadyExists
        assertFailsWith<CloudStoreException> { s.createIfAbsent("x", byteArrayOf(1)) }
        Unit
    }
}
