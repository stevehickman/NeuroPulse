package life.neurone.core.cloud

import kotlinx.coroutines.runBlocking
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * A provider stand-in whose replies are altered on the way back, so a reply can name an address the real service never would: plain `http`, or
 * another host. Every request the adapter then makes is recorded, so a test can show nothing was sent to that address.
 */
private class Altered(private val inner: HttpTransport, private val from: String, private val to: String) : HttpTransport {
    val seen = mutableListOf<HttpRequest>()

    override suspend fun post(request: HttpRequest): HttpResponse? {
        seen += request
        // A request the adapter should never have made is answered as if it had worked, so a missing check shows as a test failure and not as a 404.
        if (request.url.startsWith(to)) return HttpResponse(200, emptyMap(), """{"value":[],"singleFile":{}}""".encodeToByteArray())
        val r = inner.post(request) ?: return null
        return HttpResponse(r.status, r.headers.mapValues { it.value.replace(from, to) }, String(r.body, Charsets.ISO_8859_1).replace(from, to).toByteArray(Charsets.ISO_8859_1))
    }
}

/**
 * SWR-MG-30 (b): an address a provider's reply names is followed only when it is `https`, and one that will be sent the person's token only
 * under the service's own address. Run against the same stand-ins as the adapters' other tests, with the reply altered.
 */
class ServerUrlTests {
    private val bytes = ByteArray(400_000) { it.toByte() }

    private fun refused(altered: Altered, to: String, call: suspend () -> Unit) {
        val e = assertFailsWith<CloudStoreException> { runBlocking { call() } }
        assertTrue(altered.seen.none { it.url.startsWith(to) }, "nothing was sent to the address the reply named")
        assertFalse(to.substringAfter("://") in e.message.orEmpty(), "the message does not hold the address: ${e.message}")
    }

    // @verifies SWR-MG-30#b
    @Test
    fun theCheckTakesHttpsOnlyAndAPrefixWhenOneIsGiven() {
        assertEquals("https://up.example/x?sig=1", serverUrl("https://up.example/x?sig=1", "the address"))
        assertEquals("https://graph.example/v1.0/next", serverUrl("https://graph.example/v1.0/next", "the address", mustStartWith = "https://graph.example/v1.0/"))
        val bad = listOf(
            "http://up.example/x", "HTTPS://up.example/x", "//up.example/x", "/relative", "", "https://", "https:///x", "ftp://up.example/x",
            "https://up.example/x y", "https://up.example/x\nHost: other", "file:///etc/passwd", "javascript:alert(1)",
        )
        for (b in bad) assertFailsWith<CloudStoreException>(b) { serverUrl(b, "the address") }
        val elsewhere = listOf(
            "https://graph.example.evil.test/v1.0/next", "https://graph.example@evil.test/v1.0/next", "https://evil.test/https://graph.example/v1.0/",
            "https://graph.example/v1.0", "https://graph.example/v2.0/next", "https://graph.example:444/v1.0/next",
        )
        for (b in elsewhere) {
            val e = assertFailsWith<CloudStoreException>(b) { serverUrl(b, "the address", mustStartWith = "https://graph.example/v1.0/") }
            assertFalse("evil" in e.message.orEmpty())
        }
    }

    // @verifies SWR-MG-30#b
    @Test
    fun oneDriveSendsItsTokenToANextPageOnlyUnderItsOwnGraphAddress() {
        fun store(t: HttpTransport) = OneDriveObjectStore(t, AccessToken { "tok" }, 30, "https://graph.test/v1.0", 327_680)
        for (to in listOf("https://evil.test/v1.0", "http://graph.test/v1.0", "https://graph.test.evil.test/v1.0", "https://graph.test/beta")) {
            val fake = FakeOneDrive(pageSize = 2)
            runBlocking { for (n in listOf("np-1", "np-2", "np-3", "np-4")) store(fake).put(n, bytes) }
            val altered = Altered(fake, "\"@odata.nextLink\":\"https://graph.test/v1.0", "\"@odata.nextLink\":\"$to")
            val e = assertFailsWith<CloudStoreException> { runBlocking { store(altered).list("np-") } }
            assertTrue(altered.seen.all { it.url.startsWith("https://graph.test/v1.0/") }, "the token went nowhere but Graph: $to")
            assertFalse("evil" in e.message.orEmpty())
        }
        // The service's own next page is still followed.
        val fake = FakeOneDrive(pageSize = 2)
        runBlocking {
            for (n in listOf("np-1", "np-2", "np-3")) store(fake).put(n, bytes)
            assertEquals(listOf("np-1", "np-2", "np-3"), store(fake).list("np-"))
        }
    }

