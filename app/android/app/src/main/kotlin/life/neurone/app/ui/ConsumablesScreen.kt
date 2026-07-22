package life.neurone.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import life.neurone.app.NeurOneApplication
import life.neurone.core.consumable.ConsumableKind
import life.neurone.core.consumable.ConsumableState
import life.neurone.core.consumable.ReminderPriority

// Port of iOS ConsumableView. Lists all four consumables with sessions-remaining, a low/
// blocking badge, an order link, and snooze/replaced actions. Fed by the core
// ConsumableTracker (measurement-triggered per CLAUDE.md §5.2). Counts are SHDR-class.

private fun ConsumableKind.displayName(): String = when (this) {
    ConsumableKind.INTRANASAL_SLEEVES -> "Intranasal sleeves"
    ConsumableKind.ELECTRODE_HYDROGEL -> "Electrode hydrogel tips"
    ConsumableKind.VNS_PADS -> "VNS clip pads"
    ConsumableKind.AUDIO_CUP_FOAM -> "Audio cup foam"
}

@Composable
fun ConsumablesScreen(app: NeurOneApplication, modifier: Modifier = Modifier) {
    // Recompose when hub counts change; the app-scoped tracker updates its states from the
    // same session flow. `refresh` bumps to re-read tracker state after snooze/replace.
    val session by app.gattManager.session.collectAsState()
    var refresh by remember { mutableIntStateOf(0) }
    val tracker = app.consumableTracker
    // Touch `session` + `refresh` so this recomposes on either signal.
    @Suppress("UNUSED_EXPRESSION") session
    @Suppress("UNUSED_EXPRESSION") refresh
    val states = tracker.states

    Column(modifier.fillMaxSize().padding(16.dp)) {
        Text("Consumables", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(4.dp))
        Text(
            "Reminders are measurement-triggered from your hub's usage counters.",
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(12.dp))
        LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(states, key = { it.kind.rawValue }) { state ->
                ConsumableCard(
                    state = state,
                    onSnooze = { tracker.snooze(state.kind.rawValue); refresh++ },
                    onReplaced = { tracker.markReplaced(state.kind.rawValue); refresh++ },
                )
            }
        }
    }
}

@Composable
private fun Badge(text: String, color: Color) {
    Text(
        text,
        style = MaterialTheme.typography.labelSmall,
        color = Color.White,
        modifier = Modifier
            .clip(MaterialTheme.shapes.small)
            .background(color)
            .padding(horizontal = 6.dp, vertical = 2.dp),
    )
}

@Composable
private fun ConsumableCard(
    state: ConsumableState,
    onSnooze: () -> Unit,
    onReplaced: () -> Unit,
) {
    val uriHandler = LocalUriHandler.current
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(state.kind.displayName(), style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Medium)
                if (state.isLow) {
                    val blocking = state.kind.reminderPriority == ReminderPriority.SAFETY_BLOCKING
                    Badge(if (blocking) "Replace now" else "Low", if (blocking) Color(0xFFD32F2F) else Color(0xFFF9A825))
                }
            }
            Spacer(Modifier.height(6.dp))
            Text(
                "${state.sessionsRemaining} of ${state.kind.sessionLimit} sessions remaining",
                style = MaterialTheme.typography.bodySmall,
            )
            Spacer(Modifier.height(6.dp))
            LinearProgressIndicator(
                progress = { (state.sessionsRemaining.toFloat() / state.kind.sessionLimit).coerceIn(0f, 1f) },
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TextButton(onClick = { uriHandler.openUri(state.kind.orderUrl) }) { Text("Order") }
                TextButton(onClick = onReplaced) { Text("Mark replaced") }
                if (state.canSnooze) TextButton(onClick = onSnooze) { Text("Snooze") }
            }
        }
    }
}
