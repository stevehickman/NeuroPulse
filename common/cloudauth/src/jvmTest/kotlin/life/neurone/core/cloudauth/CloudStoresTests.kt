package life.neurone.core.cloudauth

import kotlinx.coroutines.runBlocking
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** A service's API: answers every call with an empty listing, and keeps what it was sent. */
private class RecordingApi : HttpTransport {
    val seen = mutableListOf<HttpRequest>()
    override suspend fun post(request: HttpRequest): HttpResponse? {
        seen += request
        val body = when {
            "googleapis.com" in request.url -> """{"files":[]}"""
            "graph.microsoft.com" in request.url -> """{"value":[]}"""
            "dropboxapi.com" in request.url -> """{"entries":[],"has_more":false}"""
            else -> """{"records":[]}"""
        }
        return HttpResponse(200, emptyMap(), body.encodeToByteArray())
    }
}

class CloudStoresTests {
    // @verifies SWR-MG-21#c
    @Test
    fun everyCloudRouteDeclaresNoExclusiveCreateSoTheCoreCarriesABackupAndNeverAMigration() {
        for (s in CloudService.entries) {
            val route = CloudStores.routeFor(s)
            assertFalse(route.atomicClaim, "$s")
            assertFalse(route.direct)
        }
        assertEquals("cloud-google-drive", CloudStores.routeFor(CloudService.GOOGLE_DRIVE).id)
        assertEquals("cloud-icloud-cloudkit", CloudStores.routeFor(CloudService.ICLOUD_CLOUDKIT).id)
    }

    // @verifies SWR-MG-22#a
    // @verifies SWR-MG-22#e
    @Test
    fun aTargetIsOnlyAPlainNameForEveryFieldTheServiceAsksFor() {
        val s3 = CloudService.AWS_S3
        assertEquals(listOf(TargetField.BUCKET, TargetField.REGION, TargetField.ENDPOINT), CloudStores.targetFields(s3))
        assertEquals(listOf(TargetField.GCS_BUCKET), CloudStores.targetFields(CloudService.GOOGLE_CLOUD_STORAGE))
        assertEquals(listOf(TargetField.STORAGE_ACCOUNT, TargetField.CONTAINER), CloudStores.targetFields(CloudService.AZURE_BLOB))
        assertTrue(CloudStores.targetFields(CloudService.GOOGLE_DRIVE).isEmpty())

        val ok = CloudStores.target(s3, mapOf(TargetField.BUCKET to " my-backups.2026 ", TargetField.REGION to "eu-west-2"))!!
        assertEquals("my-backups.2026", ok.values[TargetField.BUCKET], "surrounding spaces are not part of the name")
        assertNull(CloudStores.target(s3, mapOf(TargetField.BUCKET to "my-backups")), "a missing field")
        for (bad in listOf("", "ab", "My-Bucket", "a/b", "bucket?x=1", "bucket@evil.example", "a b", "-lead", "trail-", "buc\nket", "x".repeat(64), "my_bucket")) {
            assertNull(CloudStores.target(s3, mapOf(TargetField.BUCKET to bad, TargetField.REGION to "us-east-1")), "bucket '$bad'")
        }
        for (bad in listOf("", "us", "US-EAST-1", "us-east-1.evil.example", "us east", "us-east-1/x")) {
            assertNull(CloudStores.target(s3, mapOf(TargetField.BUCKET to "my-backups", TargetField.REGION to bad)), "region '$bad'")
        }
        assertNull(CloudStores.target(CloudService.GOOGLE_CLOUD_STORAGE, mapOf(TargetField.GCS_BUCKET to "x".repeat(223))))
        assertTrue(CloudStores.target(CloudService.GOOGLE_CLOUD_STORAGE, mapOf(TargetField.GCS_BUCKET to "x".repeat(222))) != null)
        // The endpoint is optional; when given it is a plain host (and port), nothing that could add a scheme, a user, a path or a query.
        val base = mapOf(TargetField.BUCKET to "my-backups", TargetField.REGION to "auto")
        assertNull(CloudStores.target(s3, base)!!.values[TargetField.ENDPOINT], "empty or absent means Amazon S3")
        assertNull(CloudStores.target(s3, base + (TargetField.ENDPOINT to "  "))!!.values[TargetField.ENDPOINT])
        for (good in listOf("s3.example.com", "acct.r2.cloudflarestorage.com", "minio.local:9000", "s3.us-west-004.backblazeb2.com")) {
            assertEquals(good, CloudStores.target(s3, base + (TargetField.ENDPOINT to good))!!.values[TargetField.ENDPOINT])
        }
        for (bad in listOf("https://s3.example.com", "s3.example.com/x", "user@s3.example.com", "s3.example.com?x=1", "S3.Example.com", "localhost", "s3.example.com:", "s3.example.com:123456", "-a.example.com", "a..example.com", "s3 example.com")) {
            assertNull(CloudStores.target(s3, base + (TargetField.ENDPOINT to bad)), "endpoint '$bad'")
        }
        val azure = CloudService.AZURE_BLOB
        assertNull(CloudStores.target(azure, mapOf(TargetField.STORAGE_ACCOUNT to "my-account", TargetField.CONTAINER to "backups")), "no hyphen in an account")
        assertNull(CloudStores.target(azure, mapOf(TargetField.STORAGE_ACCOUNT to "acct.evil.example", TargetField.CONTAINER to "backups")), "no dot in an account")
        assertNull(CloudStores.target(azure, mapOf(TargetField.STORAGE_ACCOUNT to "myaccount", TargetField.CONTAINER to "a--b")), "no double hyphen in a container")
        assertTrue(CloudStores.target(azure, mapOf(TargetField.STORAGE_ACCOUNT to "myaccount", TargetField.CONTAINER to "neurone-backups")) != null)
        assertTrue(CloudStores.target(CloudService.GOOGLE_CLOUD_STORAGE, mapOf(TargetField.GCS_BUCKET to "my_bucket.2026")) != null, "GCS allows an underscore")
    }

