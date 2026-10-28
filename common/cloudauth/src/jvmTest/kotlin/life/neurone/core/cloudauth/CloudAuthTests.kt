package life.neurone.core.cloudauth

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.runBlocking
import life.neurone.core.cloud.CloudStoreException
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.data.RandomSource
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import life.neurone.core.platform.Digests
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

internal const val GOOGLE_TOKEN = "https://oauth2.googleapis.com/token"
internal const val GOOGLE_REVOKE = "https://oauth2.googleapis.com/revoke"
internal const val MS_CONSUMERS_TOKEN = "https://login.microsoftonline.com/consumers/oauth2/v2.0/token"
internal const val MS_ORG_TOKEN = "https://login.microsoftonline.com/organizations/oauth2/v2.0/token"
internal const val DROPBOX_TOKEN = "https://api.dropboxapi.com/oauth2/token"
internal const val DROPBOX_REDIRECT = "http://127.0.0.1:9005"
internal const val GOOGLE_REDIRECT = "http://127.0.0.1:9004"
internal const val MS_REDIRECT = "msal-test://auth"
internal const val CK_CALLBACK = "https://app.example.test/ck-callback"

/** A deterministic counter in place of the platform's randomness, so two draws differ and a test can predict none of it. */
internal class CountingRandom : RandomSource {
    private var n = 0
    override fun nextBytes(count: Int): ByteArray = ByteArray(count) { (n++ * 7 + it * 13 + 1).toByte() }
}

/**
 * A stand-in for the providers' token endpoints and CloudKit's `users/current`, written from the pages read 2026-10-28 (Google's
 * native-app OAuth page, Microsoft's authorization-code-flow and refresh-token pages, Apple's CloudKit Web Services "Composing Web Service
 * Requests"). It checks that a code is redeemed with the verifier whose challenge the authorization request carried. **It is not a provider.**
 */
internal class FakeProviders : HttpTransport {
    val seen = mutableListOf<HttpRequest>()
    var offline = false
    var refreshError: String? = null // answers a refresh with this OAuth error (HTTP 400)
    var refreshStatus = 400
    var rotateRefresh = false // Microsoft-style: a new refresh token with each refresh
    var revokeStatus = 200
    private var issued = 0
    val challenges = HashMap<String, String>() // code -> code_challenge, set by the launcher
    val validRefresh = mutableSetOf<String>()

    fun form(r: HttpRequest) = r.body.decodeToString().split('&').filter { it.isNotEmpty() }.associate {
        val kv = it.split('=', limit = 2)
        life.neurone.core.cloud.percentDecode(kv[0]) to life.neurone.core.cloud.percentDecode(kv.getOrElse(1) { "" })
    }

    private fun json(code: Int, body: String) = HttpResponse(code, emptyMap(), body.encodeToByteArray())

    override suspend fun post(request: HttpRequest): HttpResponse? = synchronized(this) {
        if (offline) return null
        seen += request
        val url = request.url
        if (url.startsWith("https://api.apple-cloudkit.com/")) {
            return json(421, """{"serverErrorCode":"AUTHENTICATION_REQUIRED","reason":"request needs authorization","redirectURL":"https://idmsa.test/sign-in?x=1"}""")
        }
        val f = form(request)
        if (url == GOOGLE_REVOKE) return json(revokeStatus, "{}")
        if (url != GOOGLE_TOKEN && url != MS_CONSUMERS_TOKEN && url != MS_ORG_TOKEN && url != DROPBOX_TOKEN) return json(404, "{}")
        return when (f["grant_type"]) {
            "authorization_code" -> {
                val want = challenges[f["code"]] ?: return json(400, """{"error":"invalid_grant"}""")
                val got = Base64Url.encode(Digests.sha256(f["code_verifier"]!!.encodeToByteArray()))
                if (got != want) return json(400, """{"error":"invalid_grant"}""")
                val refresh = "refresh-${++issued}"
                validRefresh += refresh
                json(200, """{"access_token":"access-$issued","expires_in":3599,"refresh_token":"$refresh","token_type":"Bearer","scope":"${f["scope"] ?: "granted"}"}""")
            }
            "refresh_token" -> {
                refreshError?.let { return json(refreshStatus, """{"error":"$it"}""") }
                if (f["refresh_token"] !in validRefresh) return json(400, """{"error":"invalid_grant"}""")
                issued++
                val extra = if (rotateRefresh) {
                    val next = "refresh-$issued"; validRefresh += next; ""","refresh_token":"$next""""
                } else ""
                json(200, """{"access_token":"access-$issued","expires_in":3599,"token_type":"Bearer"$extra}""")
            }
            else -> json(400, """{"error":"unsupported_grant_type"}""")
        }
    }
}

