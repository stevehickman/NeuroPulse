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
        val rest = "sr=c&spr=https&sig=x"
        assertNotNull(AzureSas.parse("sv=2024-11-04&sp=rwdl&se=2030-01-01&$rest", now), "write in place of create")
        assertNotNull(AzureSas.parse("sp=rcdl&se=2030-01-01&$rest", now), "create in place of write")

        for ((why, bad) in listOf(
            "empty" to "", "no signature" to "sp=rcwdl&se=2030-01-01&sr=c&spr=https", "empty signature" to "sp=rcwdl&se=2030-01-01&sr=c&spr=https&sig=",
            "two signatures" to "sp=rcwdl&se=2030-01-01&$rest&sig=b", "no permissions" to "se=2030-01-01&$rest", "read only" to "sp=r&se=2030-01-01&$rest",
            "no list" to "sp=rcwd&se=2030-01-01&$rest", "no delete" to "sp=rcwl&se=2030-01-01&$rest", "no create or write" to "sp=rdl&se=2030-01-01&$rest",
            "a blob, not a container" to "sr=b&spr=https&sp=rcwdl&se=2030-01-01&sig=x", "expired" to "sp=rcwdl&se=1970-01-01T00%3A00%3A00Z&$rest",
            "a whole URL" to "https://acct.blob.core.windows.net/ctr?sp=rcwdl&se=2030-01-01&$rest", "a fragment" to "sp=rcwdl&se=2030-01-01&$rest#f",
            "a space inside" to "sp=rcwdl&se=2030-01-01&$rest y", "a parameter with no name" to "=x&sp=rcwdl&se=2030-01-01&$rest",
            "a newline" to "sp=rcwdl&se=2030-01-01&$rest\nsp=r",
        )) {
            assertNull(AzureSas.parse(bad, now), why)
        }
    }

    // @verifies SWR-MG-26#a
    @Test
    fun aSignatureWiderThanOneContainerOverHttpsWithAnExpiryIsRefused() {
        fun sas(sr: String? = "sr=c", spr: String? = "spr=https", sp: String = "sp=rcwdl", se: String? = "se=2030-01-01", more: String? = null) =
            listOfNotNull("sv=2024-11-04", sr, spr, sp, se, more, "sig=x").joinToString("&")
        assertNotNull(AzureSas.parse(sas(), now), "the form the rest of this test changes one field of")

        for ((why, bad) in listOf(
            // Azure's account SAS has ss and srt and no sr, and reaches every container of the storage account.
            "no sr" to sas(sr = null), "an account signature" to sas(sr = null, more = "ss=b&srt=sco"), "sr with ss beside it" to sas(more = "ss=b"),
            "sr with srt beside it" to sas(more = "srt=sco"), "two sr" to sas(sr = "sr=c&sr=c"), "a directory" to sas(sr = "sr=d"), "an empty sr" to sas(sr = "sr="),
            // Azure's default for a missing spr is https,http.
            "no spr" to sas(spr = null), "http as well" to sas(spr = "spr=https,http"), "the same, encoded" to sas(spr = "spr=https%2Chttp"),
            "two spr" to sas(spr = "spr=https&spr=https"), "another case" to sas(spr = "spr=HTTPS"),
            // Anything beyond the five letters a backup uses.
            "add" to sas(sp = "sp=racwdl"), "tags" to sas(sp = "sp=rcwdlt"), "permanent delete" to sas(sp = "sp=rcwdly"), "set policy" to sas(sp = "sp=rcwdlp"),
            "delete version" to sas(sp = "sp=rcwdlx"), "two sp" to sas(sp = "sp=rcwdl&sp=rcwdl"),
            // An expiry that is absent or unreadable used to be kept as "never expires".
            "no expiry" to sas(se = null), "an unreadable expiry" to sas(se = "se=next-week"), "an empty expiry" to sas(se = "se="), "two expiries" to sas(se = "se=2030-01-01&se=2031-01-01"),
            "an hour of 24" to sas(se = "se=2030-01-01T24%3A00Z"), "a month of 13" to sas(se = "se=2030-13-01"), "text after the date" to sas(se = "se=2030-01-01x"),
            "only a stored policy" to "sv=2024-11-04&sr=c&spr=https&si=policy&sig=x",
        )) {
            assertNull(AzureSas.parse(bad, now), why)
        }
    }

    // @verifies SWR-MG-26#a
    @Test
    fun theExpiryIsReadInEachOfAzuresThreeForms() {
        fun at(se: String) = AzureSas.parse("sr=c&spr=https&sp=rcwdl&se=$se&sig=x", now)?.expiresAtEpochSeconds
        val midnight = 1_893_456_000L // 2030-01-01T00:00:00Z
        assertEquals(midnight, at("2030-01-01"), "a date alone is midnight UTC")
        assertEquals(midnight + 12 * 3600 + 30 * 60, at("2030-01-01T12%3A30Z"), "no seconds")
        assertEquals(midnight + 12 * 3600 + 30 * 60 + 15, at("2030-01-01T12:30:15Z"), "seconds, colons not encoded")
        assertEquals(midnight + 15, at("2030-01-01T00%3A00%3A15.1234567Z"), "a fraction of a second is dropped")
        assertEquals(midnight, at("2030-01-01T00%3A00"), "no suffix is UTC")
        assertEquals(midnight - 2 * 3600, at("2030-01-01T00%3A00%3A00%2B02%3A00"), "an offset east of UTC is earlier")
        assertEquals(midnight + 5 * 3600 + 30 * 60, at("2030-01-01T00%3A00-05%3A30"))
        assertNull(AzureSas.parse("sr=c&spr=https&sp=rcwdl&se=1970-01-01T00%3A00%3A00Z&sig=x", 0), "an expiry at the epoch itself is an expiry, and past")
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
        assertEquals(1_893_456_000L, rig.auth.accounts().single().credentialExpiresAtEpochSeconds, "the host can say when it lapses; no longest lifetime is set")

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
