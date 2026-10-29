package life.neurone.core.cloudauth

import kotlinx.coroutines.runBlocking
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.platform.Digests
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

class RedirectRulesTests {
    private val google = AccountFamily.GOOGLE
    private val microsoft = AccountFamily.MICROSOFT
    private val dropbox = AccountFamily.DROPBOX

    // @verifies SWR-MG-25#a
    @Test
    fun aComputerReceivesOnlyALoopbackAddressWithAPort() {
        val d = RedirectPlatform.DESKTOP
        for (family in listOf(google, microsoft, dropbox)) {
            assertNull(RedirectRules.problem(d, family, "http://127.0.0.1:53682"))
            assertNull(RedirectRules.problem(d, family, "http://localhost:9004/cb"))
            for (bad in listOf("https://127.0.0.1:53682", "http://127.0.0.1", "http://example.com:80/cb", "com.example.app:/cb", "http://[::1]:9004")) {
                assertEquals(RedirectProblem.NEEDS_LOOPBACK, RedirectRules.problem(d, family, bad), "$family $bad")
            }
        }
    }

    // @verifies SWR-MG-25#a
    @Test
    fun iosWatchesACustomSchemeAndRefusesHttpAndHttps() {
        val ios = RedirectPlatform.IOS
        for (family in listOf(google, microsoft, dropbox)) {
            assertNull(RedirectRules.problem(ios, family, "com.googleusercontent.apps.123-abc:/oauth2redirect"))
            assertNull(RedirectRules.problem(ios, family, "msauth.life.neurone.app://auth"))
            assertEquals(RedirectProblem.NEEDS_CUSTOM_SCHEME, RedirectRules.problem(ios, family, "https://example.com/cb"))
            assertEquals(RedirectProblem.NEEDS_CUSTOM_SCHEME, RedirectRules.problem(ios, family, "http://127.0.0.1:53682"))
        }
        // The same decision the launcher makes when it is asked to open the page.
        assertFailsWith<CloudAuthException> { WebSessionRules.callbackScheme("https://example.com/cb") }
    }

    // @verifies SWR-MG-25#a
    @Test
    fun googleOnAndroidNeedsAnHttpsAppLinkWhileOthersMayUseACustomSchemeOrHttps() {
        val a = RedirectPlatform.ANDROID
        assertNull(RedirectRules.problem(a, google, "https://auth.example.com/oauth2redirect"))
        assertEquals(RedirectProblem.NEEDS_HTTPS_APP_LINK, RedirectRules.problem(a, google, "com.example.app:/oauth2redirect"))
        assertEquals(RedirectProblem.NEEDS_HTTPS_APP_LINK, RedirectRules.problem(a, google, "com.googleusercontent.apps.123:/oauth2redirect"))
        for (family in listOf(microsoft, dropbox)) {
            assertNull(RedirectRules.problem(a, family, "life.neurone.app.oauth:/cb"))
            assertNull(RedirectRules.problem(a, family, "https://auth.example.com/cb"))
        }
        for (family in listOf(google, microsoft, dropbox)) {
            assertEquals(RedirectProblem.NEEDS_NON_HTTP, RedirectRules.problem(a, family, "http://127.0.0.1:53682"), "$family")
        }
    }

    // @verifies SWR-MG-25#a
    @Test
    fun anAddressWithNoSchemeIsMalformedOnEveryPlatform() {
        for (platform in RedirectPlatform.entries) {
            for (bad in listOf("", "no-colon", ":/x", "bad scheme:/x", "bad_scheme:/x")) {
                assertEquals(RedirectProblem.MALFORMED, RedirectRules.problem(platform, google, bad), "$platform '$bad'")
            }
        }
    }

    private fun auth(platform: RedirectPlatform?, config: CloudAuthConfig): Pair<CloudAuth, FakeBrowser> {
        val providers = FakeProviders()
        val browser = FakeBrowser(providers)
        return CloudAuth(providers, CredentialStore(InMemoryKeyValueStore()), config, browser, CountingRandom(), Digests::sha256, { 1_000_000L }, redirectPlatform = platform) to browser
    }

    // @verifies SWR-MG-25#b
    @Test
    fun aRedirectThePlatformCannotReceiveReadsNotSetUpAndOpensNoBrowser() = runBlocking {
        val config = CloudAuthConfig(
            google = OAuthClientConfig("g", "com.example.app:/oauth2redirect"),
            microsoft = MicrosoftClientConfig("m", "msal-test://auth"),
            dropbox = DropboxClientConfig("db", "http://127.0.0.1:9005"),
        )
        val (a, browser) = auth(RedirectPlatform.ANDROID, config)
        assertFalse(a.isConfigured(CloudService.GOOGLE_DRIVE), "Google on Android with a custom scheme")
        assertFalse(a.isConfigured(CloudService.GOOGLE_CLOUD_STORAGE))
        assertTrue(a.isConfigured(CloudService.ONEDRIVE), "a custom scheme is fine for Microsoft on Android")
        assertFalse(a.isConfigured(CloudService.DROPBOX), "plain http on a phone")
        val e = assertFailsWith<CloudAuthException> { a.signIn(CloudService.GOOGLE_DRIVE) }
        assertEquals(CloudAuthProblem.NOT_CONFIGURED, e.problem)
        assertTrue("com.example.app" !in e.message.orEmpty(), "the message names the problem and never the address")
        assertEquals(CloudAuthProblem.NOT_CONFIGURED, assertFailsWith<CloudAuthException> { a.signIn(CloudService.DROPBOX) }.problem)
        assertTrue(browser.urls.isEmpty(), "no browser was opened for either")
    }

    // @verifies SWR-MG-25#b
    @Test
    fun aRedirectThatSuitsThePlatformSignsInAndAHostWithNoPlatformChecksNothing() = runBlocking {
        val (android, _) = auth(RedirectPlatform.ANDROID, CloudAuthConfig(google = OAuthClientConfig("g", "https://auth.example.com/oauth2redirect")))
        assertTrue(android.isConfigured(CloudService.GOOGLE_DRIVE))
        assertEquals(CloudService.GOOGLE_DRIVE, android.signIn(CloudService.GOOGLE_DRIVE).service)

        val (unchecked, _) = auth(null, CloudAuthConfig(google = OAuthClientConfig("g", "com.example.app:/oauth2redirect")))
        assertTrue(unchecked.isConfigured(CloudService.GOOGLE_DRIVE), "no platform, no check: the existing behaviour")
        Unit
    }
}
