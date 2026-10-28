package life.neurone.core.cloudauth

import kotlinx.coroutines.CompletableDeferred

/**
 * An [AuthorizationLauncher] for a host that sends the person to the system's browser and gets the answer back as an address the
 * system hands to the app (Android's intent for a registered scheme; a computer's URL handler). The host supplies the three events
 * it can see and nothing else:
 *
 *  - [open] is given the address to show, once;
 *  - [onRedirect] is called with every address the system hands the app, and says whether this launcher took it;
 *  - [onHostPaused] and [onHostResumed] are the app leaving the foreground and coming back.
 *
 * **Why it watches the foreground.** The browser does not tell the app that the person closed it. If the app comes back to the
 * front without an answer having been delivered first, the sign-in is over: it fails with [CloudAuthProblem.CANCELLED], so a
 * screen waiting on it is not left waiting for ever. This depends on the system delivering the redirect before it resumes the app,
 * which Android does (`onNewIntent` or `onCreate`, then `onResume`). A pause and resume that is not the browser (a system dialog)
 * while a sign-in is waiting cancels it too; the person starts again.
 *
 * **What it accepts.** Only an address that is the registered redirect, followed by nothing, a path, a query or a fragment. Any
 * other address is not taken (the host handles it as usual), so another app's intent cannot end a sign-in. An address that is the
 * redirect but carries a forged `code` is [CloudAuth]'s to refuse: it checks `state`, and the code cannot be redeemed without the
 * PKCE verifier that never left the app.
 *
 * One sign-in waits at a time. Starting a second cancels the first.
 */
class ExternalBrowserLauncher(private val open: (String) -> Unit) : AuthorizationLauncher {
    private class Pending(val redirectUri: String) {
        val answer = CompletableDeferred<String>()
        var left = false
    }

    private var pending: Pending? = null

    override suspend fun authorize(authorizationUrl: String, redirectUri: String): String {
        val mine = Pending(redirectUri)
        // Single-threaded by the host's main thread; the swap is before the browser opens so no answer can beat it.
        pending?.answer?.completeExceptionally(cancelled("another sign-in was started"))
        pending = mine
        try {
            open(authorizationUrl)
        } catch (e: Exception) {
            if (pending === mine) pending = null
            throw CloudAuthException(CloudAuthProblem.PROVIDER, "the browser could not be opened")
        }
        try {
            return mine.answer.await()
        } finally {
            if (pending === mine) pending = null
        }
    }

    /** True if this address ended a waiting sign-in; false if nothing was waiting or the address is not the redirect. */
    fun onRedirect(address: String): Boolean {
        val p = pending ?: return false
        if (!isRedirect(address, p.redirectUri)) return false
        return p.answer.complete(address)
    }

    fun onHostPaused() {
        pending?.left = true
    }

    fun onHostResumed() {
        val p = pending ?: return
        if (p.left) p.answer.completeExceptionally(cancelled("the browser was closed without an answer"))
    }

    /** The screen's own cancel. */
    fun cancel() {
        pending?.answer?.completeExceptionally(cancelled("sign-in was cancelled"))
    }

    private fun cancelled(why: String) = CloudAuthException(CloudAuthProblem.CANCELLED, why)

    internal companion object {
        fun isRedirect(address: String, redirectUri: String): Boolean {
            if (!address.startsWith(redirectUri)) return false
            val rest = address.substring(redirectUri.length)
            return rest.isEmpty() || rest[0] == '?' || rest[0] == '#' || rest[0] == '/' || redirectUri.endsWith("/")
        }
    }
}
