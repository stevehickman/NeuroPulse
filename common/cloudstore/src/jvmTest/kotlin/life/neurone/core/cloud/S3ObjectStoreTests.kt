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
 * A stand-in for S3, written from Amazon's S3 API reference (`docs/np_sw_cloudstore_001.md` §3.1): PutObject answers 200,
 * `If-None-Match: *` on an existing key answers **412**, GetObject answers 404 for a missing key, DeleteObject answers 204,
 * ListObjectsV2 answers XML paged by a continuation token. It also checks that every request is signed (an Authorization
 * header naming `if-none-match` among the signed headers when the request carries it). **It is not S3**, and it does not
 * verify the signature itself: the signature is the Rust core's, checked against Amazon's examples in `sigv4.rs`.
 */
internal class FakeS3(
    private val pageSize: Int = 2,
    private val bucket: String = "bkt",
    private val pathStyle: Boolean = false,
) : HttpTransport {
    val objects = LinkedHashMap<String, ByteArray>()
    val seen = mutableListOf<HttpRequest>()
    var offline = false
    var failStatus: Int? = null

    private fun xml(s: String) = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

    override suspend fun post(request: HttpRequest): HttpResponse? = synchronized(this) {
        if (offline) return null
        seen += request
        fun r(code: Int, body: ByteArray = ByteArray(0)) = HttpResponse(code, emptyMap(), body)
        fun err(code: Int, name: String) = r(code, "<Error><Code>$name</Code></Error>".encodeToByteArray())
        val auth = request.headers["authorization"] ?: return err(403, "AccessDenied")
        val signedHeaders = Regex("SignedHeaders=([^,]*)").find(auth)?.groupValues?.get(1)?.split(';') ?: return err(403, "AccessDenied")
        // Every header the request carries that S3 requires to be signed (x-amz-*) or that sets a condition must be signed.
        val mustSign = request.headers.keys.map { it.lowercase() }.filter { it.startsWith("x-amz-") || it == "if-none-match" }
        if (!signedHeaders.containsAll(mustSign) || "host" !in signedHeaders) return err(403, "SignatureDoesNotMatch")
        failStatus?.let { return err(it, "InternalError") }
        val uri = URI.create(request.url)
        val query = (uri.rawQuery ?: "").split('&').filter { it.isNotEmpty() }.associate {
            val kv = it.split('=', limit = 2)
            URLDecoder.decode(kv[0].replace("+", "%2B"), "UTF-8") to URLDecoder.decode(kv.getOrElse(1) { "" }.replace("+", "%2B"), "UTF-8")
        }
        val rawPath = uri.rawPath
        val keyPath = if (pathStyle) rawPath.removePrefix("/$bucket").removePrefix("/") else rawPath.removePrefix("/")
        if (request.method == "GET" && keyPath.isEmpty()) {
            if (query["list-type"] != "2") return err(400, "InvalidArgument")
            val prefix = query["prefix"] ?: ""
            val all = objects.keys.filter { it.startsWith(prefix) }.sorted()
            val from = query["continuation-token"]?.toInt() ?: 0
            val slice = all.drop(from).take(pageSize)
            val more = from + pageSize < all.size
            val body = "<?xml version=\"1.0\"?><ListBucketResult><Name>$bucket</Name><Prefix>${xml(prefix)}</Prefix><KeyCount>${slice.size}</KeyCount>" +
                "<IsTruncated>$more</IsTruncated>" + (if (more) "<NextContinuationToken>${from + pageSize}</NextContinuationToken>" else "") +
                slice.joinToString("") { "<Contents><Key>${xml(it)}</Key><Size>${objects[it]!!.size}</Size></Contents>" } + "</ListBucketResult>"
            return r(200, body.encodeToByteArray())
        }
        val key = keyPath.split('/').joinToString("/") { URLDecoder.decode(it.replace("+", "%2B"), "UTF-8") }
        when (request.method) {
            "PUT" -> {
                if (request.headers["x-amz-content-sha256"] != "UNSIGNED-PAYLOAD") return err(400, "XAmzContentSHA256Mismatch")
                if (request.headers["if-none-match"] == "*" && objects.containsKey(key)) return err(412, "PreconditionFailed")
                objects[key] = request.body.copyOf()
                r(200)
            }
            "GET" -> objects[key]?.let { r(200, it.copyOf()) } ?: err(404, "NoSuchKey")
            "DELETE" -> { objects.remove(key); r(204) }
            else -> err(405, "MethodNotAllowed")
        }
    }
}

