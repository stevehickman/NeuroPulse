package life.neurone.core.cloudauth

import life.neurone.core.cloud.CloudStoreException

/** Who issues the person's credential: one sign-in per family, whichever service of it is used. */
enum class AccountFamily { GOOGLE, MICROSOFT, APPLE, AWS, DROPBOX, WEBDAV, AZURE_SAS }

/**
 * A cloud store the person can sign in to, one per adapter of `:cloudstore`. A person who uses Google Drive and Google Cloud
 * Storage signs in twice, once for each: Google does not support incremental authorization for installed apps (the native-app
 * page says so), so one grant carries the scopes of one service.
 */
enum class CloudService(val family: AccountFamily, val scope: String?) {
    /** The application data folder only: a non-sensitive scope (Google Drive app data guide). */
    GOOGLE_DRIVE(AccountFamily.GOOGLE, "https://www.googleapis.com/auth/drive.appdata"),
    GOOGLE_CLOUD_STORAGE(AccountFamily.GOOGLE, "https://www.googleapis.com/auth/devstorage.read_write"),

    /** The app folder only, and a refresh token (`offline_access`). A personal Microsoft account. */
    ONEDRIVE(AccountFamily.MICROSOFT, "Files.ReadWrite.AppFolder offline_access"),

    /** Azure Storage's one delegation scope, and a refresh token. Needs a work or school account: a personal account has no Azure subscription. */
    AZURE_BLOB(AccountFamily.MICROSOFT, "https://storage.azure.com/user_impersonation offline_access"),

    /** CloudKit Web Services. No scope: the container's API token and the person's web authentication token. */
    ICLOUD_CLOUDKIT(AccountFamily.APPLE, null),

    /**
     * The app folder only (the app is registered with the "App folder" access type), and a refresh token (`token_access_type=offline`). The scopes are
     * the ones the routes the store calls declare in Dropbox's API spec (read 2026-10-28, `UC-116`): `files.content.write` for upload and delete,
     * `files.content.read` for download, `files.metadata.read` for listing the folder. **Not `account_info.read`**: it is the scope of
     * `users/get_current_account` (the person's name and e-mail address) as well as of the space figure, and no identity is requested (SWR-MG-30 (c)).
     */
    DROPBOX(AccountFamily.DROPBOX, "files.content.write files.content.read files.metadata.read"),

    /**
     * Azure Blob authorized by a **shared access signature** the person makes in Azure and pastes in (SWR-MG-26): no Entra sign-in, so a personal Azure
     * account or any account the owner has no registration for can keep a backup. The container is a service SAS with the permissions `rcwdl`.
     */
    AZURE_BLOB_SAS(AccountFamily.AZURE_SAS, null),

    /**
     * A WebDAV server the person runs or rents (Nextcloud, ownCloud, a NAS): its address, a user name and a password (an app password, where the
     * server has them), typed in. No browser sign-in and no registration with anyone. The address must be `https`, because the password goes with every call.
     */
    WEBDAV(AccountFamily.WEBDAV, null),

    /** The person's own access key and secret (or temporary ones). No browser sign-in exists. */
    AWS_S3(AccountFamily.AWS, null),
}

/**
 * The client IDs NeurOne registered at each provider, handed in by the host. **There is no default and no placeholder**: a
 * provider with no entry cannot be signed in to, and [CloudAuthProblem.NOT_CONFIGURED] says which. A client ID is a public identifier,
 * not a secret.
 */
class CloudAuthConfig(
    val google: OAuthClientConfig? = null,
    val microsoft: MicrosoftClientConfig? = null,
    val cloudKit: CloudKitConfig? = null,
    val dropbox: DropboxClientConfig? = null,
)

/**
 * A Dropbox app registered as a public client (code flow with PKCE, no app secret). [appKey] is the app's key, a public identifier; [redirectUri]
 * is the one registered in the app's settings (a loopback address on a computer, the app's own scheme or claimed address on a phone).
 */
class DropboxClientConfig(val appKey: String, val redirectUri: String)

