package life.neurone.web

import life.neurone.core.cloudauth.AuthorizationLauncher
import life.neurone.core.cloudauth.CloudAuthException
import life.neurone.core.cloudauth.CloudAuthProblem
import life.neurone.core.cloudauth.ExternalBrowserLauncher

/**
 * Signs in to a cloud provider in a small window of the person's own browser (SWR-MG-19). The provider sends that window back to
 * `oauth-callback.html` on this page's origin, which passes its address to this page over a `BroadcastChannel` and closes itself; the
 * wait ends when [ExternalBrowserLauncher] takes an address that is the registered redirect. A page cannot be reloaded for this: the
 * app keeps nothing in the browser (REQ-SW03-18 (b)), so a redirect of the whole page would lose the sign-in's own state.
 *
 * - **A window needs a click.** The browser opens a window only for a script running because of a click, for a few seconds after it. The
 *   sign-in must start from the person's tap, and a start that waits on the network first (CloudKit's) may be refused. A refused window
 *   fails the sign-in with a reason; it is never retried.
 * - **A closed window is not noticed.** A provider's page can set `Cross-Origin-Opener-Policy`, after which `window.closed` reads true
 *   at once, so it is not used to cancel. The screen's cancel ([cancel]) or the coroutine's cancellation ends the wait.
 * - Only an address on this origin is a redirect: [ExternalBrowserLauncher] takes only the registered one, and a registered address on
 *   another origin never receives the channel's messages, so the sign-in would wait for ever. The host registers
 *   `<origin>/oauth-callback.html`.
 *
 * Written without a browser to run it in here; see `NP-SW-CLOUDSTORE-001` §4c.6 for what was and was not checked.
 */
class WebAuthorizationLauncher : AuthorizationLauncher {
    private val inner = ExternalBrowserLauncher { address ->
        require(address.startsWith("https://")) { "a sign-in page is opened over https only" }
        if (!openSignInWindow(address)) throw IllegalStateException("the browser blocked the sign-in window")
    }

    init {
        listenForAnswers { address -> inner.onRedirect(address) }
    }

    override suspend fun authorize(authorizationUrl: String, redirectUri: String): String {
        if (!redirectUri.startsWith(currentOrigin() + "/")) {
            throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "the redirect address is not on this page's origin")
        }
        return inner.authorize(authorizationUrl, redirectUri)
    }

    /** The screen's own cancel; the sign-in window, if still open, is left for the person to close. */
    fun cancel() = inner.cancel()
}

@JsFun("() => location.origin")
private external fun currentOrigin(): String

// True if a window opened. The window keeps its opener: window.open with `noopener` always returns null, so a blocked window could not be told.
@JsFun(
    """(url) => {
        const w = window.open(url, 'neurone-signin', 'popup,width=520,height=720');
        return w !== null && w !== undefined;
    }""",
)
private external fun openSignInWindow(url: String): Boolean

// The channel carries only messages of this origin. A message that is not { url: string } is ignored.
@JsFun(
    """(cb) => {
        const channel = new BroadcastChannel('neurone-oauth');
        channel.onmessage = (e) => { if (e.data && typeof e.data.url === 'string') cb(e.data.url); };
    }""",
)
private external fun listenForAnswers(cb: (String) -> Unit)