/** The platform side: reads the authorization URL like a browser would and sends the browser back to the redirect. */
internal class FakeBrowser(private val providers: FakeProviders) : AuthorizationLauncher {
    val urls = mutableListOf<String>()
    var mode = "approve" // approve | deny | wrongState | cancel | other
    var callback: ((String, Map<String, String>) -> String)? = null
    private var codes = 0

    override suspend fun authorize(authorizationUrl: String, redirectUri: String): String {
        urls += authorizationUrl
        if (mode == "cancel") throw CloudAuthException(CloudAuthProblem.CANCELLED, "closed")
        callback?.let { return it(redirectUri, CloudAuth.queryParams(authorizationUrl, true)) }
        val p = CloudAuth.queryParams(authorizationUrl, true)
        return when (mode) {
            "deny" -> "$redirectUri?error=access_denied&state=${p["state"]}"
            "wrongState" -> "$redirectUri?code=c&state=not-the-state"
            "other" -> "https://evil.example.test/?code=c&state=${p["state"]}"
            else -> {
                val code = "code-${++codes}"
                providers.challenges[code] = p["code_challenge"]!!
                "$redirectUri?code=$code&state=${p["state"]}"
            }
        }
    }
}

internal class Rig(
    val config: CloudAuthConfig = CloudAuthConfig(
        google = OAuthClientConfig("g-client", GOOGLE_REDIRECT),
        microsoft = MicrosoftClientConfig("m-client", MS_REDIRECT),
        cloudKit = CloudKitConfig("iCloud.test.app", "ck-api-token", CK_CALLBACK),
        dropbox = DropboxClientConfig("db-key", DROPBOX_REDIRECT),
    ),
) {
    val providers = FakeProviders()
    val browser = FakeBrowser(providers)
    val kv = InMemoryKeyValueStore()
    var clock = 1_000_000L
    val auth = CloudAuth(providers, CredentialStore(kv), config, browser, CountingRandom(), Digests::sha256, { clock })
}

class CloudAuthTests {
    // @verifies SWR-MG-19#a
    @Test
    fun base64urlMatchesTheRfc4648VectorsAndUsesTheUrlAlphabetWithoutPadding() {
        listOf("" to "", "f" to "Zg", "fo" to "Zm8", "foo" to "Zm9v", "foob" to "Zm9vYg", "fooba" to "Zm9vYmE", "foobar" to "Zm9vYmFy").forEach { (i, o) ->
            assertEquals(o, Base64Url.encode(i.encodeToByteArray()), "'$i'")
        }
        assertEquals("-_8", Base64Url.encode(byteArrayOf(0xFB.toByte(), 0xFF.toByte())), "'+' and '/' become '-' and '_'")
    }

    // @verifies SWR-MG-19#a
    @Test
    fun pkceChallengeMatchesTheRfc7636AppendixBVectorThroughTheRustCore() {
        val verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"
        assertEquals("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM", Pkce.challengeFor(verifier, Digests::sha256))
        val made = Pkce.create(CountingRandom(), Digests::sha256)
        assertEquals(43, made.verifier.length, "32 bytes as base64url is 43 characters, the RFC's lower bound")
        assertTrue(made.verifier.all { it.isLetterOrDigit() || it == '-' || it == '_' })
    }

