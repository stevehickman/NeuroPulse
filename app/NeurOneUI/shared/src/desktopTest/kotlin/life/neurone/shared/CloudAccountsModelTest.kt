package life.neurone.shared

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.async
import kotlinx.coroutines.test.runTest
import life.neurone.core.cloudauth.AuthorizationLauncher
import life.neurone.core.cloudauth.CloudAuth
import life.neurone.core.cloudauth.CloudAuthConfig
import life.neurone.core.cloudauth.CloudAuthException
import life.neurone.core.cloudauth.CloudAuthProblem
import life.neurone.core.cloudauth.CloudService
import life.neurone.core.cloudauth.CredentialStore
import life.neurone.core.cloudauth.MicrosoftClientConfig
import life.neurone.core.cloudauth.OAuthClientConfig
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import life.neurone.shared.cloud.CloudAccountsModel
import life.neurone.shared.cloud.CloudNoticeKind
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The cloud accounts screen's state and actions over a real [CloudAuth] with a stand-in provider and browser (SWR-MG-20). */
class CloudAccountsModelTest {
    private val google = OAuthClientConfig("g-client", "http://127.0.0.1:53682")
    private val microsoft = MicrosoftClientConfig("m-client", "app.test.oauth:/redirect")

    private class Fakes {
        val posts = mutableListOf<String>()
        var tokenAnswer: HttpResponse? = HttpResponse(200, emptyMap(), """{"access_token":"access-token-1","refresh_token":"refresh-token-1","expires_in":3600}""".encodeToByteArray())
        val transport = object : HttpTransport {
            override suspend fun post(request: HttpRequest): HttpResponse? {
                posts += request.url
                return if ("revoke" in request.url) HttpResponse(200, emptyMap(), ByteArray(0)) else tokenAnswer
            }
        }
        var launch: suspend (String, String) -> String = { url, redirect ->
            "$redirect?code=c&state=${Regex("[?&]state=([^&]+)").find(url)!!.groupValues[1]}"
        }
        var shown = 0
        val launcher = object : AuthorizationLauncher {
            override suspend fun authorize(authorizationUrl: String, redirectUri: String): String {
                shown++
                return launch(authorizationUrl, redirectUri)
            }
        }
    }

    private fun model(fakes: Fakes, kv: InMemoryKeyValueStore = InMemoryKeyValueStore(), config: CloudAuthConfig = CloudAuthConfig(google = google, microsoft = microsoft)) =
        CloudAccountsModel(CloudAuth(fakes.transport, CredentialStore(kv), config, fakes.launcher))

    // @verifies SWR-MG-20#a
    @Test fun every_service_is_listed_and_only_those_the_build_is_registered_for_can_sign_in() = runTest {
        val m = model(Fakes())
        m.refresh()
        assertEquals(CloudService.values().toList(), m.rows.value.map { it.service })
        val configured = m.rows.value.filter { it.configured }.map { it.service }.toSet()
        assertEquals(setOf(CloudService.GOOGLE_DRIVE, CloudService.GOOGLE_CLOUD_STORAGE, CloudService.ONEDRIVE, CloudService.AZURE_BLOB, CloudService.AWS_S3, CloudService.WEBDAV), configured)
        assertTrue(m.rows.value.none { it.account != null })
    }

    // @verifies SWR-MG-20#a
    @Test fun signing_in_to_an_unregistered_provider_says_so_and_sends_nothing() = runTest {
        val f = Fakes()
        val m = model(f, config = CloudAuthConfig())
        m.refresh()
        m.signIn(CloudService.GOOGLE_DRIVE)
        assertEquals(CloudNoticeKind.NOT_CONFIGURED, m.notice.value?.kind)
        assertEquals(CloudService.GOOGLE_DRIVE, m.notice.value?.service)
        assertEquals(0, f.shown)
        assertTrue(f.posts.isEmpty())
    }

