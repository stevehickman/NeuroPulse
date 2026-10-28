package life.neurone.core.cloudauth

import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

private const val REDIRECT = "life.neurone.app.oauth:/redirect"

class ExternalBrowserLauncherTests {
    private val shown = mutableListOf<String>()
    private val launcher = ExternalBrowserLauncher { shown += it }

    // @verifies SWR-MG-19#f
    @Test
    fun theRedirectEndsTheWaitAndTheAddressIsHandedBackWhole() = runBlocking {
        val wait = async(start = CoroutineStart.UNDISPATCHED) { launcher.authorize("https://idp.example/auth?x=1", REDIRECT) }
        assertEquals(listOf("https://idp.example/auth?x=1"), shown)
        assertTrue(launcher.onRedirect("$REDIRECT?code=c&state=s"))
        assertEquals("$REDIRECT?code=c&state=s", wait.await())
    }

    // @verifies SWR-MG-19#f
    @Test
    fun onlyTheRegisteredRedirectIsTakenAndOtherAddressesAreLeftToTheHost() = runBlocking {
        val wait = async(start = CoroutineStart.UNDISPATCHED) { launcher.authorize("u", REDIRECT) }
        assertFalse(launcher.onRedirect("life.neurone.app.oauthX:/redirect?code=c"), "a longer scheme")
        assertFalse(launcher.onRedirect("life.neurone.app.oauth:/redirect.evil?code=c"), "a longer path")
        assertFalse(launcher.onRedirect("https://evil.example/?code=c"))
        assertFalse(launcher.onRedirect("life.neurone.app.oauth:/other"))
        assertTrue(wait.isActive, "none of them ended the sign-in")
        assertTrue(launcher.onRedirect("$REDIRECT#code=c"), "a fragment form is the redirect")
        assertEquals("$REDIRECT#code=c", wait.await())
    }

    // @verifies SWR-MG-19#f
    @Test
    fun anAddressWithNothingWaitingIsNotTakenAndAnAnswerCannotBeUsedTwice() = runBlocking {
        assertFalse(launcher.onRedirect("$REDIRECT?code=c"), "nothing is waiting")
        val wait = async(start = CoroutineStart.UNDISPATCHED) { launcher.authorize("u", REDIRECT) }
        assertTrue(launcher.onRedirect("$REDIRECT?code=one"))
        wait.await()
        assertFalse(launcher.onRedirect("$REDIRECT?code=two"), "the sign-in is over")
    }

    // @verifies SWR-MG-19#f
    @Test
    fun comingBackToTheAppWithoutAnAnswerCancelsAndAnAnswerBeforeTheResumeWins() = runBlocking {
        val closed = async(start = CoroutineStart.UNDISPATCHED) { runCatching { launcher.authorize("u", REDIRECT) } }
        launcher.onHostPaused()
        launcher.onHostResumed()
        val e = closed.await().exceptionOrNull() as CloudAuthException
        assertEquals(CloudAuthProblem.CANCELLED, e.problem)

        val answered = async(start = CoroutineStart.UNDISPATCHED) { launcher.authorize("u", REDIRECT) }
        launcher.onHostPaused()
        assertTrue(launcher.onRedirect("$REDIRECT?code=c"), "the system delivers the redirect, then resumes the app")
        launcher.onHostResumed()
        assertEquals("$REDIRECT?code=c", answered.await())
    }

    // @verifies SWR-MG-19#f
    @Test
    fun aResumeThatFollowsNoPauseDoesNotCancelAndTheScreensCancelDoes() = runBlocking {
        val wait = async(start = CoroutineStart.UNDISPATCHED) { runCatching { launcher.authorize("u", REDIRECT) } }
        launcher.onHostResumed()
        assertTrue(wait.isActive, "the app never left, so the browser has not shown yet")
        launcher.cancel()
        assertEquals(CloudAuthProblem.CANCELLED, (wait.await().exceptionOrNull() as CloudAuthException).problem)
        launcher.cancel() // nothing waiting: harmless
    }

    // @verifies SWR-MG-19#f
    @Test
    fun aSecondSignInCancelsTheFirstAndABrowserThatCannotOpenFailsWithAReason() = runBlocking {
        val first = async(start = CoroutineStart.UNDISPATCHED) { runCatching { launcher.authorize("a", REDIRECT) } }
        val second = async(start = CoroutineStart.UNDISPATCHED) { launcher.authorize("b", REDIRECT) }
        assertEquals(CloudAuthProblem.CANCELLED, (first.await().exceptionOrNull() as CloudAuthException).problem)
        assertTrue(launcher.onRedirect("$REDIRECT?code=c"))
        second.await()

        val broken = ExternalBrowserLauncher { throw IllegalStateException("no browser") }
        val e = assertFailsWith<CloudAuthException> { broken.authorize("u", REDIRECT) }
        assertEquals(CloudAuthProblem.PROVIDER, e.problem)
        assertFalse(broken.onRedirect("$REDIRECT?code=c"), "a failed open leaves nothing waiting")
    }
}