/**
 * A Google OAuth client. [redirectUri] is the one registered for the client and the platform: a loopback address
 * (`http://127.0.0.1:<port>`) on a computer, the claimed HTTPS or reverse-client-ID redirect on a phone. Google ends custom URI schemes
 * on Android and deprecates loopback on phones (native-app page), so this is not a value the library can choose.
 *
 * [clientSecret] is the value Google issues with a **Desktop app** client and with no other installed-app type (an Android or iOS client
 * has none). **It is not a secret**: it ships in an app anyone can download, so anyone can read it, and nothing may rely on it being
 * unknown (`SWR-MG-19` (m), `NP-SW-CLOUDSTORE-001` §4c.13). It is never the secret of a confidential client (a web-server or service
 * client), and it is sent only by a client whose redirect is the loopback address a computer receives: [secretForTokenCalls] is null
 * for any other redirect, so a value set by mistake in a phone build is never sent.
 */
class OAuthClientConfig(val clientId: String, val redirectUri: String, val clientSecret: String? = null) {
    /** [clientSecret] when this is a desktop-type client (a loopback redirect), else null. */
    val secretForTokenCalls: String? get() = clientSecret?.takeIf { RedirectRules.isLoopback(redirectUri) }
}

/**
 * A Microsoft Entra app registered as a public client. No secret: the native-app guidance is that a client secret "can't be reliably
 * stored on devices". The tenant is per service because a personal account (`consumers`) has no Azure and a work account has no use for OneDrive personal.
 */
class MicrosoftClientConfig(
    val clientId: String,
    val redirectUri: String,
    val oneDriveTenant: String = "consumers",
    val azureTenant: String = "organizations",
)

/**
 * The CloudKit container (it begins `iCloud.`), its API token (made in CloudKit Dashboard) and the sign-in callback URL set on that
 * token, to which Apple's sign-in page sends `ckWebAuthToken`. A server-to-server key is not supported: it reaches only the public database.
 *
 * **[apiToken] is public by design.** It identifies the container to CloudKit Web Services and goes in every request's address; it opens
 * nothing by itself, because the person's private database needs the `ckWebAuthToken` their own sign-in returns. It is compiled into the
 * app like a client ID. A server-to-server key is the confidential credential, and one is never put here (`NP-SW-CLOUDSTORE-001` §4c.13).
 *
 * **The callback carries the credential itself, and nothing in Apple's flow ties it to the request that opened the sign-in** (`OI-PRIV-13`):
 * there is no `state` and no code to redeem with a verifier, so whoever can deliver an address to the callback can hand in a token for an
 * iCloud account of their own. [CloudAuth] therefore signs in to CloudKit only when the callback is bound to the request, one of two ways:
 * the launcher is a [ConfinedCallbackLauncher], or [callbackNonceParameter] is set.
 *
 * [callbackNonceParameter] is the name of a query parameter that, added to Apple's sign-in address, comes back unchanged on the callback.
 * **None is known, so the default is null.** Apple's "Setting Up Web Services" page (read 2026-11-18) documents `ckWebAuthToken` as the only
 * thing on the callback and says nothing of a value of the caller's being returned. Set it only after a run against a real container has
 * shown the parameter return; a name Apple does not return makes every sign-in fail with [CloudAuthProblem.STATE_MISMATCH], never succeed unbound.
 */
class CloudKitConfig(
    val container: String,
    val apiToken: String,
    val signInCallbackUri: String,
    val environment: String = "production",
    val baseUrl: String = "https://api.apple-cloudkit.com",
    val callbackNonceParameter: String? = null,
)

/** Why a sign-in step failed, in terms a host can act on (Gap 2 of `docs/np_sw_cloudstore_001.md`). */
enum class CloudAuthProblem {
    /** No client ID for the provider. */
    NOT_CONFIGURED,

    /** The person closed the browser. */
    CANCELLED,

    /** The person, or the provider, refused (`access_denied`). */
    DENIED,

    /** The redirect did not carry the `state` (OAuth) or the nonce (CloudKit) that was sent: not the answer to this request. */
    STATE_MISMATCH,

    /** The stored credential no longer works (revoked, expired, a refresh the provider refused): the person must sign in again. */
    SIGN_IN_REQUIRED,

    /** The provider answered something this library does not understand, or refused for another reason. */
    PROVIDER,

    /** No answer, or the provider's server failed: try again later. Never a reason to sign out. */
    NETWORK,

    /** No such account here. */
    UNKNOWN_ACCOUNT,

    /** What the person typed is not a usable credential. */
    INVALID_INPUT,

