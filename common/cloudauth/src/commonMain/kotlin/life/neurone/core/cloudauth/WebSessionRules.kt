package life.neurone.core.cloudauth

/**
 * The decisions a host makes around a browser session that hands its answer straight back to the caller (iOS
 * `ASWebAuthenticationSession`), kept apart from the platform code so they are tested on the JVM. The session itself is the host's.
 */
object WebSessionRules {
    /**
     * The scheme a session is told to watch for. A session catches a custom scheme the app has not registered; an `http` or `https`
     * redirect needs the platform's newer address-matching form (iOS 17.4), which a host that uses this does not have, so it is refused
     * here with a reason and nothing is opened.
     */
    fun callbackScheme(redirectUri: String): String {
        val scheme = redirectUri.substringBefore(':', "").lowercase()
        if (scheme.isEmpty() || redirectUri.indexOf(':') == -1) {
            throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "the redirect address has no scheme")
        }
        if (scheme == "http" || scheme == "https") {
            throw CloudAuthException(CloudAuthProblem.PROVIDER, "this browser session needs a custom-scheme redirect, not $scheme")
        }
        return scheme
    }

    /**
     * What a finished session means. The address it returned, if any, wins; a session the person closed or dismissed is
     * [CloudAuthProblem.CANCELLED]; anything else is [CloudAuthProblem.PROVIDER] without the platform's message (it can carry the
     * address, and the address carries the code).
     */
    fun outcome(callbackAddress: String?, errorDomain: String?, errorCode: Long?): String {
        if (callbackAddress != null) return callbackAddress
        if (errorDomain == CANCEL_DOMAIN && errorCode == CANCEL_CODE) {
            throw CloudAuthException(CloudAuthProblem.CANCELLED, "the sign-in was closed")
        }
        throw CloudAuthException(CloudAuthProblem.PROVIDER, "the browser session ended without an answer")
    }

    /** `ASWebAuthenticationSessionErrorDomain` and `ASWebAuthenticationSessionErrorCodeCanceledLogin` (Apple's documented constants). */
    const val CANCEL_DOMAIN = "com.apple.AuthenticationServices.WebAuthenticationSession"
    const val CANCEL_CODE = 1L
}