    // @verifies SWR-MG-19#a
    @Test
    fun aGoogleSignInSendsPkceAndTheDriveScopeRedeemsTheCodeAndKeepsTheRefreshToken() = runBlocking {
        val rig = Rig()
        val account = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        val q = CloudAuth.queryParams(rig.browser.urls.single(), true)
        assertTrue(rig.browser.urls.single().startsWith("https://accounts.google.com/o/oauth2/v2/auth?"))
        assertEquals("g-client", q["client_id"]); assertEquals(GOOGLE_REDIRECT, q["redirect_uri"]); assertEquals("code", q["response_type"])
        assertEquals("https://www.googleapis.com/auth/drive.appdata", q["scope"]); assertEquals("S256", q["code_challenge_method"])
        assertNotEquals(null, q["state"])
        val exchange = rig.providers.form(rig.providers.seen.single())
        assertEquals("authorization_code", exchange["grant_type"]); assertEquals(GOOGLE_REDIRECT, exchange["redirect_uri"])
        assertFalse("client_secret" in exchange, "no secret unless the client type has one")
        assertEquals(CloudService.GOOGLE_DRIVE, account.service); assertFalse(account.needsSignIn)
        assertEquals("access-1", rig.auth.accessToken(account.id).bearer())
        assertFalse("openid" in q["scope"]!! || "email" in q["scope"]!!, "the person's identity is not requested")
    }

    // @verifies SWR-MG-19#a
    @Test
    fun aDesktopGoogleClientSendsItsSecretAndAMicrosoftSignInAsksForARefreshTokenInThePersonalTenant() = runBlocking {
        val withSecret = Rig(CloudAuthConfig(google = OAuthClientConfig("g", GOOGLE_REDIRECT, clientSecret = "s3cret")))
        withSecret.auth.signIn(CloudService.GOOGLE_CLOUD_STORAGE)
        assertEquals("s3cret", withSecret.providers.form(withSecret.providers.seen.single())["client_secret"])
        assertEquals("https://www.googleapis.com/auth/devstorage.read_write", CloudAuth.queryParams(withSecret.browser.urls.single(), true)["scope"])

        val ms = Rig()
        ms.auth.signIn(CloudService.ONEDRIVE)
        assertTrue(ms.browser.urls.single().startsWith("https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize?"))
        assertEquals("Files.ReadWrite.AppFolder offline_access", CloudAuth.queryParams(ms.browser.urls.single(), true)["scope"])
        assertEquals("Files.ReadWrite.AppFolder offline_access", ms.providers.form(ms.providers.seen.single())["scope"])

        val az = Rig()
        az.auth.signIn(CloudService.AZURE_BLOB)
        assertTrue(az.browser.urls.single().startsWith("https://login.microsoftonline.com/organizations/oauth2/v2.0/authorize?"))
        assertEquals("https://storage.azure.com/user_impersonation offline_access", CloudAuth.queryParams(az.browser.urls.single(), true)["scope"])
    }

    // @verifies SWR-MG-19#b
    @Test
    fun theAccessTokenIsCachedUntilItIsNearExpiryThenRefreshedAndMicrosoftsRotatedRefreshTokenReplacesTheOld() = runBlocking {
        val rig = Rig(); rig.providers.rotateRefresh = true
        val a = rig.auth.signIn(CloudService.ONEDRIVE)
        val token = rig.auth.accessToken(a.id)
        assertEquals("access-1", token.bearer()); assertEquals("access-1", token.bearer())
        assertEquals(1, rig.providers.seen.size, "no refresh while the token is good")
        rig.clock += 3599 - 59 // 59 s left: inside the skew
        assertEquals("access-2", token.bearer())
        assertEquals("refresh-1", rig.providers.form(rig.providers.seen.last())["refresh_token"])
        rig.clock += 3599 - 59
        token.bearer()
        assertEquals("refresh-2", rig.providers.form(rig.providers.seen.last())["refresh_token"], "the rotated token is the one used next")
        assertEquals("Files.ReadWrite.AppFolder offline_access", rig.providers.form(rig.providers.seen.last())["scope"])
    }

