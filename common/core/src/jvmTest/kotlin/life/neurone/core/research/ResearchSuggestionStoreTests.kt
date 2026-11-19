package life.neurone.core.research

import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.models.ResearchCategory
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ResearchSuggestionStoreTests {

    private fun store(kv: InMemoryKeyValueStore = InMemoryKeyValueStore(), optedIn: () -> Boolean = { true }) =
        ResearchSuggestionStore(kv, today = { "2026-07-06" }, portalOptedIn = optedIn)

    private fun validDraft() = ResearchSuggestionDraft(
        title = "40Hz for sleep",
        body = "Study 40Hz gamma entrainment effect on sleep onset.",
        categories = setOf(ResearchCategory.SLEEP),
    )

    // @verifies REQ-SW03-12#a
    @Test
    fun invalidDraftIsRejected() {
        val s = store()
        assertFalse(s.submit(ResearchSuggestionDraft(title = "", body = "", categories = emptySet())))
        assertEquals(0, s.suggestions.size)
    }

    // @verifies REQ-SW03-12#a
    @Test
    fun validDraftInsertsAtFront() {
        val s = store()
        assertTrue(s.submit(validDraft()))
        assertTrue(s.submit(validDraft().copy(title = "Newer idea")))
        assertEquals("Newer idea", s.suggestions.first().title)
        assertEquals(2, s.suggestions.size)
        assertEquals("2026-07-06", s.suggestions.first().submittedAtDay)
    }

    // @verifies REQ-SW03-12#b
    @Test
    fun voteTogglesCountAndFlag() {
        val s = store()
        s.submit(validDraft())
        val id = s.suggestions.first().id
        s.toggleVote(id)
        assertTrue(s.suggestions.first().didVote)
        assertEquals(1, s.suggestions.first().voteCount)
        s.toggleVote(id)
        assertFalse(s.suggestions.first().didVote)
        assertEquals(0, s.suggestions.first().voteCount)
    }

    // @verifies REQ-SW03-12
    @Test
    fun participationIntentToggles() {
        val s = store()
        s.submit(validDraft())
        val id = s.suggestions.first().id
        s.toggleParticipationIntent(id)
        assertTrue(s.suggestions.first().hasParticipationIntent)
        assertEquals(1, s.suggestions.first().participationIntentCount)
    }

    // @verifies REQ-SW03-12#c
    @Test
    fun pledgeUpdatesCumulativeAmount() {
        val s = store()
        s.submit(validDraft())
        val id = s.suggestions.first().id
        s.pledge(25, id)
        assertEquals(25, s.suggestions.first().pledgeAmount)
        assertEquals(25, s.suggestions.first().pledgedAmount)
        // Raising the pledge adjusts the cumulative total by the delta only.
        s.pledge(50, id)
        assertEquals(50, s.suggestions.first().pledgeAmount)
        assertEquals(50, s.suggestions.first().pledgedAmount)
    }

    // @verifies REQ-SW03-12#d
    @Test
    fun suggestionsPersistAcrossReload() {
        val kv = InMemoryKeyValueStore()
        val s1 = store(kv)
        s1.submit(validDraft())
        val s2 = store(kv)
        assertEquals(1, s2.suggestions.size)
        assertEquals("40Hz for sleep", s2.suggestions.first().title)
        s2.submit(validDraft().copy(title = "After the reload"))
        assertEquals(1, s2.suggestions.map { it.authorDeviceToken }.toSet().size, "the author token persists across the reload")
    }

    // @verifies REQ-SW03-12#d
    @Test
    fun deviceTokenIsStableAcrossSubmissions() {
        val s = store()
        s.submit(validDraft())
        s.submit(validDraft().copy(title = "Second"))
        val tokens = s.suggestions.map { it.authorDeviceToken }.toSet()
        assertEquals(1, tokens.size, "all suggestions from this device share one opaque token")
        assertNull(null) // sanity
    }

    // OI-PRIV-08 (3): without the portal opt-in there is no portal, so nothing is authored and no token is made.
    // @verifies REQ-SW03-12#e
    @Test
    fun aPersonNotOptedIntoThePortalSubmitsNothingAndGetsNoToken() {
        val kv = InMemoryKeyValueStore()
        var optedIn = false
        val s = store(kv) { optedIn }
        assertFalse(s.canSubmit)
        assertFalse(s.submit(validDraft()))
        assertEquals(0, s.suggestions.size)
        assertNull(kv.getString(ResearchSuggestionStore.TOKEN_KEY), "no author token is made for a person who is not in the portal")
        assertFalse(ResearchSuggestionStore(kv).submit(validDraft()), "a store built without the consent state refuses")
        optedIn = true
        assertTrue(s.submit(validDraft()))
    }

    // @verifies REQ-SW03-12#f
    @Test
    fun forgettingTheAuthorTokenUnlinksWhatWasWrittenAndTheNextSubmissionGetsANewOne() {
        val kv = InMemoryKeyValueStore()
        val s = store(kv)
        s.submit(validDraft())
        s.submit(validDraft().copy(title = "Second"))
        val old = kv.getString(ResearchSuggestionStore.TOKEN_KEY)!!

        s.forgetAuthorToken()
        assertNull(kv.getString(ResearchSuggestionStore.TOKEN_KEY))
        assertEquals(2, s.suggestions.size, "the person's own record stays")
        assertTrue(s.suggestions.all { it.authorDeviceToken.isEmpty() }, "but names no author")
        assertTrue(store(kv).suggestions.all { it.authorDeviceToken.isEmpty() }, "after a reload too")
        assertFalse(kv.getString(life.neurone.core.protocol.PersistedKeys.RESEARCH_SUGGESTIONS_KEY)!!.contains(old))

        s.submit(validDraft().copy(title = "After opting in again"))
        val fresh = s.suggestions.first().authorDeviceToken
        assertTrue(fresh.isNotEmpty() && fresh != old, "a later submission cannot be joined to the earlier ones")
    }
}
