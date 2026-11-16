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
 * **Plain `http` goes to a loopback address and nowhere else** (`SWR-MG-28` (c), `CLAUDE.md` §23, `OI-PRIV-15` point 6): the server is named by an
 * environment variable, and the requests carry a WebDAV Basic login, an Azure shared access signature and signed S3 requests, so
 * [requireLoopbackForPlainHttp] refuses any other host before anything is sent, and the test fails instead of being skipped. An `https` server may
 * be anywhere.
 *
 * - `NP_LIVE_WEBDAV_URL`, `NP_LIVE_WEBDAV_USER`, `NP_LIVE_WEBDAV_PASSWORD`: a WebDAV collection (a Nextcloud, ownCloud or `wsgidav` URL).
 *
 * What a pass shows is that the adapter works against that software, not against a real provider's account.
 */
class LiveJdkTransport : HttpTransport {
    private val client = HttpClient.newBuilder().followRedirects(HttpClient.Redirect.NEVER).connectTimeout(Duration.ofSeconds(15)).build()

    override suspend fun post(request: HttpRequest): HttpResponse? = try {
        requireLoopbackForPlainHttp(request.url)
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

/**
 * Throws unless [url] is `https`, or is `http` to a loopback address (`127.0.0.0/8`, `::1`, or a name that resolves to one). Anything else,
 * a name that does not resolve included, is refused. The message never holds the address, which may carry a credential.
 */
fun requireLoopbackForPlainHttp(url: String) {
    if (url.startsWith("https://")) return
    val host = try {
        if (url.startsWith("http://")) URI.create(url).host else null
    } catch (e: IllegalArgumentException) {
        null
    }
    val loopback = host != null && try {
        java.net.InetAddress.getByName(host.removeSurrounding("[", "]")).isLoopbackAddress
    } catch (e: java.net.UnknownHostException) {
        false
    }
    check(loopback) { "a live test sends plain http to a loopback address only (CLAUDE.md §23); this server is somewhere else, or is not http or https" }
}

private fun env(name: String): String? = System.getenv(name)?.takeIf { it.isNotBlank() }

class PlainHttpGuardTests {
    // @verifies SWR-MG-28#c
    @Test
    fun plainHttpIsSentToALoopbackAddressAndRefusedForEveryOtherHostBeforeAnythingIsSent() {
        for (ok in listOf("http://127.0.0.1:10000/devstoreaccount1/c?sig=x", "http://127.9.9.9/x", "http://[::1]:8080/x", "http://localhost:5000/x", "https://dav.example.test/remote.php")) {
            requireLoopbackForPlainHttp(ok)
        }
        // 192.0.2.0/24 and 2001:db8::/32 are documentation ranges; `.invalid` never resolves. None of these is contacted.
        for (bad in listOf(
            "http://192.0.2.1:10000/devstoreaccount1/c?sig=x", "http://10.0.0.5/x", "http://0.0.0.0/x", "http://[2001:db8::1]/x", "http://storage.invalid/x",
            "http://127.0.0.1.storage.invalid/x", "http://user:pw@192.0.2.1/x", "ftp://127.0.0.1/x", "127.0.0.1:9000/x", "http:///x", "",
        )) {
            val e = org.junit.jupiter.api.Assertions.assertThrows(IllegalStateException::class.java, { requireLoopbackForPlainHttp(bad) }, bad)
            org.junit.jupiter.api.Assertions.assertFalse(bad.isNotEmpty() && bad in e.message!!, "the message quotes the address")
        }
        // The transport the live tests use refuses before it connects: a thrown refusal, not the null of "no answer", so a test fails loudly.
        org.junit.jupiter.api.Assertions.assertThrows(IllegalStateException::class.java) {
            kotlinx.coroutines.runBlocking { LiveJdkTransport().post(HttpRequest("http://192.0.2.1/dav/x", mapOf("Authorization" to "Basic dTpw"), ByteArray(0), "GET", 2)) }
        }
    }
}

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