    // @verifies SWR-MG-20#b
    @Test fun a_sign_in_makes_one_account_and_signing_in_again_to_the_same_service_is_not_offered() = runTest {
        val f = Fakes()
        val m = model(f)
        m.refresh()
        m.signIn(CloudService.GOOGLE_DRIVE)
        assertEquals(CloudNoticeKind.SIGNED_IN, m.notice.value?.kind)
        val row = m.rows.value.first { it.service == CloudService.GOOGLE_DRIVE }
        assertTrue(row.signedIn)
        assertNull(m.busy.value, "the action ended")
        val before = f.shown
        m.signIn(CloudService.GOOGLE_DRIVE)
        assertEquals(before, f.shown, "already signed in: no second browser")
        assertEquals(1, m.rows.value.count { it.account != null })
    }

    // @verifies SWR-MG-20#b
    @Test fun closing_the_browser_says_nothing_and_every_other_failure_is_a_kind_without_the_providers_words() = runTest {
        val f = Fakes()
        val m = model(f)
        m.refresh()
        f.launch = { _, _ -> throw CloudAuthException(CloudAuthProblem.CANCELLED, "closed") }
        m.signIn(CloudService.GOOGLE_DRIVE)
        assertNull(m.notice.value, "a cancel is not an error")
        assertNull(m.rows.value.first { it.service == CloudService.GOOGLE_DRIVE }.account)

        f.launch = { _, redirect -> "$redirect?error=access_denied&state=x" }
        m.signIn(CloudService.GOOGLE_DRIVE)
        assertEquals(CloudNoticeKind.DENIED, m.notice.value?.kind)

        f.launch = { _, redirect -> "$redirect?code=c&state=not-the-state" }
        m.signIn(CloudService.GOOGLE_DRIVE)
        assertEquals(CloudNoticeKind.FAILED, m.notice.value?.kind)

        f.launch = { url, redirect -> "$redirect?code=c&state=${Regex("[?&]state=([^&]+)").find(url)!!.groupValues[1]}" }
        f.tokenAnswer = null
        m.signIn(CloudService.GOOGLE_DRIVE)
        assertEquals(CloudNoticeKind.NETWORK, m.notice.value?.kind)
        assertTrue(m.rows.value.none { it.account != null }, "nothing was saved by any failure")
    }

    // @verifies SWR-MG-20#b
    @Test fun a_sign_in_the_provider_refused_is_signed_in_again_keeping_the_account() = runTest {
        val f = Fakes()
        val kv = InMemoryKeyValueStore()
        val auth = CloudAuth(f.transport, CredentialStore(kv), CloudAuthConfig(google = google), f.launcher)
        val m = CloudAccountsModel(auth)
        m.refresh()
        m.signIn(CloudService.GOOGLE_DRIVE)
        val id = m.rows.value.first { it.account != null }.account!!.id
        auth.markNeedsSignIn(id)
        m.refresh()
        assertTrue(m.rows.value.first { it.account != null }.needsSignIn)
        m.signIn(CloudService.GOOGLE_DRIVE)
        val row = m.rows.value.first { it.account != null }
        assertEquals(id, row.account!!.id, "the same account")
        assertFalse(row.needsSignIn)
    }

    // @verifies SWR-MG-20#c
    @Test fun signing_out_removes_the_account_and_says_when_the_provider_could_not_be_asked_to_end_access() = runTest {
        val f = Fakes()
        val m = model(f)
        m.refresh()
        m.signIn(CloudService.GOOGLE_DRIVE)
        m.signOut(CloudService.GOOGLE_DRIVE)
        assertEquals(CloudNoticeKind.SIGNED_OUT, m.notice.value?.kind, "Google has a revoke call")
        assertTrue(f.posts.any { "revoke" in it })
        assertNull(m.rows.value.first { it.service == CloudService.GOOGLE_DRIVE }.account)

        m.signIn(CloudService.ONEDRIVE)
        assertTrue(m.rows.value.first { it.service == CloudService.ONEDRIVE }.signedIn)
        m.signOut(CloudService.ONEDRIVE)
        assertEquals(CloudNoticeKind.SIGNED_OUT_NOT_REVOKED, m.notice.value?.kind, "Microsoft has none used here")
        assertNull(m.rows.value.first { it.service == CloudService.ONEDRIVE }.account, "the local sign-in is gone either way")
    }

