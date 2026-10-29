package life.neurone.core.cloudauth

import kotlinx.coroutines.runBlocking
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class AzureSasAuthTests {
    private val good = "sv=2024-11-04&spr=https&se=2030-01-01T00%3A00%3A00Z&sr=c&sp=rcwdl&sig=abc%2Bdef%3D"
    private val now = 1_000_000L

    // @verifies SWR-MG-26#a
    @Test
    fun aSignatureIsAContainerQueryStringThatCanReadCreateOrWriteDeleteAndList() {
        val ok = AzureSas.parse("  ?$good  ", now)!!
        assertEquals(good, ok.query, "the leading ? and the spaces are not part of it")
        assertEquals(1_893_456_000L, ok.expiresAtEpochSeconds, "2030-01-01T00:00:00Z")
        assertNotNull(AzureSas.parse("sv=2024-11-04&sp=rwdl&sig=x", now), "write in place of create; no expiry is kept as unknown")
        assertEquals(0L, AzureSas.parse("sv=2024-11-04&sp=rcdl&sig=x", now)!!.expiresAtEpochSeconds)
        assertEquals(1_893_456_000L, AzureSas.parse("sp=rcdl&se=2030-01-01&sig=x", now)!!.expiresAtEpochSeconds, "a date alone is midnight UTC")
        assertEquals(0L, AzureSas.parse("sp=rcdl&se=next-week&sig=x", now)!!.expiresAtEpochSeconds, "an expiry that cannot be read is left to Azure")

        for ((why, bad) in listOf(
            "empty" to "", "no signature" to "sv=2024-11-04&sp=rcwdl", "empty signature" to "sv=2024-11-04&sp=rcwdl&sig=",
            "two signatures" to "sp=rcwdl&sig=a&sig=b", "no permissions" to "sv=2024-11-04&sig=x", "read only" to "sp=r&sig=x",
            "no list" to "sp=rcwd&sig=x", "no delete" to "sp=rcwl&sig=x", "no create or write" to "sp=rdl&sig=x",
            "a blob, not a container" to "sr=b&sp=rcwdl&sig=x", "expired" to "sp=rcwdl&se=1970-01-01T00%3A00%3A00Z&sig=x",
            "a whole URL" to "https://acct.blob.core.windows.net/ctr?sp=rcwdl&sig=x", "a fragment" to "sp=rcwdl&sig=x#f", "a space inside" to "sp=rcwdl&sig=x y",
            "a parameter with no name" to "=x&sp=rcwdl&sig=x", "a newline" to "sp=rcwdl&sig=x\nsp=r",
        )) {
            assertNull(AzureSas.parse(bad, now), why)
        }
    }

    // @verifies SWR-MG-26#c
    @Test
    fun aSignatureIsKeptSealedReadAtEveryCallAndRefusedPastItsExpiryOrWhenAzureRefusedIt() = runBlocking {
        val rig = Rig()
        assertTrue(rig.auth.isConfigured(CloudService.AZURE_BLOB_SAS))
        val account = rig.auth.addAzureSas(good)
        assertEquals(CloudService.AZURE_BLOB_SAS, account.service)
        assertEquals(good, rig.auth.azureSas(account.id).query())

        val provider = rig.auth.azureSas(account.id)
        val newer = good.replace("sig=abc", "sig=xyz")
        assertEquals(account.id, rig.auth.addAzureSas(newer, replacing = account.id).id)
        assertEquals(newer, provider.query(), "a provider made before sees the replacement")

        assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { rig.auth.addAzureSas("sp=r&sig=x") }.problem)
        assertEquals(1, rig.auth.accounts().size, "a refused signature kept nothing")

        rig.clock = 1_893_456_001L // past 2030-01-01
        val e = assertFailsWith<CloudAuthException> { provider.query() }
        assertEquals(CloudAuthProblem.SIGN_IN_REQUIRED, e.problem)
        assertTrue(CloudAuth.needsSignIn(e))

        rig.clock = 1_000_000L
        rig.auth.markNeedsSignIn(account.id)
        assertEquals(CloudAuthProblem.SIGN_IN_REQUIRED, assertFailsWith<CloudAuthException> { provider.query() }.problem, "a signature Azure refused is not tried again")
    }

    // @verifies SWR-MG-26#c
    @Test
    fun thereIsNoSignInAndSigningOutHasNothingToRevoke() = runBlocking {
        val rig = Rig()
        val account = rig.auth.addAzureSas(good)
        assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.AZURE_BLOB_SAS) }.problem)
        assertTrue(rig.browser.urls.isEmpty())
        assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { rig.auth.accessToken(account.id).bearer() }.problem, "it is not an OAuth account")
        assertEquals(Revocation.NOT_AVAILABLE, rig.auth.signOut(account.id), "a signature cannot be revoked from here; it lapses or is rotated in Azure")
        assertTrue(rig.auth.accounts().isEmpty())
        assertTrue(rig.providers.seen.isEmpty(), "nothing was sent to anyone")
    }

    // @verifies SWR-MG-26#d
    @Test
    fun anAzureSignaturePlaceNeedsTheAccountAndContainerAndSendsTheSignatureNotAToken() = runBlocking {
        val rig = Rig()
        val account = rig.auth.addAzureSas(good)
        assertEquals(listOf(TargetField.STORAGE_ACCOUNT, TargetField.CONTAINER), CloudStores.targetFields(CloudService.AZURE_BLOB_SAS))
        val seen = mutableListOf<HttpRequest>()
        val api = object : HttpTransport {
            override suspend fun post(request: HttpRequest): HttpResponse? {
                seen += request
                return HttpResponse(404, emptyMap(), ByteArray(0))
            }
        }
        assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { CloudStores.storeFor(rig.auth, api, account, null) }.problem)
        val target = CloudStores.target(CloudService.AZURE_BLOB_SAS, mapOf(TargetField.STORAGE_ACCOUNT to "myaccount", TargetField.CONTAINER to "backups"))!!
        assertNull(CloudStores.storeFor(rig.auth, api, account, target).get("np-migration-bundle.npbk"))
        val req = seen.single()
        assertEquals("https://myaccount.blob.core.windows.net/backups/np-migration-bundle.npbk?$good", req.url)
        assertTrue(req.headers.keys.none { it.equals("Authorization", true) })
        assertEquals(false, CloudStores.routeFor(CloudService.AZURE_BLOB_SAS).atomicClaim)
        assertEquals("cloud-azure-blob-sas", CloudStores.routeFor(CloudService.AZURE_BLOB_SAS).id)
    }
}
