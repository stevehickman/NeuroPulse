package life.neurone.core.cloud

import kotlinx.coroutines.runBlocking
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import java.net.URI
import java.net.URLDecoder
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * A stand-in for the Cloud Storage JSON API, written from its documentation (`docs/np_sw_cloudstore_001.md` §3.1): it refuses
 * `ifGenerationMatch=0` with 412 when a live object exists, answers 404 for a missing one, pages a listing, and serialises its
 * writes. **It is not Google**: it shows what the adapter sends and how it reads the replies, not that Cloud Storage answers so.
 */
internal class FakeGcs(private val token: String = "tok", private val pageSize: Int = 2) : HttpTransport {
    val objects = LinkedHashMap<String, ByteArray>()
    val seen = mutableListOf<String>()
    var offline = false
    var failStatus: Int? = null

    private fun q(uri: URI): Map<String, String> = (uri.rawQuery ?: "").split('&').filter { it.isNotEmpty() }.associate {
        val kv = it.split('=', limit = 2)
        URLDecoder.decode(kv[0], "UTF-8") to URLDecoder.decode(kv.getOrElse(1) { "" }, "UTF-8")
    }

    private fun decodePath(s: String) = URLDecoder.decode(s.replace("+", "%2B"), "UTF-8")

    override suspend fun post(request: HttpRequest): HttpResponse? = synchronized(this) {
        if (offline) return null
        val uri = URI.create(request.url)
        seen += "${request.method} ${request.url}"
        fun r(code: Int, body: ByteArray = ByteArray(0)) = HttpResponse(code, emptyMap(), body)
        if (request.headers["Authorization"] != "Bearer $token") return r(401)
        failStatus?.let { return r(it) }
        val query = q(uri)
        val path = uri.rawPath
        val up = "/upload/storage/v1/b/bkt/o"
        val objs = "/storage/v1/b/bkt/o"
        when {
            request.method == "POST" && path == up -> {
                val name = query["name"]!!
                if (query["ifGenerationMatch"] == "0" && objects.containsKey(name)) return r(412)
                objects[name] = request.body.copyOf()
                r(200, """{"name":"$name"}""".encodeToByteArray())
            }
            request.method == "GET" && path == objs -> {
                val prefix = query["prefix"] ?: ""
                val all = objects.keys.filter { it.startsWith(prefix) }.sorted()
                val from = query["pageToken"]?.toInt() ?: 0
                val slice = all.drop(from).take(pageSize)
                val next = if (from + pageSize < all.size) ""","nextPageToken":"${from + pageSize}"""" else ""
                val items = slice.joinToString(",") { """{"name":"${it.replace("\"", "\\\"")}"}""" }
                r(200, """{"items":[$items]$next}""".encodeToByteArray())
            }
            path.startsWith("$objs/") -> {
                val name = decodePath(path.removePrefix("$objs/"))
                when (request.method) {
                    "GET" -> objects[name]?.let { r(200, it.copyOf()) } ?: r(404)
                    "DELETE" -> if (objects.remove(name) != null) r(204) else r(404)
                    else -> r(405)
                }
            }
            else -> r(404)
        }
    }
}

class GcsObjectStoreContractTests : ObjectStoreContract() {
    override fun newStore(): ObjectStore = GcsObjectStore(FakeGcs(), "bkt", AccessToken { "tok" }, timeoutSeconds = 30)
}

/** What the adapter sends, and how it treats the replies it must not guess about. */
class GcsObjectStoreTests {
    private fun store(fake: FakeGcs) = GcsObjectStore(fake, "bkt", AccessToken { "tok" }, timeoutSeconds = 30)

    // @verifies SWR-MG-11#calls
    @Test
    fun aCreateSendsTheDocumentedPreconditionAndABearerToken() = runBlocking {
        val fake = FakeGcs()
        store(fake).createIfAbsent("np-migration-claim.json", byteArrayOf(1))
        assertEquals(
            "POST https://storage.googleapis.com/upload/storage/v1/b/bkt/o?uploadType=media&name=np-migration-claim.json&ifGenerationMatch=0",
            fake.seen.single(),
        )
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aPlainPutCarriesNoPrecondition() = runBlocking {
        val fake = FakeGcs()
        store(fake).put("x", byteArrayOf(1))
        assertTrue("ifGenerationMatch" !in fake.seen.single())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun noAnswerAndAnUnmappedStatusThrowInsteadOfReadingAsAbsentOrRefused() = runBlocking {
        val fake = FakeGcs()
        val s = store(fake)
        fake.offline = true
        assertFailsWith<CloudStoreException> { s.get("x") }
        assertFailsWith<CloudStoreException> { s.createIfAbsent("x", byteArrayOf(1)) }
        fake.offline = false
        fake.failStatus = 503
        assertFailsWith<CloudStoreException> { s.get("x") }
        assertFailsWith<CloudStoreException> { s.createIfAbsent("x", byteArrayOf(1)) }
        assertFailsWith<CloudStoreException> { s.delete("x") }
        assertEquals(503, assertFailsWith<CloudStoreException> { s.list("") }.status)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aRefusedTokenThrowsAndWritesNothing() = runBlocking {
        val fake = FakeGcs(token = "other")
        val s = store(fake)
        assertEquals(401, assertFailsWith<CloudStoreException> { s.put("x", byteArrayOf(1)) }.status)
        assertTrue(fake.objects.isEmpty())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun freeSpaceIsUnknownBecauseABucketHasNoReadableQuota() = runBlocking {
        assertNull(store(FakeGcs()).freeSpace())
    }
}
