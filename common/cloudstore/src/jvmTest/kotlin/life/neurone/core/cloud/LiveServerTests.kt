package life.neurone.core.cloud

import kotlinx.coroutines.future.await
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import org.junit.jupiter.api.Assumptions.assumeTrue
import java.net.URI
import java.net.http.HttpClient
import java.net.http.HttpRequest as JdkRequest
import java.net.http.HttpResponse as JdkResponse
import java.time.Duration
import java.util.Base64

/**
 * The shared contract suite run against **real server software**, when one is named in the environment; otherwise every test here is skipped
 * (an aborted assumption, so a run with no server is green and says "skipped", not "passed"). These are not part of CI: they need a server the
 * tester starts, and they speak plain `http` to a loopback address, which the adapters' callers (`WebDavAddress`) would refuse for a person's login.
 *
 * - `NP_LIVE_WEBDAV_URL`, `NP_LIVE_WEBDAV_USER`, `NP_LIVE_WEBDAV_PASSWORD`: a WebDAV collection (a Nextcloud, ownCloud or `wsgidav` URL).
 *
 * What a pass shows is that the adapter works against that software, not against a real provider's account.
 */
class LiveJdkTransport : HttpTransport {
    private val client = HttpClient.newBuilder().followRedirects(HttpClient.Redirect.NEVER).connectTimeout(Duration.ofSeconds(15)).build()

    override suspend fun post(request: HttpRequest): HttpResponse? = try {
        val b = JdkRequest.newBuilder(URI.create(request.url)).timeout(Duration.ofSeconds(request.timeoutSeconds.toLong()))
        if (request.method == "GET" || request.method == "HEAD") b.method(request.method, JdkRequest.BodyPublishers.noBody())
        else b.method(request.method, JdkRequest.BodyPublishers.ofByteArray(request.body))
        request.headers.forEach { (k, v) -> b.header(k, v) }
        val a = client.sendAsync(b.build(), JdkResponse.BodyHandlers.ofByteArray()).await()
        HttpResponse(a.statusCode(), a.headers().map().mapValues { it.value.firstOrNull().orEmpty() }, a.body())
    } catch (e: java.io.IOException) {
        null
    }
}

private fun env(name: String): String? = System.getenv(name)?.takeIf { it.isNotBlank() }

class LiveWebDavContractTests : ObjectStoreContract() {
    // wsgidav (4.4.0) checks `If-None-Match: *` and then writes, so racing creates are not arbitrated: 2 of 24 won in the live run (2026-10-30).
    override val arbitratesRacingCreates: Boolean = false

    override fun newStore(): ObjectStore {
        val url = env("NP_LIVE_WEBDAV_URL")
        assumeTrue(url != null, "NP_LIVE_WEBDAV_URL is not set: no WebDAV server to test against")
        val auth = "Basic " + Base64.getEncoder().encodeToString("${env("NP_LIVE_WEBDAV_USER")}:${env("NP_LIVE_WEBDAV_PASSWORD")}".toByteArray())
        // A shared server keeps what a test leaves, and the contract assumes a store that starts empty: each test gets its own folder.
        return WebDavObjectStore(LiveJdkTransport(), { WebDavLogin(url!!, auth) }, timeoutSeconds = 30, folder = "np-live-" + java.util.UUID.randomUUID().toString().take(8))
    }
}

/**
 * - `NP_LIVE_AZURE_BASEURL` (e.g. Azurite's `http://127.0.0.1:10000/devstoreaccount1`), `NP_LIVE_AZURE_CONTAINER` and `NP_LIVE_AZURE_SAS`: a container and a
 *   service shared access signature for it. The container is emptied before each test, because the signature is for one container.
 */
class LiveAzureSasContractTests : ObjectStoreContract() {
    override fun newStore(): ObjectStore {
        val base = env("NP_LIVE_AZURE_BASEURL")
        assumeTrue(base != null, "NP_LIVE_AZURE_BASEURL is not set: no Azure Blob server to test against")
        val store = AzureBlobObjectStore(
            LiveJdkTransport(), "devstoreaccount1", env("NP_LIVE_AZURE_CONTAINER")!!, token = null, timeoutSeconds = 30,
            baseUrl = base!!, sas = AzureSasProvider { env("NP_LIVE_AZURE_SAS")!! },
        )
        kotlinx.coroutines.runBlocking { store.list("").forEach { store.delete(it) } }
        return store
    }
}