    // @verifies SWR-MG-30#b
    @Test
    fun oneDriveUploadsAndDownloadsOnlyOverHttps() {
        fun store(t: HttpTransport) = OneDriveObjectStore(t, AccessToken { "tok" }, 30, "https://graph.test/v1.0", 327_680)
        val up = Altered(FakeOneDrive(), "https://up.test", "http://up.test")
        refused(up, "http://up.test") { store(up).put("np-bundle", bytes) }
        refused(up, "http://up.test") { store(up).createIfAbsent("np-claim", bytes) }

        val fake = FakeOneDrive()
        runBlocking { store(fake).put("np-bundle", bytes) }
        val down = Altered(fake, "https://dl.test", "http://dl.test")
        refused(down, "http://dl.test") { store(down).get("np-bundle") }
    }

    // @verifies SWR-MG-30#b
    @Test
    fun googleDriveUploadsOnlyToAnHttpsSession() {
        fun store(t: HttpTransport) = GoogleDriveObjectStore(t, AccessToken { "tok" }, 30, "https://drive.test", 262_144)
        val fake = FakeDrive()
        val create = Altered(fake, "https://drive.test/session", "http://drive.test/session")
        refused(create, "http://drive.test/session") { store(create).put("np-bundle", bytes) }
        runBlocking { store(fake).put("np-bundle", bytes) }
        val update = Altered(fake, "https://drive.test/session", "http://drive.test/session")
        refused(update, "http://drive.test/session") { store(update).put("np-bundle", bytes) }
    }

    // @verifies SWR-MG-30#b
    @Test
    fun cloudKitUploadsAndDownloadsAssetsOnlyOverHttps() {
        fun store(t: HttpTransport) = CloudKitObjectStore(t, "iCloud.test.app", CloudKitCredentials { "api" to "web" }, 30)
        val up = Altered(FakeCloudKit(), "https://up.test", "http://up.test")
        refused(up, "http://up.test") { store(up).put("np-bundle", byteArrayOf(1, 2, 3)) }
        refused(up, "http://up.test") { store(up).createIfAbsent("np-claim", byteArrayOf(1)) }

        val fake = FakeCloudKit()
        runBlocking { store(fake).put("np-bundle", byteArrayOf(1, 2, 3)) }
        val down = Altered(fake, "https://dl.test", "http://dl.test")
        refused(down, "http://dl.test") { store(down).get("np-bundle") }
    }

    // @verifies SWR-MG-30#e
    @Test
    fun noFailureOfAStoreThatCarriesItsCredentialInTheAddressQuotesTheAddress() {
        val failing = object : HttpTransport {
            override suspend fun post(request: HttpRequest): HttpResponse? = HttpResponse(500, emptyMap(), """{"serverErrorCode":"INTERNAL_ERROR"}""".encodeToByteArray())
        }
        val ck = CloudKitObjectStore(failing, "iCloud.test.app", CloudKitCredentials { "np-api-secret" to "np-web-secret" }, 30)
        val az = AzureBlobObjectStore(failing, "acct", "ctr", token = null, timeoutSeconds = 30, sas = AzureSasProvider { "sp=rcwdl&sig=np-sas-secret" })
        val calls = listOf<suspend () -> Unit>(
            { ck.get("x") }, { ck.put("x", byteArrayOf(1)) }, { ck.createIfAbsent("x", byteArrayOf(1)) }, { ck.delete("x") }, { ck.list("") },
            { az.get("x") }, { az.put("x", byteArrayOf(1)) }, { az.createIfAbsent("x", byteArrayOf(1)) }, { az.delete("x") }, { az.list("") },
        )
        for (call in calls) {
            val e = assertFailsWith<CloudStoreException> { runBlocking { call() } }
            val text = e.message.orEmpty() + e.toString()
            assertFalse("secret" in text || "://" in text || "sig=" in text || "ckAPIToken" in text, "a failure carries the status and the provider's code only: $text")
        }
    }
}
