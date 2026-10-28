package life.neurone.core.cloudauth

import life.neurone.core.cloud.CloudStoreException

/** Who issues the person's credential: one sign-in per family, whichever service of it is used. */
enum class AccountFamily { GOOGLE, MICROSOFT, APPLE, AWS }

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
)

/**
 * A Google OAuth client. [redirectUri] is the one registered for the client and the platform: a loopback address
 * (`http://127.0.0.1:<port>`) on a computer, the claimed HTTPS or reverse-client-ID redirect on a phone. Google ends custom URI schemes
 * on Android and deprecates loopback on phones (native-app page), so this is not a value the library can choose. [clientSecret] is
 * set only for a client type Google issues one for (a desktop client); Google's page calls it optional, and an Android or iOS client has none.
 */
class OAuthClientConfig(val clientId: String, val redirectUri: String, val clientSecret: String? = null)

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
 */
class CloudKitConfig(
    val container: String,
    val apiToken: String,
    val signInCallbackUri: String,
    val environment: String = "production",
    val baseUrl: String = "https://api.apple-cloudkit.com",
)

/** Why a sign-in step failed, in terms a host can act on (Gap 2 of `docs/np_sw_cloudstore_001.md`). */
enum class CloudAuthProblem {
    /** No client ID for the provider. */
    NOT_CONFIGURED,

    /** The person closed the browser. */
    CANCELLED,

    /** The person, or the provider, refused (`access_denied`). */
    DENIED,

    /** The redirect did not carry the `state` that was sent: not the answer to this request. */
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

/** An account as the host may see it: never a token. */
class CloudAccount(
    val id: String,
    val service: CloudService,
    val createdAtEpochSeconds: Long,
    /** True once the provider refused the stored credential: the person must sign in again ([CloudAuth.signIn] with `replacing`). */
    val needsSignIn: Boolean,
)

/** What signing out did at the provider. */
enum class Revocation {
    /** The provider confirmed the grant is revoked. */
    REVOKED,

    /** The provider has no call for it (Microsoft's v2 endpoint, Apple, AWS), so the grant stands until the person removes it in their account. */
    NOT_AVAILABLE,

    /** The call was made and failed or got no answer: the local credential is deleted, the grant may stand. */
    FAILED,
}
