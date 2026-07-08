package com.neurone.core.research

import com.neurone.core.common.InMemoryKeyValueStore
import com.neurone.core.models.ResearchCategory
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ResearchSuggestionStoreTests {

    private fun store(kv: InMemoryKeyValueStore = InMemoryKeyValueStore()) =
        ResearchSuggestionStore(kv, today = { "2026-07-06" })

    private fun validDraft() = ResearchSuggestionDraft(
        title = "40Hz for sleep",
        body = "Study 40Hz gamma entrainment effect on sleep onset.",
        categories = setOf(ResearchCategory.SLEEP),
    )

    @Test
    fun invalidDraftIsRejected() {
        val s = store()
        assertFalse(s.submit(ResearchSuggestionDraft(title = "", body = "", categories = emptySet())))
        assertEquals(0, s.suggestions.size)
    }

    @Test
    fun validDraftInsertsAtFront() {
        val s = store()
        assertTrue(s.submit(validDraft()))
        assertTrue(s.submit(validDraft().copy(title = "Newer idea")))
        assertEquals("Newer idea", s.suggestions.first().title)
        assertEquals(2, s.suggestions.size)
        assertEquals("2026-07-06", s.suggestions.first().submittedAtDay)
    }

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

    @Test
    fun participationIntentToggles() {
        val s = store()
        s.submit(validDraft())
        val id = s.suggestions.first().id
        s.toggleParticipationIntent(id)
        assertTrue(s.suggestions.first().hasParticipationIntent)
        assertEquals(1, s.suggestions.first().participationIntentCount)
    }

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

    @Test
    fun suggestionsPersistAcrossReload() {
        val kv = InMemoryKeyValueStore()
        val s1 = store(kv)
        s1.submit(validDraft())
        val s2 = ResearchSuggestionStore(kv)
        assertEquals(1, s2.suggestions.size)
        assertEquals("40Hz for sleep", s2.suggestions.first().title)
    }

    @Test
    fun deviceTokenIsStableAcrossSubmissions() {
        val s = store()
        s.submit(validDraft())
        s.submit(validDraft().copy(title = "Second"))
        val tokens = s.suggestions.map { it.authorDeviceToken }.toSet()
        assertEquals(1, tokens.size, "all suggestions from this device share one opaque token")
        assertNull(null) // sanity
    }
}