/** A signer that signs nothing: it records what it was asked and returns the headers a real one returns, with a marker signature. */
internal class RecordingSigner : AwsSigV4 {
    val asked = mutableListOf<AwsSigningRequest>()
    override fun sign(request: AwsSigningRequest): Map<String, String> {
        asked += request
        val names = (request.headers.keys.map { it.lowercase() } + listOf("host", "x-amz-content-sha256", "x-amz-date") +
            (if (request.credentials.sessionToken != null) listOf("x-amz-security-token") else emptyList())).sorted()
        return buildMap {
            put("x-amz-date", request.amzDate)
            put("x-amz-content-sha256", request.payloadHash)
            request.credentials.sessionToken?.let { put("x-amz-security-token", it) }
            put("authorization", "AWS4-HMAC-SHA256 Credential=${request.credentials.accessKeyId}/x, SignedHeaders=${names.joinToString(";")}, Signature=00")
        }
    }
}

private val KEYS = AwsCredentialsProvider { AwsCredentials("AKIDEXAMPLE", "secret") }
private fun now() = "20130524T000000Z"

private fun s3(fake: FakeS3, signer: AwsSigV4 = RecordingSigner(), pathStyle: Boolean = false, endpointHost: String? = null, credentials: AwsCredentialsProvider = KEYS) =
    S3ObjectStore(fake, "bkt", "us-east-1", credentials, signer, ::now, timeoutSeconds = 30, pathStyle = pathStyle, endpointHost = endpointHost)

class S3ObjectStoreContractTests : ObjectStoreContract() {
    override fun newStore(): ObjectStore = s3(FakeS3())
}

class S3PathStyleContractTests : ObjectStoreContract() {
    override fun newStore(): ObjectStore = s3(FakeS3(pathStyle = true), pathStyle = true)
}

