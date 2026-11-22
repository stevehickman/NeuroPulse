package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsOff
import androidx.compose.ui.test.assertIsOn
import androidx.compose.ui.test.isToggleable
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.consent.ConsentStore
import life.neurone.shared.resources.*
import life.neurone.shared.ui.ResearchAnalyticsSection
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The Settings switch for research analytics: what it sends is shown before it is turned on, and the store records the decision (`REQ-CONSENT-26`). */
@OptIn(ExperimentalTestApi::class)
class ResearchAnalyticsSectionTest {
    private val used = listOf(
        Res.string.research_analytics_consent_title, Res.string.research_analytics_consent_confirm, Res.string.common_cancel,
    )
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)

    private fun store(day: String = "2026-11-22"): ConsentStore {
        val kv = InMemoryKeyValueStore()
        return ConsentStore(kv, ResearchAnalyticsGate(kv, NoOpAnalyticsBackend()), today = { day })
    }

    private fun androidx.compose.ui.test.ComposeUiTest.show(store: ConsentStore) = setContent {
        strings = used.associateWith { stringResource(it) }
        ResearchAnalyticsSection(store)
    }

    // @verifies REQ-CONSENT-26#a
    @Test
    fun turningItOn_showsWhatIsSent_andNothingIsOnUntilThePersonConfirms() = runComposeUiTest {
        val store = store()
        show(store)

        onAllNodes(isToggleable()).onFirst().performClick()
        onNodeWithText(text(Res.string.research_analytics_consent_title)).assertExists()
        assertFalse(store.researchAnalyticsGranted, "the switch alone turns nothing on")

        onNodeWithText(text(Res.string.common_cancel)).performClick()
        assertFalse(store.researchAnalyticsGranted)
        onAllNodes(isToggleable()).onFirst().assertIsOff()

        onAllNodes(isToggleable()).onFirst().performClick()
        onNodeWithText(text(Res.string.research_analytics_consent_confirm)).performClick()
        assertTrue(store.researchAnalyticsGranted)
        onAllNodes(isToggleable()).onFirst().assertIsOn()
    }

    // @verifies REQ-CONSENT-26#b
    @Test
    fun turningItOff_isRecordedByTheStore_andTheScreenSaysWhen() = runComposeUiTest {
        val store = store(day = "2026-11-22").also { it.grantResearchAnalytics() }
        show(store)
        assertNull(store.researchAnalyticsWithdrawnAt)

        onAllNodes(isToggleable()).onFirst().performClick()
        assertFalse(store.researchAnalyticsGranted)
        assertEquals("2026-11-22#000001", store.researchAnalyticsWithdrawnAt)
        onAllNodes(isToggleable()).onFirst().assertIsOff()
        onNodeWithText("2026-11-22", substring = true).assertExists()
    }
}
