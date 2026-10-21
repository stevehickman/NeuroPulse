package life.neurone.shared

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.shared.resources.*
import life.neurone.shared.ui.CoherenceAnnouncer
import life.neurone.shared.ui.COHERENCE_ANNOUNCE_INTERVAL_MS
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test

/** A screen reader is told the coherence score, and not more often than every two seconds (`REQ-SW03-30`). */
@OptIn(ExperimentalTestApi::class)
class CoherenceAnnouncerTest {
    private var format = ""

    // @verifies REQ-SW03-30#a
    @Test
    fun theFirstScoreIsSpokenAtOnce() = runComposeUiTest {
        var score by mutableStateOf<Float?>(null)
        setContent {
            format = stringResource(Res.string.session_coherence_score_a11y_format, "1.5")
            CoherenceAnnouncer(score)
        }
        mainClock.autoAdvance = false
        score = 1.5f
        mainClock.advanceTimeByFrame()
        mainClock.advanceTimeByFrame()
        onNodeWithContentDescription(format).assertExists()
    }

    // @verifies REQ-SW03-30#b
    @Test
    fun aFasterScoreWaitsForTheGapAndThenSpeaksTheNewestOne() = runComposeUiTest {
        var score by mutableStateOf<Float?>(null)
        var first = ""
        var last = ""
        setContent {
            first = stringResource(Res.string.session_coherence_score_a11y_format, "1.0")
            last = stringResource(Res.string.session_coherence_score_a11y_format, "3.0")
            CoherenceAnnouncer(score)
        }
        mainClock.autoAdvance = false
        score = 1.0f
        mainClock.advanceTimeByFrame()
        mainClock.advanceTimeByFrame()
        onNodeWithContentDescription(first).assertExists()

        score = 2.0f
        mainClock.advanceTimeBy(500)
        score = 3.0f
        mainClock.advanceTimeBy(COHERENCE_ANNOUNCE_INTERVAL_MS / 2)
        onNodeWithContentDescription(first).assertExists()
        onNodeWithContentDescription(last).assertDoesNotExist()

        mainClock.advanceTimeBy(COHERENCE_ANNOUNCE_INTERVAL_MS)
        onNodeWithContentDescription(last).assertExists()
    }
}
