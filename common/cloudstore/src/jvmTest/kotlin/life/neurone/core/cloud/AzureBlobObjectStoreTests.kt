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
 * A stand-in for the Blob service, written from Microsoft's REST reference (`docs/np_sw_cloudstore_001.md` §3.1): Put Blob
 * needs `x-ms-blob-type` and answers 201; `If-None-Match: *` on an existing blob is refused; Get Blob answers 404 for a
 * missing blob; Delete Blob answers 202; List Blobs answers XML with a `NextMarker`. The documentation does not say whether the
 * refusal is a 412 or a 409 `BlobAlreadyExists`, so the stand-in does either, by [refusal]. **It is not Azure.**
 */
internal class FakeAzureBlob(
    private val token: String = "tok",
    private val pageSize: Int = 2,
    private val refusal: Int = 409,
    private val container: String = "ctr",
) : HttpTransport {
    val blobs = LinkedHashMap<String, ByteArray>()
    val seen = mutableListOf<HttpRequest>()
    var offline = false
    var failStatus: Int? = null
    var failCode: String? = null

    private fun xml(s: String) = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

    override suspend fun post(request: HttpRequest): HttpResponse? = synchronized(this) {
        if (offline) return null
        seen += request
        val uri = URI.create(request.url)
        fun r(code: Int, body: ByteArray = ByteArray(0), headers: Map<String, String> = emptyMap()) = HttpResponse(code, headers, body)
        fun err(code: Int, name: String) = r(code, "<Error><Code>$name</Code></Error>".encodeToByteArray(), mapOf("x-ms-error-code" to name))
        if (request.headers["Authorization"] != "Bearer $token") return err(401, "AuthenticationFailed")
        if ((request.headers["x-ms-version"] ?: "") < "2017-11-09") return err(400, "InvalidHeaderValue")
        failStatus?.let { return err(it, failCode ?: "ServerBusy") }
        val query = (uri.rawQuery ?: "").split('&').filter { it.isNotEmpty() }.associate {
            val kv = it.split('=', limit = 2)
            URLDecoder.decode(kv[0], "UTF-8") to URLDecoder.decode(kv.getOrElse(1) { "" }.replace("+", "%2B"), "UTF-8")
        }
        val path = uri.rawPath
        if (path == "/$container" && request.method == "GET" && query["comp"] == "list" && query["restype"] == "container") {
            val prefix = query["prefix"] ?: ""
            val all = blobs.keys.filter { it.startsWith(prefix) }.sorted()
            val from = query["marker"]?.toInt() ?: 0
            val slice = all.drop(from).take(pageSize)
            val next = if (from + pageSize < all.size) "<NextMarker>${from + pageSize}</NextMarker>" else "<NextMarker />"
            val body = "<?xml version=\"1.0\"?><EnumerationResults ContainerName=\"x\"><Prefix>${xml(prefix)}</Prefix><Blobs>" +
                slice.joinToString("") { "<Blob><Name>${xml(it)}</Name><Properties><Content-Length>${blobs[it]!!.size}</Content-Length></Properties></Blob>" } +
                "</Blobs>$next</EnumerationResults>"
            return r(200, body.encodeToByteArray())
        }
        if (!path.startsWith("/$container/")) return err(404, "ContainerNotFound")
        val name = path.removePrefix("/$container/").split('/').joinToString("/") { URLDecoder.decode(it.replace("+", "%2B"), "UTF-8") }
        when (request.method) {
            "PUT" -> {
                if (request.headers["x-ms-blob-type"] != "BlockBlob") return err(400, "MissingRequiredHeader")
                if (request.headers["If-None-Match"] == "*" && blobs.containsKey(name)) {
                    return if (refusal == 412) err(412, "ConditionNotMet") else err(409, "BlobAlreadyExists")
                }
                blobs[name] = request.body.copyOf()
                r(201)
            }
            "GET" -> blobs[name]?.let { r(200, it.copyOf()) } ?: err(404, "BlobNotFound")
            "DELETE" -> if (blobs.remove(name) != null) r(202) else err(404, "BlobNotFound")
            else -> err(405, "UnsupportedHttpVerb")
        }
    }
}

