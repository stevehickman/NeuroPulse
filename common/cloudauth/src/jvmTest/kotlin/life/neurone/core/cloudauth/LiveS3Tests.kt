package life.neurone.core.cloudauth

import life.neurone.core.cloud.AwsCredentials
import life.neurone.core.cloud.AwsCredentialsProvider
import life.neurone.core.cloud.LiveJdkTransport
import life.neurone.core.cloud.NppsAwsSigV4
import life.neurone.core.cloud.ObjectStoreContract
import life.neurone.core.cloud.S3ObjectStore
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import org.junit.jupiter.api.Assumptions.assumeTrue

private fun env(name: String): String? = System.getenv(name)?.takeIf { it.isNotBlank() }

/** Sends `https://<host>` to `http://<host>`: the S3 adapter always speaks https, and a local test server is plain http on a loopback port. */
private class PlainHttpTransport(private val inner: HttpTransport) : HttpTransport {
    override suspend fun post(request: HttpRequest): HttpResponse? =
        inner.post(HttpRequest("http://" + request.url.removePrefix("https://"), request.headers, request.body, request.method, request.timeoutSeconds))
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
        val keys = AwsCredentialsProvider { AwsCredentials(env("NP_LIVE_S3_KEY")!!, env("NP_LIVE_S3_SECRET")!!) }
        val store = S3ObjectStore(
            PlainHttpTransport(LiveJdkTransport()), env("NP_LIVE_S3_BUCKET")!!, env("NP_LIVE_S3_REGION") ?: "us-east-1", keys, NppsAwsSigV4(),
            { NppsAwsSigV4.amzDate(kotlinx.datetime.Clock.System.now()) }, timeoutSeconds = 30, endpointHost = endpoint,
        )
        kotlinx.coroutines.runBlocking { store.list("").forEach { store.delete(it) } }
        return store
    }
}
