package com.neurone.app.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.neurone.core.session.AdaptationEvent
import com.neurone.core.session.CompletedSessionSummary
import com.neurone.core.session.SessionHistoryStore
import com.neurone.core.session.SessionRecord

// Port of iOS SessionHistoryView + AdaptiveAdjustmentsCard. Day-granularity list of completed
// sessions (exact timestamps are UHDR-class and never stored); tapping opens a detail with the
// session metrics and the Adaptive Adjustments card. Adaptation events are not persisted (they
// live in UHDR), so the history-opened card shows its empty state — parity with iOS.

@Composable
fun HistoryScreen(store: SessionHistoryStore, modifier: Modifier = Modifier) {
    var selected by remember { mutableStateOf<SessionRecord?>(null) }

    val record = selected
    if (record != null) {
        SessionDetail(record = record, onBack = { selected = null }, modifier = modifier)
        return
    }

    if (store.records.isEmpty()) {
        Column(modifier.fillMaxSize().padding(24.dp)) {
            Text("Session history", style = MaterialTheme.typography.headlineMedium)
            Spacer(Modifier.height(8.dp))
            Text("Your completed sessions will appear here.", style = MaterialTheme.typography.bodyMedium)
        }
        return
    }

    LazyColumn(modifier = modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        items(store.records, key = { it.id }) { record ->
            Card(Modifier.fillMaxWidth().clickable { selected = record }) {
                Column(Modifier.padding(12.dp)) {
                    Text(record.protocolName, style = MaterialTheme.typography.titleMedium)
                    Text(record.sessionDay, style = MaterialTheme.typography.bodySmall) // day granularity — UHDR boundary
                    record.averageCoherenceScore?.let { Text("Coherence %.1f".format(it)) }
                    Text("Impedance ${record.impedancePassCount}/8 electrodes")
                }
            }
        }
    }
}

@Composable
private fun SessionDetail(record: SessionRecord, onBack: () -> Unit, modifier: Modifier = Modifier) {
    val summary = remember(record.id) { CompletedSessionSummary.fromRecord(record) }
    Column(modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp)) {
        TextButton(onClick = onBack) { Text("Back") }
        Text(record.protocolName, style = MaterialTheme.typography.headlineSmall)
        Text(record.sessionDay, style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(12.dp))

        Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp)) {
                Text("${summary.durationSeconds / 60} min session", style = MaterialTheme.typography.titleMedium)
                summary.averageCoherenceScore?.let { Text("Average coherence %.1f".format(it)) }
                summary.rmssdMilliseconds?.let { Text("RMSSD $it ms") }
                Text("Impedance ${summary.impedancePassCount}/8 electrodes passed")
            }
        }

        Spacer(Modifier.height(16.dp))
        AdaptiveAdjustmentsCard(events = summary.adaptationEvents)
    }
}

/**
 * Port of iOS AdaptiveAdjustmentsCard. Shows up to 5 closed-loop adjustments inline (plain
 * language, no raw biometrics), with "View all N" to expand; empty state when none were
 * recorded. GDPR Art. 13(2)(f) transparency surface.
 */
@Composable
fun AdaptiveAdjustmentsCard(events: List<AdaptationEvent>, modifier: Modifier = Modifier) {
    var expanded by remember { mutableStateOf(false) }
    Card(modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text("Adaptive adjustments", style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.height(8.dp))
            if (events.isEmpty()) {
                Text(
                    "No automatic adjustments were recorded for this session.",
                    style = MaterialTheme.typography.bodySmall,
                )
            } else {
                val shown = if (expanded) events else events.take(5)
                for (event in shown) {
                    Text("• ${event.trigger.plainLanguageDescription}", style = MaterialTheme.typography.bodySmall)
                    Spacer(Modifier.height(6.dp))
                }
                if (!expanded && events.size > 5) {
                    TextButton(onClick = { expanded = true }) { Text("View all ${events.size}") }
                }
            }
        }
    }
}