    /** The stored credentials cannot be read. They are left as they are, not replaced. */
    STORE_UNREADABLE,
}

/**
 * A sign-in or credential failure. It is a [CloudStoreException], with status 401 when [problem] is [CloudAuthProblem.SIGN_IN_REQUIRED], so
 * everything that already reads an adapter's `status` to tell a refused token from other failures reads this the same way.
 * The message never contains a token.
 */
class CloudAuthException(val problem: CloudAuthProblem, message: String, status: Int? = null) :
    CloudStoreException(message, status ?: if (problem == CloudAuthProblem.SIGN_IN_REQUIRED) 401 else null)

/**
 * What the platform does for a sign-in: open [authorizationUrl] in the **system browser** (not an embedded web view, so the app never
 * sees the password), wait until the browser is sent to an address that begins with [redirectUri], and return that full address. Throw
 * [CloudAuthException] with [CloudAuthProblem.CANCELLED] when the person closes the browser. Each platform writes this once: a Custom
 * Tab or the app-link activity on Android, `ASWebAuthenticationSession` on iOS, a loopback listener on a computer, a redirect page on the web.
 */
fun interface AuthorizationLauncher {
    suspend fun authorize(authorizationUrl: String, redirectUri: String): String
}

/**
 * A launcher that can keep waiting past an address that is the redirect but is not the answer to this request. [isAnswer] is [CloudAuth]'s:
 * true only for an address that carries this request's `state` (or CloudKit nonce). An address it refuses is not handed back and does not
 * end the wait, so a page or another app that can reach the redirect cannot end somebody's sign-in (`OI-PRIV-13`). [CloudAuth] checks the
 * address it is handed again, so a launcher that does not implement this is no less safe, only easier to interrupt.
 */
interface SelectiveAuthorizationLauncher : AuthorizationLauncher {
    suspend fun authorize(authorizationUrl: String, redirectUri: String, isAnswer: (String) -> Boolean): String
}

/**
 * Marks a launcher whose answer can only be an address that **the browser session it opened was itself sent to**, so nothing outside that
 * session (another app, a page in another tab, a local process) can deliver one. iOS's `ASWebAuthenticationSession` is such a session by
 * Apple's description of it. A launcher that takes its answer from the system at large (an Android intent, a loopback listener, a page's
 * `BroadcastChannel`) is not, and must not be marked: the mark is what lets a CloudKit sign-in run with no nonce ([CloudKitConfig]).
 */
interface ConfinedCallbackLauncher : AuthorizationLauncher

/**
 * The `error` of an authorization redirect as one of a fixed set (RFC 6749 section 4.1.2.1), so the provider's text, which arrives in an
 * address anything on the device may have written, never reaches an exception message, a log or a screen.
 */
internal enum class OAuthRedirectError {
    ACCESS_DENIED, INVALID_REQUEST, UNAUTHORIZED_CLIENT, UNSUPPORTED_RESPONSE_TYPE, INVALID_SCOPE, SERVER_ERROR, TEMPORARILY_UNAVAILABLE, OTHER;

    companion object {
        fun of(code: String): OAuthRedirectError = values().firstOrNull { it != OTHER && it.name.lowercase() == code } ?: OTHER
    }
}

/** An account as the host may see it: never a token. */
class CloudAccount(
    val id: String,
    val service: CloudService,
    val createdAtEpochSeconds: Long,
    /** True once the provider refused the stored credential: the person must sign in again ([CloudAuth.signIn] with `replacing`). */
    val needsSignIn: Boolean,
    /**
     * Set only on the account a sign-in or an add **returned**, and only when it took the place of an earlier credential: what was done about
     * that earlier one at the provider. Anything but [Revocation.REVOKED] means it may still work there, and the host tells the person
     * (SWR-MG-30 (d)). Null when nothing was replaced.
     */
    val replaced: Revocation? = null,
)

/** What signing out did at the provider. */
enum class Revocation {
    /** The provider confirmed the grant is revoked. */
    REVOKED,

    /** The provider has no call for it (Microsoft's v2 endpoint, Apple, and any key, login or signature the person typed), so it stands until the person removes it in their account. */
    NOT_AVAILABLE,

    /** The call was made and failed or got no answer: the local credential is deleted, the grant may stand. */
    FAILED,
}
