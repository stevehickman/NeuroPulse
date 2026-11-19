package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onLast
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.datetime.LocalDate
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.session.CompletedSessionSummary
import life.neurone.core.session.HistoryRetention
import life.neurone.core.session.SessionHistoryStore
import life.neurone.shared.resources.*
import life.neurone.shared.ui.SessionHistorySection
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** Settings' session history section (`REQ-SW03-31`): the person picks the retention period and clears the history after confirming. */
@OptIn(ExperimentalTestApi::class)
class SessionHistorySettingsTest {
    private val used = listOf(Res.string.history_retention_90_days, Res.string.history_clear_button, Res.string.common_cancel)
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)

    private fun history() = SessionHistoryStore(InMemoryKeyValueStore(), today = { LocalDate(2026, 11, 19) }).also {
        it.record(CompletedSessionSummary("Protocol 1", LocalDate(2026, 11, 19), 600, null, null, 8))
    }

    // @verifies REQ-SW03-31#a
    @Test
    fun thePersonPicksHowLongTheHistoryIsKept() = runComposeUiTest {
        val store = history()
        setContent {
            strings = used.associateWith { stringResource(it) }
            androidx.compose.foundation.layout.Column { SessionHistorySection(store) } // as Settings lays it out
        }
        assertEquals(HistoryRetention.NO_LIMIT, store.retention.value)
        onNodeWithText(text(Res.string.history_retention_90_days)).performClick()
        waitForIdle()
        assertEquals(HistoryRetention.DAYS_90, store.retention.value)
    }

    // @verifies REQ-SW03-31#b
    @Test
    fun clearingAsksFirstAndACancelKeepsTheHistory() = runComposeUiTest {
        val store = history()
        setContent {
            strings = used.associateWith { stringResource(it) }
            androidx.compose.foundation.layout.Column { SessionHistorySection(store) } // as Settings lays it out
        }
        onNodeWithText(text(Res.string.history_clear_button)).performClick()
        onNodeWithText(text(Res.string.common_cancel)).performClick()
        waitForIdle()
        assertEquals(1, store.records.size)

        onNodeWithText(text(Res.string.history_clear_button)).performClick()
        onAllNodesWithText(text(Res.string.history_clear_button)).onLast().performClick()
        waitForIdle()
        assertTrue(store.records.isEmpty())
        onNodeWithText(text(Res.string.history_clear_button)).assertIsNotEnabled()
    }
}