    // @verifies SWR-MG-20#d
    @Test fun aws_keys_are_checked_kept_once_and_signing_out_removes_them() = runTest {
        val f = Fakes()
        val m = model(f)
        m.refresh()
        m.addAwsKeys("AKIAEXAMPLE", "secret with space", null)
        assertEquals(CloudNoticeKind.KEYS_INVALID, m.notice.value?.kind)
        assertNull(m.rows.value.first { it.service == CloudService.AWS_S3 }.account)

        m.addAwsKeys(" AKIAEXAMPLE ", "wJalrXUtnFEMI", "")
        assertEquals(CloudNoticeKind.KEYS_SAVED, m.notice.value?.kind)
        assertTrue(m.rows.value.first { it.service == CloudService.AWS_S3 }.signedIn)
        m.addAwsKeys("AKIAOTHER", "another", null)
        assertEquals(1, m.rows.value.count { it.account != null }, "replacing, not adding a second")

        m.signOut(CloudService.AWS_S3)
        assertNull(m.rows.value.first { it.service == CloudService.AWS_S3 }.account)
        assertTrue(f.posts.isEmpty(), "AWS keys are never sent anywhere by this screen")
    }

    // @verifies SWR-MG-24#d
    @Test fun a_webdav_login_is_checked_kept_once_and_signing_out_removes_it_without_sending_anything() = runTest {
        val f = Fakes()
        val m = model(f)
        m.refresh()
        m.addWebDavLogin("http://nas.example/dav", "alice", "pw")
        assertEquals(CloudNoticeKind.LOGIN_INVALID, m.notice.value?.kind, "http is refused")
        m.addWebDavLogin("https://nas.example/dav", "al:ice", "pw")
        assertEquals(CloudNoticeKind.LOGIN_INVALID, m.notice.value?.kind, "a colon in the name is refused")
        assertNull(m.rows.value.first { it.service == CloudService.WEBDAV }.account)

        m.addWebDavLogin(" https://nas.example/dav ", " alice ", "p w")
        assertEquals(CloudNoticeKind.LOGIN_SAVED, m.notice.value?.kind)
        assertTrue(m.rows.value.first { it.service == CloudService.WEBDAV }.signedIn)
        m.addWebDavLogin("https://nas.example/dav", "alice", "other")
        assertEquals(1, m.rows.value.count { it.account != null }, "replacing, not adding a second")

        m.signIn(CloudService.WEBDAV) // there is no browser sign-in
        assertEquals(0, f.shown)
        m.signOut(CloudService.WEBDAV)
        assertNull(m.rows.value.first { it.service == CloudService.WEBDAV }.account)
        assertEquals(CloudNoticeKind.SIGNED_OUT_NOT_REVOKED, m.notice.value?.kind)
        assertTrue(f.posts.isEmpty(), "a WebDAV login is never sent anywhere by this screen")
    }

    // @verifies SWR-MG-20#e
    @Test fun an_unreadable_store_is_reported_and_never_overwritten() = runTest {
        val kv = InMemoryKeyValueStore()
        kv.putString(CredentialStore.KEY, "this is not json")
        val f = Fakes()
        val m = model(f, kv)
        m.refresh()
        assertEquals(CloudNoticeKind.UNREADABLE, m.notice.value?.kind)
        assertEquals(CloudService.values().size, m.rows.value.size, "the list is still drawn")
        m.signIn(CloudService.GOOGLE_DRIVE)
        assertEquals("this is not json", kv.getString(CredentialStore.KEY), "what could not be read was left alone")
    }

    // @verifies SWR-MG-20#b
    @Test fun one_action_runs_at_a_time() = runTest {
        val f = Fakes()
        val release = CompletableDeferred<Unit>()
        f.launch = { url, redirect ->
            release.await()
            "$redirect?code=c&state=${Regex("[?&]state=([^&]+)").find(url)!!.groupValues[1]}"
        }
        val m = model(f)
        m.refresh()
        val first = async { m.signIn(CloudService.GOOGLE_DRIVE) }
        while (f.shown == 0) kotlinx.coroutines.yield()
        assertEquals(CloudService.GOOGLE_DRIVE, m.busy.value)
        m.signIn(CloudService.ONEDRIVE) // ignored while one runs
        release.complete(Unit)
        first.await()
        assertEquals(1, f.shown, "the second request never reached the browser")
        assertNull(m.rows.value.first { it.service == CloudService.ONEDRIVE }.account)
        assertNull(m.busy.value)
    }
}
