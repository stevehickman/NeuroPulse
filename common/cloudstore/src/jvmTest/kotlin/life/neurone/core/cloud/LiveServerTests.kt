package life.neurone.core.cloud

import kotlinx.coroutines.future.await
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import org.junit.jupiter.api.Assumptions.assumeTrue
import kotlin.test.Test
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
 * **Plain `http` goes to this machine only** (SWR-MG-28 (f), `CLAUDE.md` §23: no unencrypted path, in a test mode either). The servers are named by
 * environment variables and are sent real-looking credentials (a WebDAV password, a signature, signed S3 requests), so [LiveJdkTransport] refuses
 * an `http` address whose host is not a loopback literal, and each suite checks its variable before it makes a store. A variable that names
 * another host over `http` **fails** the test; it is not skipped. An `https` address may be any host: that path is encrypted.
 *
 * - `NP_LIVE_WEBDAV_URL`, `NP_LIVE_WEBDAV_USER`, `NP_LIVE_WEBDAV_PASSWORD`: a WebDAV collection (a Nextcloud, ownCloud or `wsgidav` URL).
 *
 * What a pass shows is that the adapter works against that software, not against a real provider's account.
 */
class LiveJdkTransport : HttpTransport {
    private val client = HttpClient.newBuilder().followRedirects(HttpClient.Redirect.NEVER).connectTimeout(Duration.ofSeconds(15)).build()

    override suspend fun post(request: HttpRequest): HttpResponse? = try {
        // Plain http goes to this machine only (a failed test otherwise); an address that is neither http nor https cannot be sent, which is no answer.
        val scheme = try { URI(request.url).scheme?.lowercase() } catch (e: java.net.URISyntaxException) { null }
        if (scheme == "http") LiveEndpoints.check(request.url, "a live-test request")
        if (scheme != "http" && scheme != "https") throw IllegalArgumentException("not an http or https address")
        val b = JdkRequest.newBuilder(URI.create(request.url)).timeout(Duration.ofSeconds(request.timeoutSeconds.toLong()))
        if (request.method == "GET" || request.method == "HEAD") b.method(request.method, JdkRequest.BodyPublishers.noBody())
        else b.method(request.method, JdkRequest.BodyPublishers.ofByteArray(request.body))
        request.headers.forEach { (k, v) -> b.header(k, v) }
        val a = client.sendAsync(b.build(), JdkResponse.BodyHandlers.ofByteArray()).await()
        HttpResponse(a.statusCode(), a.headers().map().mapValues { it.value.firstOrNull().orEmpty() }, a.body())
    } catch (e: java.io.IOException) {
        null
    } catch (e: IllegalArgumentException) {
        null // an address the JDK will not take is not an answer, and its message would quote the address (the HttpTransport contract)
    }
}

/** Where a live test may send (SWR-MG-28 (f)): `https` anywhere, plain `http` only to this machine. */
object LiveEndpoints {
    /**
     * True for the loopback literals only: `localhost`, `127.x.y.z` and `::1`. No name is resolved, because a name that resolves to this machine
     * today is not a promise about where the next request goes.
     */
    fun isLoopback(host: String?): Boolean {
        val h = host?.removePrefix("[")?.removeSuffix("]")?.lowercase() ?: return false
        return h == "localhost" || h == "::1" || Regex("127\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}").matches(h)
    }

    /**
     * Throws unless [url] is `https`, or `http` to a loopback host. [what] names the setting for the message, which **never holds the address**
     * (it may carry a credential in its query). An [IllegalStateException] fails a test; it is not an assumption, so nothing is skipped.
     */
    fun check(url: String, what: String) {
        val uri = try {
            URI(url)
        } catch (e: java.net.URISyntaxException) {
            throw IllegalStateException("$what is not an address")
        }
        when (uri.scheme?.lowercase()) {
            "https" -> return
            "http" -> if (!isLoopback(uri.host)) throw IllegalStateException("$what is plain http to a host that is not this machine; a live test sends credentials, so it is refused (CLAUDE.md section 23)")
            else -> throw IllegalStateException("$what is neither https nor loopback http")
        }
    }
}

private fun env(name: String): String? = System.getenv(name)?.takeIf { it.isNotBlank() }

class LiveWebDavContractTests : ObjectStoreContract() {
    // wsgidav (4.4.0) checks `If-None-Match: *` and then writes, so racing creates are not arbitrated: 2 of 24 won in the live run (2026-10-30).
    override val arbitratesRacingCreates: Boolean = false

    override fun newStore(): ObjectStore {
        val url = env("NP_LIVE_WEBDAV_URL")
        assumeTrue(url != null, "NP_LIVE_WEBDAV_URL is not set: no WebDAV server to test against")
        LiveEndpoints.check(url!!, "NP_LIVE_WEBDAV_URL")
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
        LiveEndpoints.check(base!!, "NP_LIVE_AZURE_BASEURL")
        val store = AzureBlobObjectStore(
            LiveJdkTransport(), "devstoreaccount1", env("NP_LIVE_AZURE_CONTAINER")!!, token = null, timeoutSeconds = 30,
            baseUrl = base!!, sas = AzureSasProvider { env("NP_LIVE_AZURE_SAS")!! },
        )
        kotlinx.coroutines.runBlocking { store.list("").forEach { store.delete(it) } }
        return store
    }
}

/** The live suites' own guard, which runs with or without a server (SWR-MG-28 (f)). */
class LiveEndpointsTests {
    // @verifies SWR-MG-28#f
    @Test
    fun plainHttpIsRefusedUnlessTheHostIsThisMachine() {
        for (ok in listOf("http://127.0.0.1:10000/devstoreaccount1", "http://localhost:8080/dav/", "http://[::1]:9000/", "http://127.9.8.7/", "https://dav.example.com/remote.php/dav/")) {
            LiveEndpoints.check(ok, "the address")
        }
        val refused = listOf(
            "http://dav.example.com/", "http://10.0.0.5:9000/", "http://192.168.1.2/", "http://localhost.example.com/", "http://127.0.0.1.example.com/",
            "http://user@example.com/", "http://127.0.0.1@example.com/", "ftp://127.0.0.1/", "example.com:9000", "::nonsense::", "http:///x",
        )
        for (bad in refused) {
            val e = org.junit.jupiter.api.Assertions.assertThrows(IllegalStateException::class.java, { LiveEndpoints.check(bad, "the address") }, bad)
            kotlin.test.assertFalse(bad in e.message.orEmpty(), "the message never holds the address")
        }
    }

    // @verifies SWR-MG-28#f
    @Test
    fun theLiveTransportSendsNothingOverPlainHttpToAnotherHost() {
        val e = org.junit.jupiter.api.Assertions.assertThrows(IllegalStateException::class.java) {
            kotlinx.coroutines.runBlocking {
                LiveJdkTransport().post(HttpRequest("http://dav.example.com/x?sig=np-secret", mapOf("Authorization" to "Basic np-secret"), ByteArray(0), "GET", 5))
            }
        }
        kotlin.test.assertFalse("np-secret" in e.message.orEmpty())
    }
}

/** The live suites' transport against the shared transport checks: it is a transport too, and it carries the same credentials. */
class LiveJdkTransportTests : life.neurone.core.net.HttpTransportChecks() {
    override fun transport(): HttpTransport = LiveJdkTransport()

    // @verifies SWR-MG-28#e
    @Test
    fun urlIsNeverLoggedOrSurfaced() = theRequestUrlIsNeverLoggedOrSurfaced()
}
