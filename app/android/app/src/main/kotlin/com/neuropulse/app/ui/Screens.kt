package com.neuropulse.app.ui

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
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
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

// SessionScreen lives in SessionScreen.kt (full renderer of connection + live session
// state). ProtocolMenuScreen lives in ProtocolMenuScreen.kt.

// HistoryScreen lives in HistoryScreen.kt (list + detail + Adaptive Adjustments card).
// ConsumablesScreen lives in ConsumablesScreen.kt (wired to the core ConsumableTracker).

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
    var showBipa by remember { mutableStateOf(false) }
    var showResearch by remember { mutableStateOf(false) }
    var showOta by remember { mutableStateOf(false) }
    var showSetup by remember { mutableStateOf(false) }
    var eegGranted by remember {
        mutableStateOf(app.keyValueStore.getBoolean(OnboardingKeys.BIPA_ACCEPTED))
    }
    val firmwareVersion by app.gattManager.hubFirmwareVersion.collectAsState()
    val otaStatus by app.gattManager.otaStatus.collectAsState()

    when {
        showSetup -> SetupWizardScreen(
            app = app,
            onFinish = { showSetup = false },
            modifier = modifier,
        )
        showOta -> OtaScreen(
            firmwareVersion = firmwareVersion,
            status = otaStatus,
            onBack = { showOta = false },
            modifier = modifier,
        )
        // Re-present the biometric written release (parity with iOS "Manage EEG data consent").
        showBipa -> BipaConsentScreen(onAccepted = {
            app.keyValueStore.putBoolean(OnboardingKeys.BIPA_ACCEPTED, true)
            app.protocolLibrary.updateEEGConsent(true)
            eegGranted = true
            showBipa = false
        })
        // Re-open the L1–L4 research-consent flow.
        showResearch -> ConsentOnboardingScreen(
            store = app.consentStore,
            onComplete = { showResearch = false },
        )
        else -> SettingsContent(
            app = app,
            eegGranted = eegGranted,
            onManageEeg = { showBipa = true },
            onRevokeEeg = {
                app.keyValueStore.putBoolean(OnboardingKeys.BIPA_ACCEPTED, false)
                app.protocolLibrary.updateEEGConsent(false)
                eegGranted = false
            },
            onManageResearch = { showResearch = true },
            onManageFirmware = { showOta = true },
            onDeviceSetup = { showSetup = true },
            modifier = modifier,
        )
    }
}

@Composable
private fun SettingsContent(
    app: NeuroPulseApplication,
    eegGranted: Boolean,
    onManageEeg: () -> Unit,
    onRevokeEeg: () -> Unit,
    onManageResearch: () -> Unit,
    onManageFirmware: () -> Unit,
    onDeviceSetup: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier.fillMaxSize().padding(24.dp)) {
        Text("Settings", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(20.dp))

        // Biometric (EEG) consent
        Text("Brainwave (EEG) data consent", style = MaterialTheme.typography.titleMedium)
        Text(
            if (eegGranted) "Granted — EEG neurofeedback is available."
            else "Not granted — EEG neurofeedback and closed-loop protocols are disabled.",
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (!eegGranted) {
                Button(onClick = onManageEeg) { Text("Review consent") }
            } else {
                OutlinedButton(onClick = onRevokeEeg) { Text("Revoke") }
            }
        }

        Spacer(Modifier.height(24.dp))

        // Research participation
        Text("Research participation", style = MaterialTheme.typography.titleMedium)
        Text("Manage the research areas and studies you take part in.", style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onManageResearch) { Text("Manage") }

        Spacer(Modifier.height(24.dp))

        // Firmware / OTA
        Text("Firmware", style = MaterialTheme.typography.titleMedium)
        Text("View your hub's firmware version and update status.", style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onManageFirmware) { Text("Firmware & updates") }

        Spacer(Modifier.height(24.dp))

        // Device setup wizard
        Text("Device setup", style = MaterialTheme.typography.titleMedium)
        Text("Re-run the guided hardware setup for your headset.", style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onDeviceSetup) { Text("Set up device") }

        Spacer(Modifier.height(24.dp))

        // Research analytics gate
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text("Research analytics", Modifier.weight(1f))
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

        Spacer(Modifier.height(24.dp))
        Text("NeuroPulse Home 0.1.0", style = MaterialTheme.typography.bodySmall)
    }
}
