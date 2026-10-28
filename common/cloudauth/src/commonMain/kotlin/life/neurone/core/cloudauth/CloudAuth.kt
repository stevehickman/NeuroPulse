package life.neurone.core.cloudauth

import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.longOrNull
import life.neurone.core.cloud.AccessToken
import life.neurone.core.cloud.AwsCredentials
import life.neurone.core.cloud.AwsCredentialsProvider
import life.neurone.core.cloud.CloudKitCredentials
import life.neurone.core.cloud.CloudStoreException
import life.neurone.core.cloud.percentDecode
import life.neurone.core.cloud.percentEncode
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import life.neurone.core.platform.Digests
import life.neurone.core.platform.epochSeconds
import life.neurone.core.platform.toHex

/**
 * Sign-in, credential storage and refresh for the cloud stores of `:cloudstore` (SWR-MG-11, `docs/np_sw_cloudstore_001.md` §4c).
 * It never holds a credential of NeurOne's: every token here is the person's own at the provider, kept on this install, sealed at rest
 * through [CredentialStore], and sent nowhere but to that provider.
 *
 * - **Google and Microsoft:** OAuth 2.0 authorization code with PKCE (RFC 7636, `S256`) as a public client. The person signs in in the
 *   system browser ([AuthorizationLauncher]); the library checks `state`, redeems the code, keeps the refresh token, and
 *   [accessToken] refreshes silently. A refresh the provider refuses (`invalid_grant`) marks the account [CloudAccount.needsSignIn]; a
 *   refresh that gets no answer does not.
 * - **Apple (CloudKit Web Services):** the web-authentication token Apple's sign-in page returns, kept beside the container's API token.
 * - **AWS:** the access key, secret and optional session token the person types ([addAwsKeys]); no browser sign-in exists.
 *
 * The identity of the person is never requested (no `openid`, `email` or `profile` scope): NeurOne needs a grant to a folder, not to know who
 * holds it, and an account is shown by the local id, which the host may label with whatever the person types.
 */