    // @verifies SWR-MG-19#b
    @Test
    fun manySimultaneousCallsMakeOneRefresh() = runBlocking {
        val rig = Rig()
        val a = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        rig.clock += 10_000
        val token = rig.auth.accessToken(a.id)
        val got = (1..12).map { async(Dispatchers.Default) { token.bearer() } }.awaitAll()
        assertEquals(1, got.toSet().size)
        assertEquals(1, rig.providers.seen.count { rig.providers.form(it)["grant_type"] == "refresh_token" })
    }

    // @verifies SWR-MG-19#b
    @Test
    fun aRefreshTheProviderRefusesMarksTheAccountAndAsksForSignInWithStatus401ButNoAnswerDoesNot() = runBlocking {
        val rig = Rig()
        val a = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        rig.clock += 10_000
        rig.providers.offline = true
        val net = assertFailsWith<CloudAuthException> { rig.auth.accessToken(a.id).bearer() }
        assertEquals(CloudAuthProblem.NETWORK, net.problem)
        assertFalse(rig.auth.accounts().single().needsSignIn, "a missing answer is never a reason to sign out")
        rig.providers.offline = false; rig.providers.refreshStatus = 503; rig.providers.refreshError = "temporarily_unavailable"
        assertEquals(CloudAuthProblem.NETWORK, assertFailsWith<CloudAuthException> { rig.auth.accessToken(a.id).bearer() }.problem)
        assertFalse(rig.auth.accounts().single().needsSignIn)

        rig.providers.refreshStatus = 400; rig.providers.refreshError = "invalid_grant"
        val e = assertFailsWith<CloudAuthException> { rig.auth.accessToken(a.id).bearer() }
        assertEquals(CloudAuthProblem.SIGN_IN_REQUIRED, e.problem); assertEquals(401, e.status)
        assertTrue(CloudAuth.needsSignIn(e)); assertFalse("refresh-1" in e.message!!, "a token is never in a message")
        assertTrue(rig.auth.accounts().single().needsSignIn)
        val before = rig.providers.seen.size
        assertFailsWith<CloudAuthException> { rig.auth.accessToken(a.id).bearer() }
        assertEquals(before, rig.providers.seen.size, "a flagged account is not asked again")

        rig.providers.refreshError = null
        val again = rig.auth.signIn(CloudService.GOOGLE_DRIVE, replacing = a.id)
        assertEquals(a.id, again.id); assertFalse(again.needsSignIn); assertEquals(1, rig.auth.accounts().size)
        assertEquals("access-2", rig.auth.accessToken(a.id).bearer())
    }

