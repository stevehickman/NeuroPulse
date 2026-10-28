package life.neurone.core.cloudauth

import kotlinx.coroutines.runBlocking
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** A service's API: answers every call with an empty listing, and keeps what it was sent. */
private class RecordingApi : HttpTransport {
    val seen = mutableListOf<HttpRequest>()
    override suspend fun post(request: HttpRequest): HttpResponse? {
        seen += request
        val body = when {
            "googleapis.com" in request.url -> """{"files":[]}"""
            "graph.microsoft.com" in request.url -> """{"value":[]}"""
            else -> """{"records":[]}"""
        }
        return HttpResponse(200, emptyMap(), body.encodeToByteArray())
    }
}

class CloudStoresTests {
    // @verifies SWR-MG-21#c
    @Test
    fun everyCloudRouteDeclaresNoExclusiveCreateSoTheCoreCarriesABackupAndNeverAMigration() {
        for (s in listOf(CloudService.GOOGLE_DRIVE, CloudService.ONEDRIVE, CloudService.ICLOUD_CLOUDKIT)) {
            assertTrue(CloudStores.canKeepBackups(s))
            val route = CloudStores.routeFor(s)
            assertFalse(route.atomicClaim, "$s")
            assertFalse(route.direct)
        }
        assertEquals("cloud-google-drive", CloudStores.routeFor(CloudService.GOOGLE_DRIVE).id)
        assertEquals("cloud-icloud-cloudkit", CloudStores.routeFor(CloudService.ICLOUD_CLOUDKIT).id)
    }

    // @verifies SWR-MG-21#a
    @Test
    fun onlyTheServicesThatNeedNothingBeyondTheSignInCanKeepABackup() = runBlocking {
        for (s in listOf(CloudService.GOOGLE_CLOUD_STORAGE, CloudService.AZURE_BLOB, CloudService.AWS_S3)) {
            assertFalse(CloudStores.canKeepBackups(s), "$s needs a bucket or container")
            assertFailsWith<IllegalArgumentException> { CloudStores.routeFor(s) }
        }
        val rig = Rig()
        val account = rig.auth.addAwsKeys("AKIAEXAMPLE", "secret")
        assertFailsWith<IllegalArgumentException> { CloudStores.storeFor(rig.auth, RecordingApi(), account) }
    }

    // @verifies SWR-MG-21#b
    @Test
    fun aGoogleDriveAndAOneDriveStoreSendTheAccountsOwnTokenToTheServiceAndGetAFreshOneWhenItHasExpired() = runBlocking {
        for ((service, host) in listOf(CloudService.GOOGLE_DRIVE to "googleapis.com", CloudService.ONEDRIVE to "graph.microsoft.com")) {
            val rig = Rig()
            val account = rig.auth.signIn(service)
            val api = RecordingApi()
            val store = CloudStores.storeFor(rig.auth, api, account)

            store.list("")
            assertTrue(host in api.seen.last().url, "$service goes to its own service")
            assertEquals("Bearer access-1", api.seen.last().headers["Authorization"], "$service sends the sign-in's token")

            rig.clock += 3_600 // past the token's life
            store.list("")
            assertEquals("Bearer access-2", api.seen.last().headers["Authorization"], "$service: the same store, a fresh token")
            assertEquals(2, rig.providers.seen.count { it.url.endsWith("/token") && rig.providers.form(it)["grant_type"] == "refresh_token" } + 1, "one refresh, no more")
        }
    }

    // @verifies SWR-MG-21#b
    @Test
    fun aStoreWhoseSignInWasRefusedFailsWithTheSignInStatusAndNothingIsSentToTheService() = runBlocking {
        val rig = Rig()
        val account = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        val api = RecordingApi()
        val store = CloudStores.storeFor(rig.auth, api, account)
        rig.clock += 3_600
        rig.providers.refreshError = "invalid_grant"
        val e = assertFailsWith<life.neurone.core.cloud.CloudStoreException> { store.list("") }
        assertEquals(401, e.status)
        assertTrue(CloudAuth.needsSignIn(e))
        assertTrue(api.seen.isEmpty(), "no request went out without a token")
    }

    // @verifies SWR-MG-21#a
    @Test
    fun anICloudStoreNeedsTheBuildsContainerAndUsesTheAccountsWebToken() = runBlocking {
        val rig = Rig()
        rig.browser.callback = { redirect, _ -> "$redirect?ckWebAuthToken=web-token-1" }
        val account = rig.auth.signIn(CloudService.ICLOUD_CLOUDKIT)
        val api = RecordingApi()
        CloudStores.storeFor(rig.auth, api, account).list("")
        val url = api.seen.last().url
        assertTrue("api.apple-cloudkit.com" in url && "iCloud.test.app" in url && "ckAPIToken=ck-api-token" in url && "ckWebAuthToken=web-token-1" in url, url)

        // The same credentials under a build with no container: the store is refused, naming what is missing.
        val unregistered = CloudAuth(rig.providers, CredentialStore(rig.kv), CloudAuthConfig(), rig.browser)
        val e = assertFailsWith<CloudAuthException> { CloudStores.storeFor(unregistered, api, account) }
        assertEquals(CloudAuthProblem.NOT_CONFIGURED, e.problem)
    }
}
