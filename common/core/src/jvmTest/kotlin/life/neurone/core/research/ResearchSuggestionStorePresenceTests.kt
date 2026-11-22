package life.neurone.core.research

import life.neurone.core.common.PresenceInstall
import life.neurone.core.models.ResearchCategory
import life.neurone.core.protocol.PersistedKeys
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The suggestion store under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-19`): built locked it must not replace the sealed
 * suggestions, and an author-token withdrawal made then must still reach them.
 */
class ResearchSuggestionStorePresenceTests {

    private val sealed = setOf(PersistedKeys.RESEARCH_SUGGESTIONS_KEY, ResearchSuggestionStore.TOKEN_KEY)

    private fun draft() = ResearchSuggestionDraft(
        title = "40Hz for sleep",
        body = "Study 40Hz gamma entrainment effect on sleep onset.",
        categories = setOf(ResearchCategory.SLEEP),
    )

    private fun installWithSuggestion(): PresenceInstall = PresenceInstall { sealed }.also { install ->
        val run = install.Run()
        run.unlock()
        val store = ResearchSuggestionStore(run.kv, today = { "2026-07-06" }, portalOptedIn = { true }, lock = run.lock)
        check(store.submit(draft()))
        run.lock.lock()
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_store_built_locked_neither_replaces_the_sealed_suggestions_nor_accepts_changes() {
        val run = installWithSuggestion().Run()
        val store = ResearchSuggestionStore(run.kv, today = { "2026-07-06" }, portalOptedIn = { true }, lock = run.lock)

        assertFalse(store.isReadable)
        assertTrue(store.suggestions.isEmpty())
        assertFalse(store.submit(draft()), "a suggestion is not written on a list that was not read")

        run.unlock()

        assertEquals(1, store.suggestions.size, "the sealed suggestion is still there, and no second one was written")
    }

    // The author token is the one value that joins a person's suggestions: a withdrawal must not wait for the unlock to remove it.
    // @verifies REQ-PRES-01#d
    @Test fun an_author_token_withdrawal_made_while_locked_removes_the_token_at_once_and_blanks_the_suggestions_at_the_unlock() {
        val run = installWithSuggestion().Run()
        val store = ResearchSuggestionStore(run.kv, today = { "2026-07-06" }, portalOptedIn = { true }, lock = run.lock)

        store.forgetAuthorToken()

        run.unlock()

        assertNull(run.kv.getString(ResearchSuggestionStore.TOKEN_KEY), "the token is gone")
        val kept = store.suggestions.single()
        assertEquals("40Hz for sleep", kept.title, "the person's own suggestion stays")
        assertEquals("", kept.authorDeviceToken, "and no longer names an author")
    }
}
