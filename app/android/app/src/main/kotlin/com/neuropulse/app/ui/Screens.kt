package com.neuropulse.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.neuropulse.app.NeuroPulseApplication
import com.neuropulse.app.R
import com.neuropulse.core.consent.ConsentStore
import com.neuropulse.core.session.SessionHistoryStore

// Screen skeletons — structure and privacy wiring in place; visual completion
// tracked as follow-up work in app/android/ISA.md (Out of Scope note).
// Every stimulation-adjacent screen carries the regulatory footer (parity
// with iOS App Store constraint).

@Composable
fun SessionScreen(modifier: Modifier = Modifier) {
    Column(
        modifier = modifier.fillMaxSize().padding(24.dp),
        verticalArrangement = Arrangement.Center,
    ) {
        Text("Session", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))
        Text(stringResource(R.string.bluetooth_off_message))
        Spacer(Modifier.height(16.dp))
        Text(
            stringResource(R.string.regulatory_footer),
            style = MaterialTheme.typography.bodySmall,
        )
    }
}

@Composable
fun HistoryScreen(store: SessionHistoryStore, modifier: Modifier = Modifier) {
    LazyColumn(modifier = modifier.fillMaxSize().padding(16.dp)) {
        items(store.records) { record ->
            Card(Modifier.padding(vertical = 4.dp)) {
                Column(Modifier.padding(12.dp)) {
                    Text(record.protocolName, style = MaterialTheme.typography.titleMedium)
                    // Day granularity only — exact timestamps are UHDR-class.
                    Text(record.sessionDay, style = MaterialTheme.typography.bodySmall)
                    record.averageCoherenceScore?.let {
                        Text("Coherence %.1f".format(it))
                    }
                    Text("Impedance ${record.impedancePassCount}/8 electrodes")
                }
            }
        }
    }
}

@Composable
fun ConsumablesScreen(modifier: Modifier = Modifier) {
    Column(modifier = modifier.fillMaxSize().padding(24.dp)) {
        Text("Consumables", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))
        Text("Reminders are measurement-triggered from your hub's usage counters.")
    }
}

@Composable
fun ConsentDashboardScreen(store: ConsentStore, modifier: Modifier = Modifier) {
    var blanket by remember { mutableStateOf(store.researchConsent.blanketConsentGranted) }

    Column(modifier = modifier.fillMaxSize().padding(24.dp)) {
        Text("Privacy & Research", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Text("Blanket research consent")
        Switch(
            checked = blanket,
            onCheckedChange = { on ->
                if (!on) {
                    // Blanket withdrawal also tears down research analytics
                    // (CLAUDE.md §6.0) — handled inside the store.
                    store.withdrawBlanketResearchConsent()
                } else {
                    store.updateResearchConsent(
                        store.researchConsent.copy(blanketConsentGranted = true),
                    )
                }
                blanket = on
            },
        )
        Spacer(Modifier.height(16.dp))
        Text(
            "Once your anonymized data has been included in a published study, it cannot be " +
                "individually withdrawn from that dataset. Withdrawing consent immediately and " +
                "permanently stops any further data flowing to any future dataset — including " +
                "data from sessions that occurred before your withdrawal.",
            style = MaterialTheme.typography.bodySmall,
        )
    }
}

@Composable
fun SettingsScreen(app: NeuroPulseApplication, modifier: Modifier = Modifier) {
    Column(modifier = modifier.fillMaxSize().padding(24.dp)) {
        Text("Settings", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Text("Research analytics")
        Switch(
            checked = app.researchAnalyticsGate.isOpen,
            onCheckedChange = { on ->
                if (!on) {
                    app.consentStore.revokeResearchAnalytics()
                } else {
                    app.keyValueStore.putBoolean(
                        com.neuropulse.core.analytics.ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY,
                        true,
                    )
                    app.researchAnalyticsGate.configure()
                }
            },
        )
    }
}
