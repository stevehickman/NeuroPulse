package life.neurone.core.cloud

import kotlinx.coroutines.runBlocking
import kotlinx.datetime.Instant
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The Kotlin binding to the Rust core's AWS Signature Version 4 signer, against Amazon's own published S3 examples
 * (`common/npps-core/src/sigv4.rs` checks the same four in Rust). Needs the native core, as the other `:core` tests do.
 */
class NppsAwsSigV4Tests {
    private val keys = AwsCredentials("AKIAIOSFODNN7EXAMPLE", "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY")
    private val empty = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

    private fun request(path: String, query: List<Pair<String, String>>, headers: Map<String, String>) = AwsSigningRequest(
        method = "GET", host = "examplebucket.s3.amazonaws.com", path = path, query = query, headers = headers,
        payloadHash = empty, region = "us-east-1", service = "s3", credentials = keys, amzDate = "20130524T000000Z",
    )

    // @verifies SWR-MG-11#calls
    @Test
    fun theKotlinBindingReproducesAmazonsPublishedGetObjectSignature() {
        val h = NppsAwsSigV4().sign(request("/test.txt", emptyList(), mapOf("range" to "bytes=0-9")))
        assertEquals(
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request, " +
                "SignedHeaders=host;range;x-amz-content-sha256;x-amz-date, " +
                "Signature=f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41",
            h["authorization"],
        )
        assertEquals("20130524T000000Z", h["x-amz-date"])
        assertEquals(empty, h["x-amz-content-sha256"])
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun theKotlinBindingReproducesAmazonsPublishedListObjectsSignatureWithTheQueryGivenOutOfOrder() {
        val h = NppsAwsSigV4().sign(request("/", listOf("prefix" to "J", "max-keys" to "2"), emptyMap()))
        assertTrue(h["authorization"]!!.endsWith("Signature=34b48302e7b5fa45bde8084f4b7868a86f0a534bc59db6670ed5711ef69dc6f7"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aMalformedRequestIsAnErrorFromTheCoreNotASignature() {
        val bad = AwsSigningRequest("GET", "h", "relative", emptyList(), emptyMap(), empty, "us-east-1", "s3", keys, "20130524T000000Z")
        val failed = runCatching { NppsAwsSigV4().sign(bad) }.isFailure
        assertTrue(failed)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun theAmzDateIsTheUtcFormTheSignerWants() {
        assertEquals("20130524T000000Z", NppsAwsSigV4.amzDate(Instant.parse("2013-05-24T00:00:00Z")))
        assertEquals("20261027T200917Z", NppsAwsSigV4.amzDate(Instant.parse("2026-10-27T20:09:17.123456Z")))
        assertEquals("20260101T000000Z", NppsAwsSigV4.amzDate(Instant.parse("2026-01-01T00:00:00Z")))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun theS3AdapterWithTheRealSignerSendsAnAuthorizationThatNamesTheConditionAsSigned() = runBlocking {
        val seen = mutableListOf<HttpRequest>()
        val transport = object : HttpTransport {
            override suspend fun post(request: HttpRequest): HttpResponse? { seen += request; return HttpResponse(200, emptyMap(), ByteArray(0)) }
        }
        val store: ObjectStore = S3ObjectStore(
            transport, "examplebucket", "us-east-1", AwsCredentialsProvider { keys }, NppsAwsSigV4(), { "20130524T000000Z" }, timeoutSeconds = 30,
        )
        assertTrue(store.createIfAbsent("np-migration-claim.json", byteArrayOf(1)))
        val auth = seen.single().headers["authorization"]!!
        assertTrue("SignedHeaders=content-type;host;if-none-match;x-amz-content-sha256;x-amz-date," in auth, auth)
        assertEquals("UNSIGNED-PAYLOAD", seen.single().headers["x-amz-content-sha256"])
    }
}