class CloudAuth(
    private val transport: HttpTransport,
    private val credentials: CredentialStore,
    private val config: CloudAuthConfig,
    private val launcher: AuthorizationLauncher,
    private val random: RandomSource = SystemRandomSource,
    private val sha256: (ByteArray) -> ByteArray = Digests::sha256,
    private val now: () -> Long = ::epochSeconds,
    // PROVISIONAL (UC-113): no measurement of how long a token endpoint takes on a slow network.
    private val timeoutSeconds: Int = 30,
    /** An access token this close to expiry is refreshed first, so it does not expire in flight. No figure is derived (`UC-112`). */
    // PROVISIONAL (UC-112): no measurement of clock error or of how long a request holds a token.
    private val expirySkewSeconds: Long = 60,
) {
    private val lock = Mutex()
    private val json = Json { ignoreUnknownKeys = true }

    /** The accounts on this install. */
    /**
     * Whether this build holds a registration for [service]'s provider. False means a sign-in to it fails with [CloudAuthProblem.NOT_CONFIGURED]
     * before anything is sent, so a screen can say so up front. AWS has no registration to hold: the person brings their own keys.
     */
    fun isConfigured(service: CloudService): Boolean = when (service.family) {
        AccountFamily.GOOGLE -> config.google != null
        AccountFamily.MICROSOFT -> config.microsoft != null
        AccountFamily.APPLE -> config.cloudKit != null
        AccountFamily.AWS -> true
    }

    suspend fun accounts(): List<CloudAccount> = lock.withLock { credentials.load().accounts.map { it.view() } }

    // ── Sign-in ────────────────────────────────────────────────────────────────────────────────────────────────────

    /**
     * Sign in to [service] through the system browser and keep the result. With [replacing] it signs an existing account in again (after
     * [CloudAccount.needsSignIn]) and keeps its id. AWS has no browser sign-in: use [addAwsKeys].
     */
    suspend fun signIn(service: CloudService, replacing: String? = null): CloudAccount {
        replacing?.let { checkReplacing(it, service) }
        return when (service.family) {
            AccountFamily.GOOGLE, AccountFamily.MICROSOFT -> oauthSignIn(service, replacing)
            AccountFamily.APPLE -> cloudKitSignIn(replacing)
            AccountFamily.AWS -> throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "AWS has no browser sign-in; the person's access key is added with addAwsKeys")
        }
    }

    private suspend fun checkReplacing(id: String, service: CloudService) {
        val existing = lock.withLock { credentials.load().accounts.firstOrNull { it.id == id } }
            ?: throw CloudAuthException(CloudAuthProblem.UNKNOWN_ACCOUNT, "no such account to sign in again")
        if (existing.service != service) throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "the account is for a different service")
    }

    private class OAuthEndpoints(
        val authorize: String,
        val token: String,
        val revoke: String?,
        val clientId: String,
        val redirectUri: String,
        val clientSecret: String?,
    )

    private fun endpoints(service: CloudService): OAuthEndpoints = when (service.family) {
        AccountFamily.GOOGLE -> {
            val c = config.google ?: throw CloudAuthException(CloudAuthProblem.NOT_CONFIGURED, "no Google OAuth client is configured")
            OAuthEndpoints(
                "https://accounts.google.com/o/oauth2/v2/auth", "https://oauth2.googleapis.com/token",
                "https://oauth2.googleapis.com/revoke", c.clientId, c.redirectUri, c.clientSecret,
            )
        }
        AccountFamily.MICROSOFT -> {
            val c = config.microsoft ?: throw CloudAuthException(CloudAuthProblem.NOT_CONFIGURED, "no Microsoft app registration is configured")
            val tenant = if (service == CloudService.ONEDRIVE) c.oneDriveTenant else c.azureTenant
            OAuthEndpoints(
                "https://login.microsoftonline.com/${percentEncode(tenant)}/oauth2/v2.0/authorize",
                "https://login.microsoftonline.com/${percentEncode(tenant)}/oauth2/v2.0/token",
                null, c.clientId, c.redirectUri, null,
            )
        }
        else -> throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "${service.name} does not use OAuth")
    }

    private suspend fun oauthSignIn(service: CloudService, replacing: String?): CloudAccount {
        val e = endpoints(service)
        val pkce = Pkce.create(random, sha256)
        val state = Base64Url.encode(random.nextBytes(16))
        val url = e.authorize + "?" + form(
            "client_id" to e.clientId,
            "redirect_uri" to e.redirectUri,
            "response_type" to "code",
            "scope" to service.scope!!,
            "state" to state,
            "code_challenge" to pkce.challenge,
            "code_challenge_method" to "S256",
        )
        val back = launcher.authorize(url, e.redirectUri)
        val code = codeFrom(back, e.redirectUri, state)
        val reply = tokenReply(
            postForm(
                e.token,
                listOfNotNull(
                    "client_id" to e.clientId,
                    "grant_type" to "authorization_code",
                    "code" to code,
                    "redirect_uri" to e.redirectUri,
                    "code_verifier" to pkce.verifier,
                    e.clientSecret?.let { "client_secret" to it },
                    if (service.family == AccountFamily.MICROSOFT) "scope" to service.scope!! else null,
                ),
            ),
            refreshing = false,
        )
        val refresh = reply["refresh_token"]?.jsonPrimitive?.contentOrNull
            ?: throw CloudAuthException(CloudAuthProblem.PROVIDER, "the provider returned no refresh token")
        val at = now()
        return store(service, replacing) { id, created ->
            StoredAccount(
                id, service, created, false,
                accessToken = reply["access_token"]!!.jsonPrimitive.content,
                accessTokenExpiresAt = at + (reply["expires_in"]?.jsonPrimitive?.longOrNull ?: 0L),
                refreshToken = refresh,
            )
        }
    }

    private fun codeFrom(redirect: String, redirectUri: String, state: String): String {
        if (!redirect.startsWith(redirectUri)) throw CloudAuthException(CloudAuthProblem.PROVIDER, "the browser came back to an address that is not the redirect")
        val p = queryParams(redirect, plusIsSpace = true)
        p["error"]?.let { err ->
            throw CloudAuthException(if (err == "access_denied") CloudAuthProblem.DENIED else CloudAuthProblem.PROVIDER, "sign-in was refused: $err")
        }
        if (p["state"] != state) throw CloudAuthException(CloudAuthProblem.STATE_MISMATCH, "the answer is not for this sign-in request")
        return p["code"] ?: throw CloudAuthException(CloudAuthProblem.PROVIDER, "the redirect carried no authorization code")
    }

    private suspend fun cloudKitSignIn(replacing: String?): CloudAccount {
        val c = config.cloudKit ?: throw CloudAuthException(CloudAuthProblem.NOT_CONFIGURED, "no CloudKit container is configured")
        val probe = transport.post(
            HttpRequest(
                "${c.baseUrl}/database/1/${percentEncode(c.container)}/${c.environment}/private/users/current?ckAPIToken=${percentEncode(c.apiToken)}",
                emptyMap(), ByteArray(0), "GET", timeoutSeconds,
            ),
        ) ?: throw CloudAuthException(CloudAuthProblem.NETWORK, "no answer from CloudKit")
        val body = parseObject(probe)
        val signInUrl = body?.get("redirectURL")?.jsonPrimitive?.contentOrNull
        if (body?.get("serverErrorCode")?.jsonPrimitive?.contentOrNull != "AUTHENTICATION_REQUIRED" || signInUrl == null) {
            throw CloudAuthException(CloudAuthProblem.PROVIDER, "CloudKit did not ask for sign-in (status ${probe.status})")
        }
        val back = launcher.authorize(signInUrl, c.signInCallbackUri)
        if (!back.startsWith(c.signInCallbackUri)) throw CloudAuthException(CloudAuthProblem.PROVIDER, "the browser came back to an address that is not the sign-in callback")
        // The token holds '+', '/' and '=' and may arrive encoded or not; '+' here is a plus, not a space.
        val token = queryParams(back, plusIsSpace = false)["ckWebAuthToken"]
            ?: throw CloudAuthException(CloudAuthProblem.DENIED, "sign-in returned no web authentication token")
        // Not verified here: Apple says a token serves one round trip and the next comes in the reply, so a probe would spend it. The first
        // adapter call is the check, and a refusal comes back as status 421 (needsSignIn).
        return store(CloudService.ICLOUD_CLOUDKIT, replacing) { id, created ->
            StoredAccount(id, CloudService.ICLOUD_CLOUDKIT, created, false, webAuthToken = token)
        }
    }

    /**
     * Keep the person's AWS access key as an account of [CloudService.AWS_S3]. [expiresAtEpochSeconds] is for temporary keys; 0 means
     * none. Nothing is sent to AWS here, so a wrong key is found by the first request and not by this call.
     */
    suspend fun addAwsKeys(
        accessKeyId: String,
        secretAccessKey: String,
        sessionToken: String? = null,
        expiresAtEpochSeconds: Long = 0,
        replacing: String? = null,
    ): CloudAccount {
        fun bad(s: String) = s.isEmpty() || s.any { it.isWhitespace() || it.code < 0x20 || it.code == 0x7F }
        if (bad(accessKeyId) || bad(secretAccessKey) || (sessionToken != null && bad(sessionToken))) {
            throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "an AWS key may not be empty or contain a space or control character")
        }
        if (expiresAtEpochSeconds != 0L && expiresAtEpochSeconds <= now()) throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "these AWS keys have already expired")
        replacing?.let { checkReplacing(it, CloudService.AWS_S3) }
        return store(CloudService.AWS_S3, replacing) { id, created ->
            StoredAccount(
                id, CloudService.AWS_S3, created, false,
                awsAccessKeyId = accessKeyId, awsSecretAccessKey = secretAccessKey, awsSessionToken = sessionToken, awsExpiresAt = expiresAtEpochSeconds,
            )
        }
    }

    private suspend fun store(service: CloudService, replacing: String?, make: (String, Long) -> StoredAccount): CloudAccount =
        lock.withLock {
            val vault = credentials.load()
            val old = replacing?.let { id -> vault.accounts.firstOrNull { it.id == id } }
            val id = old?.id ?: random.nextBytes(16).toHex()
            val made = make(id, old?.createdAtEpochSeconds ?: now())
            credentials.save(Vault(vault.version, vault.accounts.filter { it.id != id } + made))
            made.view()
        }

    // ── What the adapters take ─────────────────────────────────────────────────────────────────────────────────────

    /** The bearer token of a Google or Microsoft account for `GcsObjectStore`, `AzureBlobObjectStore`, `OneDriveObjectStore` or `GoogleDriveObjectStore`; refreshed when it is close to expiry. */
    fun accessToken(accountId: String): AccessToken = AccessToken { freshAccessToken(accountId) }

    private suspend fun freshAccessToken(accountId: String): String = lock.withLock {
        val vault = credentials.load()
        val acc = vault.accounts.firstOrNull { it.id == accountId } ?: throw CloudAuthException(CloudAuthProblem.UNKNOWN_ACCOUNT, "no such account")
        if (acc.service.family != AccountFamily.GOOGLE && acc.service.family != AccountFamily.MICROSOFT) {
            throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "${acc.service.name} has no OAuth access token")
        }
        if (acc.needsSignIn) throw CloudAuthException(CloudAuthProblem.SIGN_IN_REQUIRED, "the person must sign in again")
        val cached = acc.accessToken
        if (cached != null && acc.accessTokenExpiresAt - expirySkewSeconds > now()) return@withLock cached
        val refresh = acc.refreshToken ?: throw CloudAuthException(CloudAuthProblem.SIGN_IN_REQUIRED, "no refresh token is stored")
        val e = endpoints(acc.service)
        val reply = try {
            tokenReply(
                postForm(
                    e.token,
                    listOfNotNull(
                        "client_id" to e.clientId,
                        "grant_type" to "refresh_token",
                        "refresh_token" to refresh,
                        e.clientSecret?.let { "client_secret" to it },
                        if (acc.service.family == AccountFamily.MICROSOFT) "scope" to acc.service.scope!! else null,
                    ),
                ),
                refreshing = true,
            )
        } catch (x: CloudAuthException) {
            if (x.problem == CloudAuthProblem.SIGN_IN_REQUIRED) {
                credentials.save(Vault(vault.version, vault.accounts.map { if (it.id == accountId) it.copyWith(needsSignIn = true) else it }))
            }
            throw x
        }
        val access = reply["access_token"]!!.jsonPrimitive.content
        val expires = now() + (reply["expires_in"]?.jsonPrimitive?.longOrNull ?: 0L)
        // Microsoft returns a new refresh token with each refresh and says to replace the old one; Google usually returns none.
        val newRefresh = reply["refresh_token"]?.jsonPrimitive?.contentOrNull ?: refresh
        credentials.save(
            Vault(vault.version, vault.accounts.map { if (it.id == accountId) it.copyWith(accessToken = access, accessTokenExpiresAt = expires, refreshToken = newRefresh) else it }),
        )
        access
    }

    /** The keys of an AWS account for `S3ObjectStore`. Temporary keys past their expiry need [addAwsKeys] again. */
    fun awsCredentials(accountId: String): AwsCredentialsProvider = AwsCredentialsProvider {
        lock.withLock {
            val acc = credentials.load().accounts.firstOrNull { it.id == accountId } ?: throw CloudAuthException(CloudAuthProblem.UNKNOWN_ACCOUNT, "no such account")
            if (acc.service != CloudService.AWS_S3) throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "${acc.service.name} has no AWS keys")
            if (acc.awsExpiresAt != 0L && acc.awsExpiresAt <= now()) throw CloudAuthException(CloudAuthProblem.SIGN_IN_REQUIRED, "the temporary AWS keys have expired")
            AwsCredentials(acc.awsAccessKeyId!!, acc.awsSecretAccessKey!!, acc.awsSessionToken)
        }
    }

    /** The container's API token and the person's web token for `CloudKitObjectStore`. Pair it with [cloudKitTokenSink]. */
    fun cloudKitCredentials(accountId: String): CloudKitCredentials = CloudKitCredentials {
        val c = config.cloudKit ?: throw CloudAuthException(CloudAuthProblem.NOT_CONFIGURED, "no CloudKit container is configured")
        lock.withLock {
            val acc = credentials.load().accounts.firstOrNull { it.id == accountId } ?: throw CloudAuthException(CloudAuthProblem.UNKNOWN_ACCOUNT, "no such account")
            if (acc.service != CloudService.ICLOUD_CLOUDKIT) throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "${acc.service.name} is not a CloudKit account")
            if (acc.needsSignIn) throw CloudAuthException(CloudAuthProblem.SIGN_IN_REQUIRED, "the person must sign in again")
            c.apiToken to acc.webAuthToken
        }
    }

    /** What to pass as `CloudKitObjectStore`'s `onWebAuthToken`: keeps the replacement token CloudKit hands back, since each serves one round trip. */
    fun cloudKitTokenSink(accountId: String): suspend (String) -> Unit = { token ->
        lock.withLock {
            val vault = credentials.load()
            if (vault.accounts.any { it.id == accountId }) {
                credentials.save(Vault(vault.version, vault.accounts.map { if (it.id == accountId) it.copyWith(webAuthToken = token) else it }))
            }
        }
    }

    /** Record that the provider refused the credential, for a failure only the adapter sees (CloudKit's 421, a 401 from a store). */
    suspend fun markNeedsSignIn(accountId: String) {
        lock.withLock {
            val vault = credentials.load()
            credentials.save(Vault(vault.version, vault.accounts.map { if (it.id == accountId) it.copyWith(needsSignIn = true) else it }))
        }
    }

    // ── Sign-out ───────────────────────────────────────────────────────────────────────────────────────────────────

    /**
     * Forget the account and its credential here, then ask the provider to revoke the grant where it has a call for it (Google does; the
     * result says so). The local credential is deleted whatever the provider answers.
     */
    suspend fun signOut(accountId: String): Revocation {
        val removed = lock.withLock {
            val vault = credentials.load()
            val acc = vault.accounts.firstOrNull { it.id == accountId } ?: throw CloudAuthException(CloudAuthProblem.UNKNOWN_ACCOUNT, "no such account")
            credentials.save(Vault(vault.version, vault.accounts.filter { it.id != accountId }))
            acc
        }
        val revoke = if (removed.service.family == AccountFamily.GOOGLE) endpoints(removed.service).revoke else null
        val token = removed.refreshToken ?: removed.accessToken
        if (revoke == null || token == null) return Revocation.NOT_AVAILABLE
        return try {
            if (postForm(revoke, listOf("token" to token)).status == 200) Revocation.REVOKED else Revocation.FAILED
        } catch (e: CloudAuthException) {
            Revocation.FAILED
        }
    }

    // ── Wire ───────────────────────────────────────────────────────────────────────────────────────────────────────

    private fun form(vararg params: Pair<String, String>): String = form(params.toList())

    private fun form(params: List<Pair<String, String>>): String = params.joinToString("&") { "${percentEncode(it.first)}=${percentEncode(it.second)}" }

    private suspend fun postForm(url: String, params: List<Pair<String, String>>): HttpResponse =
        transport.post(
            HttpRequest(
                url,
                mapOf("Content-Type" to "application/x-www-form-urlencoded", "Accept" to "application/json"),
                form(params).encodeToByteArray(), "POST", timeoutSeconds,
            ),
        ) ?: throw CloudAuthException(CloudAuthProblem.NETWORK, "no answer from the provider")

    private fun parseObject(r: HttpResponse): JsonObject? = try {
        json.parseToJsonElement(r.body.decodeToString()).jsonObject
    } catch (e: Exception) {
        null
    }

    private fun tokenReply(r: HttpResponse, refreshing: Boolean): JsonObject {
        val obj = parseObject(r)
        val hasAccess = obj?.get("access_token")?.jsonPrimitive?.contentOrNull != null
        if (r.status == 200 && obj != null && hasAccess) return obj
        val err = obj?.get("error")?.jsonPrimitive?.contentOrNull
        throw when {
            err == "access_denied" -> CloudAuthException(CloudAuthProblem.DENIED, "the provider refused: $err")
            refreshing && err in SIGN_IN_ERRORS -> CloudAuthException(CloudAuthProblem.SIGN_IN_REQUIRED, "the provider no longer accepts the credential: $err")
            r.status >= 500 || r.status == 429 -> CloudAuthException(CloudAuthProblem.NETWORK, "the provider answered ${r.status}", r.status)
            else -> CloudAuthException(CloudAuthProblem.PROVIDER, "the token endpoint answered ${r.status}${err?.let { ": $it" } ?: ""}", r.status)
        }
    }

    companion object {
        /** OAuth errors on a refresh that mean the person must sign in again (RFC 6749 `invalid_grant`; Microsoft's `interaction_required` and kin). */
        private val SIGN_IN_ERRORS = setOf("invalid_grant", "interaction_required", "consent_required", "login_required")

        /** True when [e] says the credential is no longer accepted: this library's own sign-in-required, or an adapter's 401 or CloudKit's 421. */
        fun needsSignIn(e: Throwable): Boolean =
            (e is CloudAuthException && e.problem == CloudAuthProblem.SIGN_IN_REQUIRED) || (e is CloudStoreException && (e.status == 401 || e.status == 421))

        /** The query parameters of a redirect (or its fragment when it has no query), percent-decoded. */
        internal fun queryParams(url: String, plusIsSpace: Boolean): Map<String, String> {
            val noFragment = url.substringBefore('#')
            val raw = if ('?' in noFragment) noFragment.substringAfter('?') else if ('#' in url) url.substringAfter('#') else ""
            return raw.split('&').filter { it.isNotEmpty() }.associate {
                val k = it.substringBefore('=')
                val v = it.substringAfter('=', "")
                fun d(s: String) = percentDecode(if (plusIsSpace) s.replace('+', ' ') else s)
                d(k) to d(v)
            }
        }
    }
}
