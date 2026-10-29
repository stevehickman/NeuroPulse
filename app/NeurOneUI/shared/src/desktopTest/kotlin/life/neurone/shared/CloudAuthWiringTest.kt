package life.neurone.shared

import androidx.compose.runtime.Composable
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.test.runTest
import life.neurone.core.cloudauth.AuthorizationLauncher
import life.neurone.core.cloudauth.CloudAuthConfig
import life.neurone.core.cloudauth.CloudAuthException
import life.neurone.core.cloudauth.CloudAuthProblem
import life.neurone.core.cloudauth.CloudService
import life.neurone.core.cloudauth.OAuthClientConfig
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The app builds one cloud sign-in from what the host gives it, and keeps what it returns where everything else sealed is kept (SWR-MG-19, SWR-MG-16). */
class CloudAuthWiringTest {
    private val redirect = "http://127.0.0.1:53682"

    /** A token endpoint that answers every request with the same tokens, and a browser that approves whatever it is shown. */
    private class Fakes {
        val posts = mutableListOf<HttpRequest>()
        val transport = object : HttpTransport {
            override suspend fun post(request: HttpRequest): HttpResponse? {
                posts += request
                return HttpResponse(200, emptyMap(), """{"access_token":"access-secret-token","refresh_token":"refresh-secret-token","expires_in":3600}""".encodeToByteArray())
            }
        }
        val launcher = object : AuthorizationLauncher {
            override suspend fun authorize(authorizationUrl: String, redirectUri: String): String {
                val state = Regex("[?&]state=([^&]+)").find(authorizationUrl)!!.groupValues[1]
                return "$redirectUri/?code=c&state=$state"
            }
        }
    }

    private fun services(
        kv: life.neurone.core.common.KeyValueStore = InMemoryKeyValueStore(),
        secrets: life.neurone.core.common.SecretKeyStore? = null,
        http: HttpTransport? = null,
        launcher: AuthorizationLauncher? = null,
        config: CloudAuthConfig = CloudAuthConfig(),
        redirectPlatform: life.neurone.core.cloudauth.RedirectPlatform? = null,
    ) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val secretKeyStore = secrets
            override val httpTransport = http
            override val authorizationLauncher = launcher
            override val redirectPlatform = redirectPlatform
            @Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
        cloudAuthConfig = config,
    )

    // @verifies SWR-MG-19#k
    @Test fun a_host_with_a_transport_and_a_launcher_has_a_cloud_sign_in_and_one_missing_either_has_none() {
        val fakes = Fakes()
        assertNotNull(services(http = fakes.transport, launcher = fakes.launcher).cloudAuth)
        assertNull(services(http = fakes.transport, launcher = null).cloudAuth, "no way to show the page")
        assertNull(services(http = null, launcher = fakes.launcher).cloudAuth, "no way to reach the provider (the browser)")
        assertNull(services().cloudAuth)
    }

    // @verifies SWR-MG-19#d
    @Test fun the_build_with_no_registered_client_fails_a_sign_in_naming_the_provider_and_sends_nothing() = runTest {
        val fakes = Fakes()
        val auth = services(http = fakes.transport, launcher = fakes.launcher).cloudAuth!!
        val e = assertFailsWith<CloudAuthException> { auth.signIn(CloudService.GOOGLE_DRIVE) }
        assertEquals(CloudAuthProblem.NOT_CONFIGURED, e.problem)
        assertTrue(fakes.posts.isEmpty())
        assertTrue(auth.accounts().isEmpty())
    }

    // @verifies SWR-MG-19#k
    @Test fun a_registered_provider_signs_in_and_the_credentials_are_sealed_in_the_apps_store_not_the_hosts_raw_one() = runTest {
        val fakes = Fakes()
        val raw = InMemoryKeyValueStore()
        val auth = services(raw, InMemorySecretKeyStore(), fakes.transport, fakes.launcher, CloudAuthConfig(google = OAuthClientConfig("client", redirect))).cloudAuth!!
        val account = auth.signIn(CloudService.GOOGLE_DRIVE)
        assertEquals(listOf(account.id), auth.accounts().map { it.id })
        val onDisk = raw.getString("np.cloud.accounts")
        assertNotNull(onDisk, "the credentials were stored")
        assertFalse("refresh-secret-token" in onDisk || "access-secret-token" in onDisk, "no token is readable in the host's raw store")
    }

    // @verifies SWR-MG-25#c
    @Test fun the_hosts_platform_reaches_the_sign_in_so_an_unreceivable_redirect_reads_not_set_up() {
        val fakes = Fakes()
        val config = CloudAuthConfig(google = OAuthClientConfig("g", redirect)) // a loopback address: right for a computer, wrong for a phone
        val desktop = services(http = fakes.transport, launcher = fakes.launcher, config = config, redirectPlatform = life.neurone.core.cloudauth.RedirectPlatform.DESKTOP)
        assertTrue(desktop.cloudAuth!!.isConfigured(life.neurone.core.cloudauth.CloudService.GOOGLE_DRIVE))
        val android = services(http = fakes.transport, launcher = fakes.launcher, config = config, redirectPlatform = life.neurone.core.cloudauth.RedirectPlatform.ANDROID)
        assertTrue(!android.cloudAuth!!.isConfigured(life.neurone.core.cloudauth.CloudService.GOOGLE_DRIVE))
        val unchecked = services(http = fakes.transport, launcher = fakes.launcher, config = config)
        assertTrue(unchecked.cloudAuth!!.isConfigured(life.neurone.core.cloudauth.CloudService.GOOGLE_DRIVE), "a host that names no platform checks nothing")
    }
}
