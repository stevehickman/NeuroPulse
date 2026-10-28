package life.neurone.core.cloudauth

/** Where the app runs, as far as how a sign-in comes back to it: the three hosts that have a browser launcher (the web has none). */
enum class RedirectPlatform { ANDROID, IOS, DESKTOP }

/** Why a registered redirect address cannot work on a platform. */
enum class RedirectProblem {
    /** No scheme, or not an address at all. */
    MALFORMED,

    /** A computer's launcher listens on the loopback address (`http://127.0.0.1:<port>`), so that is the only redirect it can receive. */
    NEEDS_LOOPBACK,

    /** iOS's browser session catches a custom scheme and refuses `http` and `https` ([WebSessionRules.callbackScheme]). */
    NEEDS_CUSTOM_SCHEME,

    /** Google does not take a custom scheme on Android: the redirect must be an `https` App Link the app has claimed. */
    NEEDS_HTTPS_APP_LINK,

    /** A phone's redirect is a custom scheme or an `https` App Link; plain `http` could be read by anything on the network path. */
    NEEDS_NON_HTTP,
}

/**
 * Whether a provider's registered redirect address can work on a platform (SWR-MG-25), decided before a browser is opened so a build
 * registered the wrong way reads "not set up" and nobody is sent to an error page at the provider.
 *
 * The rules come from two places. The launchers: a computer's [LoopbackAuthorizationLauncher] receives only a loopback address, and iOS's
 * `ASWebAuthenticationSession` here watches only a custom scheme ([WebSessionRules]). And one provider's published statement: Google's
 * installed-app page (read 2026-10-28) says "Custom URI schemes are no longer supported on Android and Chrome apps" and that the loopback
 * address is deprecated for Android and iOS client types, so on Android Google's redirect must be an `https` App Link. **Nothing here is
 * known for Microsoft or Dropbox beyond the launchers**: a registration either of them refuses is found by the provider, not here.
 */
object RedirectRules {
    /** The problem with [redirectUri] for [family] on [platform], or null when nothing known rules it out. */
    fun problem(platform: RedirectPlatform, family: AccountFamily, redirectUri: String): RedirectProblem? {
        val colon = redirectUri.indexOf(':')
        val scheme = if (colon > 0) redirectUri.substring(0, colon).lowercase() else return RedirectProblem.MALFORMED
        if (scheme.any { !(it.isLetterOrDigit() || it == '.' || it == '+' || it == '-') }) return RedirectProblem.MALFORMED
        val web = scheme == "http" || scheme == "https"
        return when (platform) {
            RedirectPlatform.DESKTOP -> if (isLoopback(redirectUri)) null else RedirectProblem.NEEDS_LOOPBACK
            RedirectPlatform.IOS -> if (web) RedirectProblem.NEEDS_CUSTOM_SCHEME else null
            RedirectPlatform.ANDROID -> when {
                scheme == "http" -> RedirectProblem.NEEDS_NON_HTTP
                family == AccountFamily.GOOGLE && scheme != "https" -> RedirectProblem.NEEDS_HTTPS_APP_LINK
                else -> null
            }
        }
    }

    /** `http://127.0.0.1:<port>` or `http://localhost:<port>`, the form [LoopbackAuthorizationLauncher] can bind. */
    private fun isLoopback(redirectUri: String): Boolean {
        if (!redirectUri.startsWith("http://")) return false
        val authority = redirectUri.removePrefix("http://").substringBefore('/').substringBefore('?').substringBefore('#')
        val host = authority.substringBeforeLast(':', "")
        val port = authority.substringAfterLast(':', "").toIntOrNull() ?: return false
        return (host == "127.0.0.1" || host.equals("localhost", ignoreCase = true)) && port in 1..65535
    }
}
