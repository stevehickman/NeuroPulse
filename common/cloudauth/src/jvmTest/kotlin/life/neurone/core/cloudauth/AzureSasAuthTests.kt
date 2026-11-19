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
        assertNotNull(AzureSas.parse("sv=2024-11-04&spr=https&se=2030-01-01&sr=c&sp=rwdl&sig=x", now), "write in place of create")
        assertNotNull(AzureSas.parse("spr=https&se=2030-01-01&sr=c&sp=rcdl&sig=x", now), "create in place of write")
        assertEquals(1_893_456_000L, AzureSas.parse("spr=https&sr=c&sp=rcdl&se=2030-01-01&sig=x", now)!!.expiresAtEpochSeconds, "a date alone is midnight UTC")

        fun with(sp: String = "rcwdl", rest: String = "spr=https&se=2030-01-01&sr=c") = "sv=2024-11-04&$rest&sp=$sp&sig=x"
        for ((why, bad) in listOf(
            "empty" to "", "no signature" to "sv=2024-11-04&spr=https&se=2030-01-01&sr=c&sp=rcwdl", "empty signature" to "spr=https&se=2030-01-01&sr=c&sp=rcwdl&sig=",
            "two signatures" to with() + "&sig=b", "no permissions" to "spr=https&se=2030-01-01&sr=c&sig=x", "read only" to with("r"),
            "no list" to with("rcwd"), "no delete" to with("rcwl"), "no create or write" to with("rdl"),
            "a blob, not a container" to with(rest = "spr=https&se=2030-01-01&sr=b"), "expired" to with(rest = "spr=https&sr=c&se=1970-01-01T00%3A00%3A00Z"),
            "a whole URL" to "https://acct.blob.core.windows.net/ctr?" + with(), "a fragment" to with() + "#f", "a space inside" to with() + " y",
            "a parameter with no name" to "=x&" + with(), "a newline" to with() + "\nsp=r",
        )) {
            assertNull(AzureSas.parse(bad, now), why)
        }
    }

    // @verifies SWR-MG-30#a
    @Test
    fun aSignatureForTheWholeAccountOrWithNoReadableExpiryOrOverPlainHttpOrWithMorePermissionsIsRefused() {
        fun with(sp: String = "rcwdl", rest: String = "spr=https&se=2030-01-01&sr=c") = "sv=2024-11-04&$rest&sp=$sp&sig=x"
        assertNotNull(AzureSas.parse(with(), now), "the form every case below departs from")
        for ((why, bad) in listOf(
            "no sr: an account-wide signature has none" to with(rest = "spr=https&se=2030-01-01"),
            "an account SAS as Azure writes one" to "sv=2024-11-04&ss=b&srt=sco&sp=rwdlc&se=2030-01-01T00:00:00Z&spr=https&sig=x",
            "an account SAS with a container sr added" to with(rest = "ss=b&srt=sco&spr=https&se=2030-01-01&sr=c"),
            "signed services alone" to with(rest = "ss=b&spr=https&se=2030-01-01&sr=c"),
            "sr given twice" to with(rest = "spr=https&se=2030-01-01&sr=c&sr=c"), "sr for a blob version" to with(rest = "spr=https&se=2030-01-01&sr=bv"),
            "sr for a directory" to with(rest = "spr=https&se=2030-01-01&sr=d"),
            "no expiry: it was kept as never expiring" to with(rest = "spr=https&sr=c"), "an expiry that cannot be read" to with(rest = "spr=https&sr=c&se=next-week"),
            "an empty expiry" to with(rest = "spr=https&sr=c&se="), "two expiries" to with(rest = "spr=https&sr=c&se=2030-01-01&se=2031-01-01"),
            "an expiry held by a stored access policy" to with(rest = "spr=https&sr=c&si=policy"),
            "an expiry at this very second" to with(rest = "spr=https&sr=c&se=1970-01-12T13%3A46%3A40Z"),
            "no spr: Azure's default allows http" to with(rest = "se=2030-01-01&sr=c"), "https and http" to with(rest = "spr=https,http&se=2030-01-01&sr=c"),
            "http" to with(rest = "spr=http&se=2030-01-01&sr=c"), "spr twice" to with(rest = "spr=https&spr=https&se=2030-01-01&sr=c"),
            "add" to with("racwdl"), "delete version" to with("rcwdxl"), "tags" to with("rcwdlt"), "find" to with("rcwdlf"), "move" to with("rcwdlm"),
            "execute" to with("rcwdle"), "ownership" to with("rcwdlo"), "permissions" to with("rcwdlp"), "immutability policy" to with("rcwdli"),
            "permanent delete" to with("rcwdly"), "a letter twice" to with("rrcwdl"), "an upper-case letter" to with("RCWDL"), "two sp fields" to with() + "&sp=r",
        )) {
            assertNull(AzureSas.parse(bad, now), why)
        }
    }

    // @verifies SWR-MG-30#a
    @Test
    fun theExpiryIsReadInEachFormAzureDocuments() {
        fun at(se: String) = AzureSas.parse("sr=c&spr=https&sp=rcwdl&se=$se&sig=x", now)?.expiresAtEpochSeconds
        val midnight = 1_893_456_000L // 2030-01-01T00:00:00Z
        assertEquals(midnight, at("2030-01-01"), "a date alone is midnight UTC")
        assertEquals(midnight + 12 * 3600 + 30 * 60, at("2030-01-01T12%3A30Z"), "no seconds")
        assertEquals(midnight + 12 * 3600 + 30 * 60 + 15, at("2030-01-01T12:30:15Z"), "seconds, colons not encoded")
        assertEquals(midnight + 15, at("2030-01-01T00%3A00%3A15.1234567Z"), "a fraction of a second is dropped")
        assertEquals(midnight, at("2030-01-01T00%3A00"), "no suffix is UTC")
        assertEquals(midnight + 15, at("2030-01-01T00%3A00%3A15"), "no suffix, with seconds")
        assertEquals(midnight - 2 * 3600, at("2030-01-01T00%3A00%3A00%2B02%3A00"), "an offset east of UTC is earlier")
        assertEquals(midnight + 5 * 3600 + 30 * 60, at("2030-01-01T00%3A00-05%3A30"), "an offset with no seconds")
        for (bad in listOf("2030-01-01T24%3A00Z", "2030-13-01", "2030-01-01T12%3A60Z", "2030-01-01x", "2030-01-01T12Z", "2030-01-01T00%3A00%2B25%3A00")) assertNull(at(bad), bad)
    }

    // @verifies SWR-MG-29#a
    @Test
    fun aSignatureKeptUnderTheEarlierLooserCheckIsNotUsed() = runBlocking {
        val rig = Rig()
        val account = rig.auth.addAzureSas(good)
        // What an earlier build could have kept: no sr, no expiry read (stored as 0, "never").
        val old = rig.kv.getString(CredentialStore.KEY)!!
        assertTrue(good in old && "\"azureSasExpiresAt\":1893456000" in old, "the stored record holds the signature and its expiry")
        rig.kv.putString(CredentialStore.KEY, old.replace(good, "sv=2024-11-04&sp=rcwdl&sig=abc").replace("\"azureSasExpiresAt\":1893456000", "\"azureSasExpiresAt\":0"))
        val e = assertFailsWith<CloudAuthException> { rig.auth.azureSas(account.id).query() }
        assertEquals(CloudAuthProblem.SIGN_IN_REQUIRED, e.problem, "it reads as needing a new signature, and is never sent")
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

        assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { rig.auth.addAzureSas("spr=https&se=2030-01-01&sr=c&sp=r&sig=x") }.problem)
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