    // @verifies SWR-MG-19#a
    @Test
    fun aRefusedWrongStateOrForeignRedirectOrClosedBrowserFailsWithAReasonAndStoresNothing() = runBlocking {
        val rig = Rig()
        rig.browser.mode = "deny"
        assertEquals(CloudAuthProblem.DENIED, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.GOOGLE_DRIVE) }.problem)
        rig.browser.mode = "wrongState"
        assertEquals(CloudAuthProblem.STATE_MISMATCH, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.GOOGLE_DRIVE) }.problem)
        rig.browser.mode = "other"
        assertEquals(CloudAuthProblem.PROVIDER, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.GOOGLE_DRIVE) }.problem)
        rig.browser.mode = "cancel"
        assertEquals(CloudAuthProblem.CANCELLED, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.GOOGLE_DRIVE) }.problem)
        assertTrue(rig.auth.accounts().isEmpty()); assertTrue(rig.providers.seen.isEmpty(), "nothing was redeemed")
    }

    // @verifies SWR-MG-19#d
    @Test
    fun aProviderWithNoClientConfiguredRefusesBeforeAnythingIsSentAndNamesIt() = runBlocking {
        val rig = Rig(CloudAuthConfig())
        for (s in listOf(CloudService.GOOGLE_DRIVE, CloudService.ONEDRIVE, CloudService.ICLOUD_CLOUDKIT)) {
            assertEquals(CloudAuthProblem.NOT_CONFIGURED, assertFailsWith<CloudAuthException> { rig.auth.signIn(s) }.problem, s.name)
        }
        assertTrue("Google" in assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.GOOGLE_CLOUD_STORAGE) }.message!!)
        assertTrue(rig.browser.urls.isEmpty() && rig.providers.seen.isEmpty())
    }

    // @verifies SWR-MG-19#e
    @Test
    fun cloudKitSignInOpensAppleSignInPageKeepsThePlusSignedTokenAndTheSinkReplacesIt() = runBlocking {
        val rig = Rig()
        rig.browser.callback = { redirect, _ -> "$redirect?ckWebAuthToken=ab+c/d=" }
        val a = rig.auth.signIn(CloudService.ICLOUD_CLOUDKIT)
        assertEquals("https://idmsa.test/sign-in?x=1", rig.browser.urls.single())
        assertEquals("GET", rig.providers.seen.single().method)
        assertTrue("ckAPIToken=ck-api-token" in rig.providers.seen.single().url)
        assertEquals("ck-api-token" to "ab+c/d=", rig.auth.cloudKitCredentials(a.id).tokens())

        rig.auth.cloudKitTokenSink(a.id)("next+token/2=")
        assertEquals("next+token/2=", rig.auth.cloudKitCredentials(a.id).tokens().second)

        rig.browser.callback = { redirect, _ -> "$redirect?ckWebAuthToken=x%2By%2Fz%3D" }
        val b = rig.auth.signIn(CloudService.ICLOUD_CLOUDKIT)
        assertEquals("x+y/z=", rig.auth.cloudKitCredentials(b.id).tokens().second, "an encoded token decodes to the same characters")

        rig.browser.callback = { redirect, _ -> "$redirect?nothing=1" }
        assertEquals(CloudAuthProblem.DENIED, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.ICLOUD_CLOUDKIT) }.problem)

        rig.auth.markNeedsSignIn(a.id)
        assertTrue(assertFailsWith<CloudAuthException> { rig.auth.cloudKitCredentials(a.id).tokens() }.status == 401)
    }

    // @verifies SWR-MG-19#e
    @Test
    fun awsKeysAreCheckedStoredAndHandedBackAndTemporaryOnesExpire() = runBlocking {
        val rig = Rig()
        for (bad in listOf("" to "s", "AKID" to "", "AK ID" to "s", "AKID" to "s\n")) {
            assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { rig.auth.addAwsKeys(bad.first, bad.second) }.problem, bad.toString())
        }
        assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { rig.auth.addAwsKeys("AKID", "s", "tok", expiresAtEpochSeconds = rig.clock - 1) }.problem)
        assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.AWS_S3) }.problem, "no browser sign-in for AWS")
        val a = rig.auth.addAwsKeys("AKID", "secret", "tok", expiresAtEpochSeconds = rig.clock + 100)
        val c = rig.auth.awsCredentials(a.id).credentials()
        assertEquals(Triple("AKID", "secret", "tok"), Triple(c.accessKeyId, c.secretAccessKey, c.sessionToken))
        rig.clock += 100
        val e = assertFailsWith<CloudAuthException> { rig.auth.awsCredentials(a.id).credentials() }
        assertEquals(CloudAuthProblem.SIGN_IN_REQUIRED, e.problem)
        assertTrue(rig.providers.seen.isEmpty(), "nothing is sent to AWS to add a key")
    }

    // @verifies SWR-MG-19#e
    @Test
    fun signOutDeletesTheCredentialAndRevokesWhereTheProviderCanAndSaysWhenItCannot() = runBlocking {
        val rig = Rig()
        val g = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        assertEquals(Revocation.REVOKED, rig.auth.signOut(g.id))
        assertEquals("refresh-1", rig.providers.form(rig.providers.seen.last())["token"])
        assertTrue(rig.auth.accounts().isEmpty())
        assertEquals(CloudAuthProblem.UNKNOWN_ACCOUNT, assertFailsWith<CloudAuthException> { rig.auth.accessToken(g.id).bearer() }.problem)

        val g2 = rig.auth.signIn(CloudService.GOOGLE_DRIVE); rig.providers.revokeStatus = 400
        assertEquals(Revocation.FAILED, rig.auth.signOut(g2.id)); assertTrue(rig.auth.accounts().isEmpty(), "deleted whatever the provider says")
        val g3 = rig.auth.signIn(CloudService.GOOGLE_DRIVE); rig.providers.offline = true
        assertEquals(Revocation.FAILED, rig.auth.signOut(g3.id)); rig.providers.offline = false

        val m = rig.auth.signIn(CloudService.ONEDRIVE)
        assertEquals(Revocation.NOT_AVAILABLE, rig.auth.signOut(m.id))
        assertEquals(Revocation.NOT_AVAILABLE, rig.auth.signOut(rig.auth.addAwsKeys("AKID", "s").id))
    }

    // @verifies SWR-MG-19#c
    @Test
    fun anUnreadableStoreIsNeverOverwrittenAndSeveralAccountsKeepTheirOwnCredentials() = runBlocking {
        val rig = Rig()
        val g = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        val g2 = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        assertNotEquals(g.id, g2.id)
        assertEquals("access-1", rig.auth.accessToken(g.id).bearer()); assertEquals("access-2", rig.auth.accessToken(g2.id).bearer())
        rig.kv.putString(CredentialStore.KEY, "{not json")
        assertEquals(CloudAuthProblem.STORE_UNREADABLE, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.GOOGLE_DRIVE) }.problem)
        assertEquals("{not json", rig.kv.getString(CredentialStore.KEY), "left as it was")
    }

    // @verifies SWR-MG-19#c
    @Test
    fun theCredentialsAreOneValueSealedAtRestAndNoTokenIsReadableInTheHostsRawStore() = runBlocking {
        val raw = InMemoryKeyValueStore()
        val sealed = EncryptedKeyValueStore(raw, InMemorySecretKeyStore(), setOf(CredentialStore.KEY))
        val providers = FakeProviders()
        val auth = CloudAuth(providers, CredentialStore(sealed), CloudAuthConfig(google = OAuthClientConfig("g", GOOGLE_REDIRECT)), FakeBrowser(providers), CountingRandom(), Digests::sha256, { 1L })
        val a = auth.signIn(CloudService.GOOGLE_DRIVE)
        val onDisk = raw.getString(CredentialStore.KEY)!!
        assertFalse("refresh-1" in onDisk || "access-1" in onDisk, "no token in the clear")
        assertEquals("access-1", auth.accessToken(a.id).bearer())
    }

    // @verifies SWR-MG-19#b
    @Test
    fun needsSignInReadsTheAuthProblemAndTheStatusesAdaptersReport() {
        assertTrue(CloudAuth.needsSignIn(CloudStoreException("x", 401)))
        assertTrue(CloudAuth.needsSignIn(CloudStoreException("x", 421)), "CloudKit's AUTHENTICATION_REQUIRED")
        assertFalse(CloudAuth.needsSignIn(CloudStoreException("x", 500)))
        assertFalse(CloudAuth.needsSignIn(CloudAuthException(CloudAuthProblem.NETWORK, "x")))
        assertFalse(CloudAuth.needsSignIn(RuntimeException("x")))
        assertNull(CloudAuthException(CloudAuthProblem.NETWORK, "x").status)
    }

    // @verifies SWR-MG-19#d
    @Test
    fun aProviderIsConfiguredOnlyWhenTheBuildHoldsItsRegistrationAndAwsAlwaysIs() {
        val some = Rig(CloudAuthConfig(google = OAuthClientConfig("g", GOOGLE_REDIRECT))).auth
        assertTrue(some.isConfigured(CloudService.GOOGLE_DRIVE))
        assertTrue(some.isConfigured(CloudService.GOOGLE_CLOUD_STORAGE))
        assertFalse(some.isConfigured(CloudService.ONEDRIVE))
        assertFalse(some.isConfigured(CloudService.AZURE_BLOB))
        assertFalse(some.isConfigured(CloudService.ICLOUD_CLOUDKIT))
        assertTrue(some.isConfigured(CloudService.AWS_S3))
        assertFalse(Rig(CloudAuthConfig()).auth.isConfigured(CloudService.GOOGLE_DRIVE))
    }

    // @verifies SWR-MG-19#a
    @Test
    fun theRedirectParserReadsAQueryOrAFragmentAndKeepsOrDropsPlusAsAskedForCredential() {
        assertEquals(mapOf("code" to "4/P7q", "state" to "a b"), CloudAuth.queryParams("http://127.0.0.1:9004/?code=4%2FP7q&state=a+b", true))
        assertEquals("a+b", CloudAuth.queryParams("http://x/?t=a+b", false)["t"])
        assertEquals("k", CloudAuth.queryParams("app://cb#code=k", true)["code"], "no query: the fragment")
        assertEquals("q", CloudAuth.queryParams("app://cb?code=q#code=f", true)["code"], "the query wins over a fragment")
        assertTrue(CloudAuth.queryParams("app://cb", true).isEmpty())
    }

    // @verifies SWR-MG-23#a
    @Test
    fun aDropboxSignInAsksForOfflineAccessWithPkceAndNoSecretAndKeepsARefreshableAccount() = runBlocking {
        val rig = Rig()
        assertTrue(rig.auth.isConfigured(CloudService.DROPBOX))
        val account = rig.auth.signIn(CloudService.DROPBOX)
        val url = rig.browser.urls.single()
        val p = CloudAuth.queryParams(url, true)
        assertTrue(url.startsWith("https://www.dropbox.com/oauth2/authorize?"), url)
        assertEquals("db-key", p["client_id"]); assertEquals(DROPBOX_REDIRECT, p["redirect_uri"]); assertEquals("code", p["response_type"])
        assertEquals("offline", p["token_access_type"]); assertEquals("S256", p["code_challenge_method"])
        assertEquals("files.content.write files.content.read account_info.read", p["scope"])
        val exchange = rig.providers.form(rig.providers.seen.single { it.url == DROPBOX_TOKEN })
        assertEquals("db-key", exchange["client_id"]); assertNull(exchange["client_secret"], "a public client sends no secret"); assertNull(exchange["scope"])
        assertEquals("access-1", rig.auth.accessToken(account.id).bearer())
        rig.clock += 3_600
        assertEquals("access-2", rig.auth.accessToken(account.id).bearer(), "refreshed with the refresh token")
        assertEquals(Revocation.NOT_AVAILABLE, rig.auth.signOut(account.id), "no revoke call is used for Dropbox")
    }

    // @verifies SWR-MG-23#a
    @Test
    fun aBuildWithNoDropboxAppCannotSignInToDropbox() = runBlocking {
        val rig = Rig(CloudAuthConfig(google = OAuthClientConfig("g-client", GOOGLE_REDIRECT)))
        assertFalse(rig.auth.isConfigured(CloudService.DROPBOX))
        assertEquals(CloudAuthProblem.NOT_CONFIGURED, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.DROPBOX) }.problem)
        assertTrue(rig.browser.urls.isEmpty(), "no browser was opened")
    }
}