    // @verifies SWR-MG-22#b
    // @verifies SWR-MG-22#e
    @Test
    fun aBucketStoreGoesToTheNamedBucketWithTheAccountsCredentialAndIsRefusedWithoutAValidTarget() = runBlocking {
        suspend fun sent(rig: Rig, account: CloudAccount, target: BackupTarget?): HttpRequest {
            val api = RecordingApi()
            runCatching { CloudStores.storeFor(rig.auth, api, account, target).list("") } // the stand-in's reply is not a real listing; the request is what is checked
            return api.seen.last()
        }

        val rig = Rig()
        val gcs = sent(rig, rig.auth.signIn(CloudService.GOOGLE_CLOUD_STORAGE), CloudStores.target(CloudService.GOOGLE_CLOUD_STORAGE, mapOf(TargetField.GCS_BUCKET to "my-bucket")))
        assertTrue("storage.googleapis.com" in gcs.url && "my-bucket" in gcs.url, gcs.url)
        assertEquals("Bearer access-1", gcs.headers["Authorization"])

        val azure = sent(rig, rig.auth.signIn(CloudService.AZURE_BLOB), CloudStores.target(CloudService.AZURE_BLOB, mapOf(TargetField.STORAGE_ACCOUNT to "myaccount", TargetField.CONTAINER to "backups")))
        assertTrue(azure.url.startsWith("https://myaccount.blob.core.windows.net/backups"), azure.url)
        assertTrue(azure.headers["Authorization"]!!.startsWith("Bearer "))

        val aws = rig.auth.addAwsKeys("AKIAEXAMPLE", "secret")
        val s3 = sent(rig, aws, CloudStores.target(CloudService.AWS_S3, mapOf(TargetField.BUCKET to "my-backups", TargetField.REGION to "eu-west-2")))
        assertTrue(s3.url.startsWith("https://my-backups.s3.eu-west-2.amazonaws.com/"), s3.url)
        assertTrue(s3.headers.entries.any { it.key.equals("Authorization", true) && it.value.startsWith("AWS4-HMAC-SHA256 Credential=AKIAEXAMPLE/") }, "signed with the account's keys")
        val dotted = sent(rig, aws, CloudStores.target(CloudService.AWS_S3, mapOf(TargetField.BUCKET to "my.backups", TargetField.REGION to "eu-west-2")))
        assertTrue(dotted.url.startsWith("https://s3.eu-west-2.amazonaws.com/my.backups"), "a dotted bucket is addressed by path: ${dotted.url}")

        val compatible = sent(rig, aws, CloudStores.target(CloudService.AWS_S3, mapOf(TargetField.BUCKET to "my-backups", TargetField.REGION to "auto", TargetField.ENDPOINT to "acct.r2.example.com")))
        assertTrue(compatible.url.startsWith("https://acct.r2.example.com/my-backups"), "an S3-compatible store is addressed by path at its own host: ${compatible.url}")
        assertTrue(compatible.headers.entries.any { it.key.equals("Authorization", true) && "/auto/s3/aws4_request" in it.value }, "signed for the region it names")

        for (account in listOf(aws, rig.auth.signIn(CloudService.GOOGLE_CLOUD_STORAGE))) {
            val e = assertFailsWith<CloudAuthException> { CloudStores.storeFor(rig.auth, RecordingApi(), account, null) }
            assertEquals(CloudAuthProblem.INVALID_INPUT, e.problem)
            // a hand-built target with a name that would have changed the host
            val evil = BackupTarget(mapOf(TargetField.BUCKET to "x.evil.example/..", TargetField.REGION to "eu-west-2"))
            assertFailsWith<CloudAuthException> { CloudStores.storeFor(rig.auth, RecordingApi(), account, evil) }
            val evilHost = BackupTarget(mapOf(TargetField.BUCKET to "my-backups", TargetField.REGION to "auto", TargetField.ENDPOINT to "evil.example/x@y"))
            assertFailsWith<CloudAuthException> { CloudStores.storeFor(rig.auth, RecordingApi(), account, evilHost) }
        }
    }

    // @verifies SWR-MG-21#b
    // @verifies SWR-MG-23#b
    @Test
    fun aGoogleDriveAndAOneDriveStoreSendTheAccountsOwnTokenToTheServiceAndGetAFreshOneWhenItHasExpired() = runBlocking {
        for ((service, host) in listOf(CloudService.GOOGLE_DRIVE to "googleapis.com", CloudService.ONEDRIVE to "graph.microsoft.com", CloudService.DROPBOX to "dropboxapi.com")) {
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
