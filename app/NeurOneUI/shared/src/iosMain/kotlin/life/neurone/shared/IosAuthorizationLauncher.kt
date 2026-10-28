package life.neurone.shared

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import life.neurone.core.cloudauth.AuthorizationLauncher
import life.neurone.core.cloudauth.CloudAuthException
import life.neurone.core.cloudauth.CloudAuthProblem
import life.neurone.core.cloudauth.WebSessionRules
import platform.AuthenticationServices.ASPresentationAnchor
import platform.AuthenticationServices.ASWebAuthenticationPresentationContextProvidingProtocol
import platform.AuthenticationServices.ASWebAuthenticationSession
import platform.Foundation.NSURL
import platform.UIKit.UIApplication
import platform.UIKit.UIWindow
import platform.darwin.NSObject
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/**
 * Signs in to a cloud provider in the system's web authentication session (SWR-MG-19): the person's own Safari sign-in, in a sheet the
 * app cannot read, which hands back only the address it was sent to. The session catches a custom-scheme redirect without the app
 * registering the scheme, so no `Info.plist` change is needed; an `https` redirect is refused (see [WebSessionRules.callbackScheme]).
 *
 * Only an `https` address is opened. The decisions about the redirect and the outcome are in [WebSessionRules], which has JVM tests;
 * this class is the platform calls around them and has not been built or run (no Mac in the session that wrote it).
 *
 * The session and its context provider are held here until the session answers: the session keeps its provider weakly, and a session
 * nothing holds can be freed before the person has finished.
 */
class IosAuthorizationLauncher : AuthorizationLauncher {
    private var session: ASWebAuthenticationSession? = null
    private var anchor: AnchorProvider? = null

    override suspend fun authorize(authorizationUrl: String, redirectUri: String): String {
        if (!authorizationUrl.startsWith("https://")) {
            throw CloudAuthException(CloudAuthProblem.PROVIDER, "a sign-in page is opened over https only")
        }
        val scheme = WebSessionRules.callbackScheme(redirectUri)
        val url = NSURL.URLWithString(authorizationUrl) ?: throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "the sign-in address is not an address")
        return withContext(Dispatchers.Main) {
            suspendCancellableCoroutine { cont ->
                val provider = AnchorProvider()
                val started = ASWebAuthenticationSession(url, scheme) { callback, error ->
                    session = null
                    anchor = null
                    try {
                        cont.resume(WebSessionRules.outcome(callback?.absoluteString, error?.domain, error?.code))
                    } catch (e: CloudAuthException) {
                        cont.resumeWithException(e)
                    }
                }
                started.presentationContextProvider = provider
                // The person's existing sign-in to the provider is what makes this one tap; a private session would ask for it again.
                started.prefersEphemeralWebBrowserSession = false
                session = started
                anchor = provider
                cont.invokeOnCancellation { started.cancel() }
                if (!started.start()) {
                    session = null
                    anchor = null
                    cont.resumeWithException(CloudAuthException(CloudAuthProblem.PROVIDER, "the sign-in sheet could not be shown"))
                }
            }
        }
    }
}

private class AnchorProvider : NSObject(), ASWebAuthenticationPresentationContextProvidingProtocol {
    override fun presentationAnchorForWebAuthenticationSession(session: ASWebAuthenticationSession): ASPresentationAnchor =
        UIApplication.sharedApplication.keyWindow ?: UIWindow()
}
