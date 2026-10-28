package life.neurone.core.cloudauth

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class WebSessionRulesTests {
    // @verifies SWR-MG-19#g
    @Test
    fun theSessionWatchesTheRedirectsCustomSchemeAndRefusesAWebAddressOrNoScheme() {
        assertEquals("life.neurone.app.oauth", WebSessionRules.callbackScheme("life.neurone.app.oauth:/redirect"))
        assertEquals("msauth.life.neurone", WebSessionRules.callbackScheme("MSAuth.Life.Neurone://auth"))
        assertEquals(CloudAuthProblem.PROVIDER, assertFailsWith<CloudAuthException> { WebSessionRules.callbackScheme("https://app.example.test/cb") }.problem)
        assertEquals(CloudAuthProblem.PROVIDER, assertFailsWith<CloudAuthException> { WebSessionRules.callbackScheme("HTTP://127.0.0.1:9004") }.problem)
        assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { WebSessionRules.callbackScheme("no-scheme-here") }.problem)
        assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException> { WebSessionRules.callbackScheme(":/x") }.problem)
    }

    // @verifies SWR-MG-19#g
    @Test
    fun anAddressWinsAClosedSessionIsCancelledAndAnythingElseIsAFailureThatQuotesNothing() {
        assertEquals("app:/cb?code=c", WebSessionRules.outcome("app:/cb?code=c", null, null))
        assertEquals("app:/cb?code=c", WebSessionRules.outcome("app:/cb?code=c", WebSessionRules.CANCEL_DOMAIN, 1L), "an address is an answer whatever else is set")
        val closed = assertFailsWith<CloudAuthException> { WebSessionRules.outcome(null, WebSessionRules.CANCEL_DOMAIN, WebSessionRules.CANCEL_CODE) }
        assertEquals(CloudAuthProblem.CANCELLED, closed.problem)
        val other = assertFailsWith<CloudAuthException> { WebSessionRules.outcome(null, WebSessionRules.CANCEL_DOMAIN, 2L) }
        assertEquals(CloudAuthProblem.PROVIDER, other.problem)
        val foreign = assertFailsWith<CloudAuthException> { WebSessionRules.outcome(null, "NSURLErrorDomain", 1L) }
        assertEquals(CloudAuthProblem.PROVIDER, foreign.problem)
        assertEquals(CloudAuthProblem.PROVIDER, assertFailsWith<CloudAuthException> { WebSessionRules.outcome(null, null, null) }.problem)
    }
}