class AzureBlobContract409Tests : ObjectStoreContract() {
    override fun newStore(): ObjectStore =
        AzureBlobObjectStore(FakeAzureBlob(refusal = 409), "acct", "ctr", AccessToken { "tok" }, timeoutSeconds = 30, baseUrl = "https://acct.blob.core.windows.net")
}

class AzureBlobContract412Tests : ObjectStoreContract() {
    override fun newStore(): ObjectStore =
        AzureBlobObjectStore(FakeAzureBlob(refusal = 412), "acct", "ctr", AccessToken { "tok" }, timeoutSeconds = 30, baseUrl = "https://acct.blob.core.windows.net")
}

/** What the adapter sends, and how it treats the replies it must not guess about. */
class AzureBlobObjectStoreTests {
    private fun store(fake: FakeAzureBlob) =
        AzureBlobObjectStore(fake, "acct", "ctr", AccessToken { "tok" }, timeoutSeconds = 30, baseUrl = "https://acct.blob.core.windows.net")

    // @verifies SWR-MG-11#calls
    @Test
    fun aCreateSendsTheDocumentedPreconditionTheBlobTypeAndABearerToken() = runBlocking {
        val fake = FakeAzureBlob()
        store(fake).createIfAbsent("np-migration-claim.json", byteArrayOf(1))
        val req = fake.seen.single()
        assertEquals("PUT", req.method)
        assertEquals("https://acct.blob.core.windows.net/ctr/np-migration-claim.json", req.url)
        assertEquals("*", req.headers["If-None-Match"])
        assertEquals("BlockBlob", req.headers["x-ms-blob-type"])
        assertEquals("Bearer tok", req.headers["Authorization"])
        assertEquals("2025-11-05", req.headers["x-ms-version"])
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aPlainPutCarriesNoPrecondition() = runBlocking {
        val fake = FakeAzureBlob()
        store(fake).put("x", byteArrayOf(1))
        assertNull(fake.seen.single().headers["If-None-Match"])
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aNameKeepsItsSlashesAndEncodesTheRestInThePath() = runBlocking {
        val fake = FakeAzureBlob()
        store(fake).put("dir/a b+c", byteArrayOf(1))
        assertEquals("https://acct.blob.core.windows.net/ctr/dir/a%20b%2Bc", fake.seen.single().url)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aConflictThatIsNotBlobAlreadyExistsIsNotReadAsTheClaimBeingHeld() = runBlocking {
        val fake = FakeAzureBlob()
        val s = store(fake)
        fake.failStatus = 409
        fake.failCode = "LeaseIdMissing"
        assertEquals(409, assertFailsWith<CloudStoreException> { s.createIfAbsent("x", byteArrayOf(1)) }.status)
        fake.failCode = "BlobAlreadyExists"
        assertEquals(false, s.createIfAbsent("x", byteArrayOf(1)))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun noAnswerAndAnUnmappedStatusThrowInsteadOfReadingAsAbsentOrRefused() = runBlocking {
        val fake = FakeAzureBlob()
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
        val fake = FakeAzureBlob(token = "other")
        assertEquals(401, assertFailsWith<CloudStoreException> { store(fake).put("x", byteArrayOf(1)) }.status)
        assertTrue(fake.blobs.isEmpty())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aListReadsXmlEntitiesAndFollowsTheNextMarker() = runBlocking {
        val fake = FakeAzureBlob(pageSize = 2)
        val s = store(fake)
        for (n in listOf("p/a&b", "p/c<d", "p/e", "p/f", "other")) s.put(n, byteArrayOf(1))
        assertEquals(listOf("p/a&b", "p/c<d", "p/e", "p/f"), s.list("p/"))
        assertTrue(fake.seen.any { "marker=" in it.url })
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun freeSpaceIsUnknownBecauseAnAccountHasNoQuotaTheTokenCanRead() = runBlocking {
        assertNull(store(FakeAzureBlob()).freeSpace())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun percentDecodeIsTheInverseOfPercentEncodeAndLeavesAStrayPercentAlone() {
        for (t in listOf("a b+c/d", "naïve ☃ 𝄞", "100%", "%zz", "")) assertEquals(t, percentDecode(percentEncode(t)))
        assertEquals("50%", percentDecode("50%"))
        assertContentEquals("é".encodeToByteArray(), percentDecode("%C3%A9").encodeToByteArray())
    }
}
