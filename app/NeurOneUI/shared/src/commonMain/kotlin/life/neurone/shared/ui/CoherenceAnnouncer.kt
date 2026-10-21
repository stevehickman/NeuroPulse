package life.neurone.shared.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import life.neurone.core.platform.formatFixed
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

/** The shortest gap between two spoken coherence scores (`REQ-SW03-30` (b), `UC-108`). */
internal const val COHERENCE_ANNOUNCE_INTERVAL_MS = 2_000L

/**
 * Tells a screen reader the session's coherence score as it changes, politely (it waits for speech in progress) and no more often than
 * [COHERENCE_ANNOUNCE_INTERVAL_MS], so a score that updates every few hundred milliseconds does not bury the breathing cue (`REQ-SW03-30`).
 * Nothing is drawn: a one-dp box carries the live region. The first score is spoken at once and the latest one is spoken when the gap
 * ends. There is no timer loop: the collector announces, waits, and then takes the newest value, so it is idle when the score is.
 */
@Composable
internal fun CoherenceAnnouncer(score: Float?) {
    val current by rememberUpdatedState(score)
    var announced by remember { mutableStateOf<Float?>(null) }
    LaunchedEffect(Unit) {
        snapshotFlow { current }.collect { value ->
            if (value != null) {
                announced = value
                delay(COHERENCE_ANNOUNCE_INTERVAL_MS)
            }
        }
    }
    val spoken = announced?.let { stringResource(Res.string.session_coherence_score_a11y_format, formatFixed(it.toDouble(), 1)) } ?: ""
    Box(Modifier.size(1.dp).semantics { liveRegion = LiveRegionMode.Polite; contentDescription = spoken })
}
