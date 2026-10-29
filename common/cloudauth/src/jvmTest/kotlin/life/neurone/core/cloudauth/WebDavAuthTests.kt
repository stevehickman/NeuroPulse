package life.neurone.core.cloudauth

import kotlinx.coroutines.runBlocking
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

class WebDavAuthTests {
    // @verifies SWR-MG-24#a
    @Test
    fun anAddressIsAnHttpsFolderAndNothingThatCouldSendARequestElsewhere() {
        assertEquals("https://cloud.example.com/remote.php/dav/files/alice/", WebDavAddress.parse(" HTTPS://Cloud.Example.com/remote.php/dav/files/alice "))
        assertEquals("https://nas/dav/", WebDavAddress.parse("https://nas/dav/"))
        assertEquals("https://192.168.1.5:5006/", WebDavAddress.parse("https://192.168.1.5:5006"))
        assertEquals("https://h.example/files/a%20b/", WebDavAddress.parse("https://h.example/files/a%20b/"))
        for (bad in listOf(
            "", "cloud.example.com/dav", "http://cloud.example.com/dav/", "ftp://x.example/", "https://", "https:///dav",
            "https://user@cloud.example.com/dav/", "https://user:pw@cloud.example.com/", "https://cloud.example.com@evil.example/", "https://cloud.example.com/dav?x=1",
            "https://cloud.example.com/dav#f", "https://[::1]/dav/", "https://cloud example.com/", "https://cloud.example.com:/dav/", "https://cloud.example.com:99999/",
            "https://cloud.example.com:80a/", "https://cloud.example.com/../x/", "https://cloud.example.com/a/./b/", "https://cloud.example.com/%2e%2e/x/",
            "https://cloud.example.com/a%2Fb/", "https://cloud.example.com/a%zz/", "https://cloud.example.com/a\\b/", "https://-bad.example/", "https://bad..example/",
        )) {
            assertNull(WebDavAddress.parse(bad), "'$bad'")
        }
    }

    // @verifies SWR-MG-24#b
    @Test
    fun theBasicHeaderIsRfc7617sOwnExampleAndUtf8() {
        assertEquals("Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==", WebDavAuthTestSupport.basic("Aladdin", "open sesame"))
        assertEquals("Basic dGVzdDpwYXNzd8O2cmQ=", WebDavAuthTestSupport.basic("test", "passwörd"))
    }

    // @verifies SWR-MG-24#b
    @Test
    fun aLoginIsKeptSealedChecksItsInputsAndIsReadAtEveryCall() = runBlocking {
        val rig = Rig()
        assertTrue(rig.auth.isConfigured(CloudService.WEBDAV))
        val account = rig.auth.addWebDavLogin("https://cloud.example.com/dav", "alice", "p:ss word")
        assertEquals(CloudService.WEBDAV, account.service)
        val login = rig.auth.webDavLogin(account.id).get()
        assertEquals("https://cloud.example.com/dav/", login.baseUrl)
        assertEquals(WebDavAuthTestSupport.basic("alice", "p:ss word"), login.authorization, "a colon and a space are fine in the password")

        // The same account signed in again keeps its id and the new login is used by a provider made before.
        val provider = rig.auth.webDavLogin(account.id)
        val again = rig.auth.addWebDavLogin("https://cloud.example.com/dav", "alice", "new", replacing = account.id)
        assertEquals(account.id, again.id)
        assertEquals(WebDavAuthTestSupport.basic("alice", "new"), provider.get().authorization)

        for ((url, user, pw) in listOf(
            Triple("http://cloud.example.com/dav", "alice", "x"), Triple("https://cloud.example.com/dav", "", "x"), Triple("https://cloud.example.com/dav", "a:b", "x"),
            Triple("https://cloud.example.com/dav", "al\nice", "x"), Triple("https://cloud.example.com/dav", "alice", ""), Triple("https://cloud.example.com/dav", "alice", "p\u0000w"),
        )) {
            assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { rig.auth.addWebDavLogin(url, user, pw) }.problem, "$url $user")
        }
        assertEquals(1, rig.auth.accounts().size, "a refused login kept nothing")
    }

    // @verifies SWR-MG-24#b
    @Test
    fun aFlaggedLoginAsksForTheLoginAgainNoBrowserSignInExistsAndSignOutHasNothingToRevoke() = runBlocking {
        val rig = Rig()
        val account = rig.auth.addWebDavLogin("https://cloud.example.com/dav", "alice", "pw")
        assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.WEBDAV) }.problem)
        assertTrue(rig.browser.urls.isEmpty())
        rig.auth.markNeedsSignIn(account.id)
        val e = assertFailsWith<CloudAuthException> { rig.auth.webDavLogin(account.id).get() }
        assertEquals(CloudAuthProblem.SIGN_IN_REQUIRED, e.problem)
        assertTrue(CloudAuth.needsSignIn(e))
        assertEquals(Revocation.NOT_AVAILABLE, rig.auth.signOut(account.id))
        assertTrue(rig.auth.accounts().isEmpty())
    }

    // @verifies SWR-MG-24#c
    @Test
    fun aWebDavBackupPlaceGoesToTheNamedFolderWithTheLoginAndIsABackupOnlyRoute() = runBlocking {
        val rig = Rig()
        val account = rig.auth.addWebDavLogin("https://cloud.example.com/dav/", "alice", "pw")
        val seen = mutableListOf<HttpRequest>()
        val api = object : HttpTransport {
            override suspend fun post(request: HttpRequest): HttpResponse? {
                seen += request
                return HttpResponse(404, emptyMap(), ByteArray(0))
            }
        }
        val store = CloudStores.storeFor(rig.auth, api, account)
        assertEquals(null, store.get("np-migration-bundle.npbk"))
        assertEquals("https://cloud.example.com/dav/neurone-backup/np-migration-bundle.npbk", seen.single().url)
        assertEquals("GET", seen.single().method)
        assertEquals(WebDavAuthTestSupport.basic("alice", "pw"), seen.single().headers["Authorization"])
        assertEquals(false, CloudStores.routeFor(CloudService.WEBDAV).atomicClaim)
        assertEquals("cloud-webdav", CloudStores.routeFor(CloudService.WEBDAV).id)
        assertTrue(CloudStores.targetFields(CloudService.WEBDAV).isEmpty())
    }
}

/** The public face of the header builder under the name the tests use. */
private object WebDavAuthTestSupport {
    fun basic(user: String, password: String) = WebDavAddress.basic(user, password)
}