/** What the adapter describes to the signer, sends, and refuses to guess about. */
class S3ObjectStoreTests {
    // @verifies SWR-MG-11#calls
    @Test
    fun aCreateAsksTheSignerToSignTheConditionAndSendsTheSignedHeaders() = runBlocking {
        val fake = FakeS3(); val signer = RecordingSigner()
        s3(fake, signer).createIfAbsent("np-migration-claim.json", byteArrayOf(1))
        val asked = signer.asked.single()
        assertEquals("PUT", asked.method)
        assertEquals("bkt.s3.us-east-1.amazonaws.com", asked.host)
        assertEquals("/np-migration-claim.json", asked.path)
        assertEquals("UNSIGNED-PAYLOAD", asked.payloadHash)
        assertEquals("*", asked.headers["if-none-match"], "the condition is part of what is signed")
        assertEquals("us-east-1", asked.region); assertEquals("s3", asked.service); assertEquals("20130524T000000Z", asked.amzDate)
        val sent = fake.seen.single()
        assertEquals("https://bkt.s3.us-east-1.amazonaws.com/np-migration-claim.json", sent.url)
        assertEquals("*", sent.headers["if-none-match"])
        assertEquals("UNSIGNED-PAYLOAD", sent.headers["x-amz-content-sha256"])
        assertTrue(sent.headers["authorization"]!!.startsWith("AWS4-HMAC-SHA256 "))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aPlainPutCarriesNoCondition() = runBlocking {
        val fake = FakeS3(); val signer = RecordingSigner()
        s3(fake, signer).put("x", byteArrayOf(1))
        assertNull(signer.asked.single().headers["if-none-match"])
        assertNull(fake.seen.single().headers["if-none-match"])
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun theKeyIsEncodedOnceInThePathAndTheListQueryIsHandedOverRaw() = runBlocking {
        val fake = FakeS3(); val signer = RecordingSigner()
        val s = s3(fake, signer)
        s.put("dir/a b+c", byteArrayOf(1))
        assertEquals("/dir/a%20b%2Bc", signer.asked.last().path)
        s.list("dir/a b")
        val q = signer.asked.last()
        assertEquals("/", q.path)
        assertEquals(listOf("list-type" to "2", "prefix" to "dir/a b"), q.query, "raw pairs: the signer encodes and sorts them")
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun pathStyleAndACustomEndpointAddressTheBucketInThePath() = runBlocking {
        val a = FakeS3(pathStyle = true); val sa = RecordingSigner()
        s3(a, sa, pathStyle = true).put("k", byteArrayOf(1))
        assertEquals("s3.us-east-1.amazonaws.com", sa.asked.single().host)
        assertEquals("/bkt/k", sa.asked.single().path)
        val b = FakeS3(pathStyle = true); val sb = RecordingSigner()
        s3(b, sb, endpointHost = "objects.example.test").put("k", byteArrayOf(1))
        assertEquals("objects.example.test", sb.asked.single().host)
        assertEquals("https://objects.example.test/bkt/k", b.seen.single().url)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aSessionTokenIsHandedToTheSignerAndSentBack() = runBlocking {
        val fake = FakeS3(); val signer = RecordingSigner()
        s3(fake, signer, credentials = { AwsCredentials("AKID", "s", "tok") }).put("k", byteArrayOf(1))
        assertEquals("tok", signer.asked.single().credentials.sessionToken)
        assertEquals("tok", fake.seen.single().headers["x-amz-security-token"])
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aConflictIsNeitherCreatedNorExistsSoItThrows() = runBlocking {
        val fake = FakeS3()
        val s = s3(fake)
        fake.failStatus = 409
        assertEquals(409, assertFailsWith<CloudStoreException> { s.createIfAbsent("x", byteArrayOf(1)) }.status)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun noAnswerAnAccessDeniedAndAServerErrorThrowInsteadOfReadingAsAbsentOrRefused() = runBlocking {
        val fake = FakeS3()
        val s = s3(fake)
        fake.offline = true
        assertFailsWith<CloudStoreException> { s.get("x") }
        assertFailsWith<CloudStoreException> { s.createIfAbsent("x", byteArrayOf(1)) }
        fake.offline = false
        fake.failStatus = 403
        assertEquals(403, assertFailsWith<CloudStoreException> { s.get("x") }.status)
        fake.failStatus = 500
        assertFailsWith<CloudStoreException> { s.delete("x") }
        assertEquals(500, assertFailsWith<CloudStoreException> { s.list("") }.status)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aListFollowsTheContinuationTokenAndReadsXmlEntities() = runBlocking {
        val fake = FakeS3(pageSize = 2)
        val s = s3(fake)
        for (n in listOf("p/a&b", "p/c<d", "p/e", "p/f", "other")) s.put(n, byteArrayOf(1))
        assertEquals(listOf("p/a&b", "p/c<d", "p/e", "p/f"), s.list("p/"))
        assertTrue(fake.seen.any { "continuation-token=" in it.url })
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aTruncatedListWithNoTokenThrowsInsteadOfStoppingEarly() = runBlocking {
        val broken = object : HttpTransport {
            override suspend fun post(request: HttpRequest): HttpResponse? =
                HttpResponse(200, emptyMap(), "<ListBucketResult><IsTruncated>true</IsTruncated><Contents><Key>a</Key></Contents></ListBucketResult>".encodeToByteArray())
        }
        val s = S3ObjectStore(broken, "bkt", "us-east-1", KEYS, RecordingSigner(), ::now, timeoutSeconds = 30)
        assertFailsWith<CloudStoreException> { s.list("") }
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun freeSpaceIsUnknownBecauseABucketHasNoQuotaTheCredentialsCanRead() = runBlocking {
        assertNull(s3(FakeS3()).freeSpace())
    }
}
