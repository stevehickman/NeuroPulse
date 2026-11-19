package life.neurone.core.cloudauth

import life.neurone.core.cloud.AwsCredentials
import life.neurone.core.cloud.AwsCredentialsProvider
import life.neurone.core.cloud.LiveEndpoints
import life.neurone.core.cloud.LiveJdkTransport
import life.neurone.core.cloud.NppsAwsSigV4
import life.neurone.core.cloud.ObjectStoreContract
import life.neurone.core.cloud.S3ObjectStore
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import org.junit.jupiter.api.Assumptions.assumeTrue
import kotlin.test.Test

private fun env(name: String): String? = System.getenv(name)?.takeIf { it.isNotBlank() }

/**
 * Sends `https://<host>` to `http://<host>`: the S3 adapter always speaks https, and a local test server is plain http on a loopback port.
 * **Only for a loopback host** (SWR-MG-30 (f), `CLAUDE.md` §23): the request is signed with the keys the environment names, so an address on any
 * other host is refused with an [IllegalStateException], which fails the test, and nothing is sent.
 */
internal class PlainHttpTransport(private val inner: HttpTransport) : HttpTransport {
    override suspend fun post(request: HttpRequest): HttpResponse? {
        val plain = "http://" + request.url.removePrefix("https://")
        LiveEndpoints.check(plain, "a live S3 request (NP_LIVE_S3_ENDPOINT)")
        return inner.post(HttpRequest(plain, request.headers, request.body, request.method, request.timeoutSeconds))
    }
}

/**
 * - `NP_LIVE_S3_ENDPOINT` (`host:port`), `NP_LIVE_S3_BUCKET`, `NP_LIVE_S3_REGION`, `NP_LIVE_S3_KEY`, `NP_LIVE_S3_SECRET`: an S3-compatible server and a bucket that
 *   exists. The bucket is emptied before each test. **A server that does not check signatures (moto's default) shows the requests and the conditional
 *   create work, not that the signature is accepted.**
 */
class LiveS3ContractTests : ObjectStoreContract() {
    override fun newStore(): ObjectStore {
        val endpoint = env("NP_LIVE_S3_ENDPOINT")
        assumeTrue(endpoint != null, "NP_LIVE_S3_ENDPOINT is not set: no S3 server to test against")
        LiveEndpoints.check("http://$endpoint", "NP_LIVE_S3_ENDPOINT")
        val keys = AwsCredentialsProvider { AwsCredentials(env("NP_LIVE_S3_KEY")!!, env("NP_LIVE_S3_SECRET")!!) }
        val store = S3ObjectStore(
            PlainHttpTransport(LiveJdkTransport()), env("NP_LIVE_S3_BUCKET")!!, env("NP_LIVE_S3_REGION") ?: "us-east-1", keys, NppsAwsSigV4(),
            { NppsAwsSigV4.amzDate(kotlinx.datetime.Clock.System.now()) }, timeoutSeconds = 30, endpointHost = endpoint,
        )
        kotlinx.coroutines.runBlocking { store.list("").forEach { store.delete(it) } }
        return store
    }
}

/** The downgrade the live S3 suite needs, checked with or without a server (SWR-MG-30 (f)). */
class PlainHttpTransportTests {
    private class Recording : HttpTransport {
        val seen = mutableListOf<String>()
        override suspend fun post(request: HttpRequest): HttpResponse? {
            seen += request.url
            return HttpResponse(200, emptyMap(), ByteArray(0))
        }
    }

    // @verifies SWR-MG-30#f
    @Test
    fun aSignedRequestIsSentOverPlainHttpOnlyToThisMachine() = kotlinx.coroutines.runBlocking {
        val inner = Recording()
        val t = PlainHttpTransport(inner)
        t.post(HttpRequest("https://127.0.0.1:5000/bucket/key", emptyMap(), ByteArray(0), "GET"))
        kotlin.test.assertEquals(listOf("http://127.0.0.1:5000/bucket/key"), inner.seen)
        for (other in listOf("https://s3.example.com/bucket/key", "https://bucket.s3.eu-west-2.amazonaws.com/key", "https://10.1.2.3:9000/b/k")) {
            org.junit.jupiter.api.Assertions.assertThrows(IllegalStateException::class.java) { kotlinx.coroutines.runBlocking { t.post(HttpRequest(other, emptyMap(), ByteArray(0), "GET")) } }
        }
        kotlin.test.assertEquals(1, inner.seen.size, "nothing was sent to a host that is not this machine")
    }
}
