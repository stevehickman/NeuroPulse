package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.consent.ConsentStore
import life.neurone.core.models.ResearchCategory
import life.neurone.shared.ui.WithdrawnNote
import life.neurone.shared.ui.withdrawnDay
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** The consent screens show when a consent was withdrawn, from the store's record and only while it is off (SWR-MG-04; package 6zzz). */
@OptIn(ExperimentalTestApi::class)
class WithdrawnNoteTest {

    private object NoBackend : AnalyticsBackend {
        override fun configure() {}
        override fun reset() {}
        override fun track(event: String, properties: Map<String, String>) {}
    }

    // @verifies SWR-MG-04
    @Test
    fun theDayShowsOnlyWhileTheConsentIsOffAndATimeIsRecorded() {
        assertEquals("2026-10-17", withdrawnDay(granted = false, withdrawnAt = "2026-10-17T09:30:00Z"))
        // A later re-grant leaves the record but is not the current state.
        assertNull(withdrawnDay(granted = true, withdrawnAt = "2026-10-17T09:30:00Z"))
        // Never asked has no time, so it shows nothing and stays distinguishable from "said stop".
        assertNull(withdrawnDay(granted = false, withdrawnAt = null))
        assertNull(withdrawnDay(granted = false, withdrawnAt = ""))
    }

    // @verifies SWR-MG-04
    @Test
    fun theStoresRecordedWithdrawalReachesTheLine() = runComposeUiTest {
        val kv = InMemoryKeyValueStore()
        val store = ConsentStore(kv, ResearchAnalyticsGate(kv, NoBackend), now = { "2026-10-17T09:30:00Z" })
        store.updateResearchConsent(
            store.researchConsent.copy(categoryConsents = mapOf(ResearchCategory.entries.first() to true)),
        )
        store.updateResearchConsent(
            store.researchConsent.copy(categoryConsents = mapOf(ResearchCategory.entries.first() to false)),
        )
        val category = ResearchCategory.entries.first()
        val research = store.researchConsent
        setContent {
            WithdrawnNote(withdrawnDay(research.categoryConsents[category] ?: false, research.categoryWithdrawnAt[category]))
        }
        onNodeWithText("Withdrawn on 2026-10-17").assertExists()
    }
}
