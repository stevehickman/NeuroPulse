package life.neurone.core.cloud

import kotlinx.coroutines.runBlocking
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** A provider stand-in whose replies are altered on the way back, and which records every request that was sent on. */
private class Altered(private val inner: HttpTransport, private val alter: (String) -> String) : HttpTransport {
    val sent = mutableListOf<HttpRequest>()

    override suspend fun post(request: HttpRequest): HttpResponse? {
        sent += request
        val r = inner.post(request) ?: return null
        return HttpResponse(r.status, r.headers.mapValues { alter(it.value) }, alter(r.body.decodeToString()).encodeToByteArray())
    }
}

/**
 * An address a provider's reply supplies is checked before anything is sent to it (`SWR-MG-28` (a), `OI-PRIV-15` point 2). The stand-ins are
 * the adapters' own, with one address in a reply swapped; a pass shows what the adapter does with such a reply, **not that a provider sends one**.
 */
class FollowUpAddressTests {
    private val notHttps = listOf("http://up.test/x", "HTTP://up.test/x", "ftp://up.test/x", "//up.test/x", "/x", "up.test/x", "https://", "https:///x", "https://user@up.test/x", "")

    /** [call] fails with a message that does not quote [address], and nothing was sent to it. */
    private suspend fun refused(t: Altered, address: String, call: suspend () -> Unit) {
        val e = assertFailsWith<CloudStoreException>(address) { call() }
        assertTrue(address.isEmpty() || address !in e.message!!, "the message quotes the address: ${e.message}")
        assertTrue(t.sent.none { address.isNotEmpty() && it.url.startsWith(address) }, "a request went to $address")
    }

    // @verifies SWR-MG-28#a
    @Test
    fun theCheckTakesHttpsWithAHostAndHoldsATokenBearingLinkToTheProvidersOwnAddress() {
        assertEquals("https://sn3302.up.1drv.com/up/abc", followUp("https://sn3302.up.1drv.com/up/abc", "upload"))
        for (bad in notHttps) assertFailsWith<CloudStoreException>(bad) { followUp(bad, "upload") }

        val base = "https://graph.microsoft.com/v1.0"
        for (ok in listOf(base, "$base/me/drive/special/approot/children?\$skiptoken=abc", "$base?x=1")) assertEquals(ok, followUp(ok, "list", under = base))
        for (bad in listOf(
            "https://evil.example/v1.0/me/drive", "https://graph.microsoft.com.evil.example/v1.0/me", "https://graph.microsoft.com/v1.0@evil.example/me",
            "https://graph.microsoft.com/v1.0evil/me", "https://graph.microsoft.com/beta/me", "https://graph.microsoft.com@evil.example/v1.0/me",
            "http://graph.microsoft.com/v1.0/me",
        )) {
            val e = assertFailsWith<CloudStoreException>(bad) { followUp(bad, "list", under = base) }
            assertTrue(bad !in e.message!!)
        }
    }

    // @verifies SWR-MG-28#a
    @Test
    fun oneDriveSendsItsTokenToANextPageOnlyUnderItsOwnAddress() = runBlocking {
        for (next in listOf(
            "https://evil.test/v1.0/me/drive/special/approot/children?skip=2", "https://graph.test.evil.test/v1.0/me/drive/special/approot/children?skip=2",
            "http://graph.test/v1.0/me/drive/special/approot/children?skip=2", "https://graph.test/beta/children?skip=2",
        )) {
            val fake = FakeOneDrive()
            val t = Altered(fake) { it.replace("https://graph.test/v1.0/me/drive/special/approot/children?skip=2", next) }
            val s = OneDriveObjectStore(t, AccessToken { "tok" }, timeoutSeconds = 30, baseUrl = "https://graph.test/v1.0", chunkBytes = 327_680)
            listOf("np-1", "np-2", "np-3").forEach { s.put(it, byteArrayOf(1)) }
            t.sent.clear()
            refused(t, next) { s.list("np-") }
            assertEquals(1, t.sent.size, "the first page only; the token went nowhere else")
        }
        // The stand-in's own next link is under the base address and is followed.
        val fake = FakeOneDrive()
        val s = OneDriveObjectStore(fake, AccessToken { "tok" }, timeoutSeconds = 30, baseUrl = "https://graph.test/v1.0", chunkBytes = 327_680)
        listOf("np-1", "np-2", "np-3").forEach { s.put(it, byteArrayOf(1)) }
        assertEquals(listOf("np-1", "np-2", "np-3"), s.list("np-"))
    }

    // @verifies SWR-MG-28#a
    @Test
    fun oneDriveSendsBytesAndFetchesADownloadOnlyOverHttps() = runBlocking {
        for (bad in listOf("http://up.test", "ftp://up.test", "//up.test")) {
            val t = Altered(FakeOneDrive()) { it.replace("https://up.test", bad) }
            val s = OneDriveObjectStore(t, AccessToken { "tok" }, timeoutSeconds = 30, baseUrl = "https://graph.test/v1.0", chunkBytes = 327_680)
            refused(t, "$bad/upload/0") { s.put("np-x", byteArrayOf(1, 2, 3)) }
            assertTrue(t.sent.none { it.method == "PUT" }, "no bytes left for $bad")
        }
        for (bad in listOf("http://dl.test", "file://dl.test")) {
            val fake = FakeOneDrive()
            val t = Altered(fake) { it.replace("https://dl.test", bad) }
            val s = OneDriveObjectStore(t, AccessToken { "tok" }, timeoutSeconds = 30, baseUrl = "https://graph.test/v1.0", chunkBytes = 327_680)
            s.put("np-x", byteArrayOf(1, 2, 3))
            t.sent.clear()
            refused(t, "$bad/dl/np-x") { s.get("np-x") }
            assertEquals(1, t.sent.size, "the request that was redirected, and no second one")
        }
    }

    // @verifies SWR-MG-28#a
    @Test
    fun googleDriveSendsBytesToAnUploadSessionOnlyOverHttps() = runBlocking {
        for (bad in listOf("http://drive.test", "ftp://drive.test", "//drive.test")) {
            val t = Altered(FakeDrive()) { it.replace("https://drive.test", bad) }
            val s = GoogleDriveObjectStore(t, AccessToken { "tok" }, timeoutSeconds = 30, baseUrl = "https://drive.test", chunkBytes = 262_144)
            refused(t, "$bad/session/0") { s.put("np-x", byteArrayOf(1, 2, 3)) }
            assertTrue(t.sent.none { it.method == "PUT" }, "no bytes left for $bad")
        }
    }

    // @verifies SWR-MG-28#a
    @Test
    fun cloudKitSendsAnAssetAndFetchesOneOnlyOverHttps() = runBlocking {
        fun store(t: HttpTransport) = CloudKitObjectStore(t, "iCloud.test.app", CloudKitCredentials { "api" to "web" }, timeoutSeconds = 30)
        for (bad in listOf("http://up.test", "ftp://up.test")) {
            val t = Altered(FakeCloudKit()) { it.replace("https://up.test", bad) }
            refused(t, "$bad/u/0") { store(t).put("np-x", byteArrayOf(1, 2, 3)) }
            assertEquals(1, t.sent.size, "the request for an upload address, and no upload")
        }
        val fake = FakeCloudKit()
        store(fake).put("np-x", byteArrayOf(1, 2, 3))
        val t = Altered(fake) { it.replace("https://dl.test", "http://dl.test") }
        val e = assertFailsWith<CloudStoreException> { store(t).get("np-x") }
        assertTrue("dl.test" !in e.message!!)
        assertEquals(1, t.sent.size, "the lookup, and no download")
    }
}
