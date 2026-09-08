package life.neurone.app.ui

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
import life.neurone.app.NeurOneApplication
import life.neurone.app.R
import life.neurone.core.consent.ConsentStore
import life.neurone.core.session.SessionHistoryStore

// Screen skeletons — structure and privacy wiring in place; visual completion
// tracked as follow-up work in app/android/ISA.md (Out of Scope note).
// Every stimulation-adjacent screen carries the regulatory footer (parity
// with iOS App Store constraint).

// SessionScreen lives in SessionScreen.kt (full renderer of connection + live session
// state). ProtocolMenuScreen lives in ProtocolMenuScreen.kt.

// HistoryScreen lives in HistoryScreen.kt (list + detail + Adaptive Adjustments card).
// ConsumablesScreen lives in ConsumablesScreen.kt (wired to the core ConsumableTracker).

@Composable
fun ConsentDashboardScreen(app: NeurOneApplication, modifier: Modifier = Modifier) {
    val store = app.consentStore
    var blanket by remember { mutableStateOf(store.researchConsent.blanketConsentGranted) }
    var showPortal by remember { mutableStateOf(false) }

    if (showPortal) {
        ResearchPortalScreen(
            store = app.researchSuggestionStore,
            onBack = { showPortal = false },
            modifier = modifier,
        )
        return
    }

    Column(modifier = modifier.fillMaxSize().padding(24.dp)) {
        Text(stringResource(R.string.and_ui_privacy_research), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Text(stringResource(R.string.and_ui_blanket_research_consent))
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
            stringResource(R.string.consent_once_your_anonymized_data_has_been_included) +
                stringResource(R.string.and_ui_individually_withdrawn_from_that_dataset_wit) +
                stringResource(R.string.and_ui_permanently_stops_any_further_data_flowing_t) +
                stringResource(R.string.consent_data_from_sessions_that_occurred_before_your),
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(24.dp))
        Text(stringResource(R.string.portal_research_ideas), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.and_ui_suggest_studies_vote_and_register_interest_i), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = { showPortal = true }) { Text(stringResource(R.string.and_ui_open_research_portal)) }
    }
}

@Composable
fun SettingsScreen(app: NeurOneApplication, modifier: Modifier = Modifier) {
    var showBipa by remember { mutableStateOf(false) }
    var showResearch by remember { mutableStateOf(false) }
    var showOta by remember { mutableStateOf(false) }
    var showSetup by remember { mutableStateOf(false) }
    var showLimits by remember { mutableStateOf(false) }
    var eegGranted by remember {
        mutableStateOf(app.keyValueStore.getBoolean(OnboardingKeys.BIPA_ACCEPTED))
    }
    val firmwareVersion by app.gattManager.hubFirmwareVersion.collectAsState()
    val otaStatus by app.gattManager.otaStatus.collectAsState()

    when {
        showLimits -> LimitsSettingsScreen(
            store = app.limitsStore,
            onDone = { showLimits = false },
            modifier = modifier,
        )
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
            onManageLimits = { showLimits = true },
            modifier = modifier,
        )
    }
}

@Composable
private fun SettingsContent(
    app: NeurOneApplication,
    eegGranted: Boolean,
    onManageEeg: () -> Unit,
    onRevokeEeg: () -> Unit,
    onManageResearch: () -> Unit,
    onManageFirmware: () -> Unit,
    onDeviceSetup: () -> Unit,
    onManageLimits: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier.fillMaxSize().padding(24.dp)) {
        Text(stringResource(R.string.tab_settings), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(20.dp))

        // Biometric (EEG) consent
        Text(stringResource(R.string.and_ui_brainwave_eeg_data_consent), style = MaterialTheme.typography.titleMedium)
        Text(
            if (eegGranted) stringResource(R.string.and_ui_granted_eeg_neurofeedback_is_available)
            else stringResource(R.string.and_ui_not_granted_eeg_neurofeedback_and_closed_loo),
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (!eegGranted) {
                Button(onClick = onManageEeg) { Text(stringResource(R.string.and_ui_review_consent)) }
            } else {
                OutlinedButton(onClick = onRevokeEeg) { Text(stringResource(R.string.dashboard_revoke_button)) }
            }
        }

        Spacer(Modifier.height(24.dp))

        // Research participation
        Text(stringResource(R.string.consent_research_participation), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.and_ui_manage_the_research_areas_and_studies_you_ta), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onManageResearch) { Text(stringResource(R.string.and_ui_manage)) }

        Spacer(Modifier.height(24.dp))

        // Firmware / OTA
        Text(stringResource(R.string.ota_firmware), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.and_ui_view_your_hub_s_firmware_version_and_update), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onManageFirmware) { Text(stringResource(R.string.and_ui_firmware_updates)) }

        Spacer(Modifier.height(24.dp))

        // Device setup wizard
        Text(stringResource(R.string.and_ui_device_setup), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.and_ui_re_run_the_guided_hardware_setup_for_your_he), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onDeviceSetup) { Text(stringResource(R.string.and_ui_set_up_device)) }

        Spacer(Modifier.height(24.dp))

        // Dosage limits
        Text(stringResource(R.string.limits_dosage_limits), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(R.string.and_ui_set_global_caps_on_stimulation_dose_and_inte), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onManageLimits) { Text(stringResource(R.string.and_ui_edit_dosage_limits)) }

        Spacer(Modifier.height(24.dp))

        // Research analytics gate
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(stringResource(R.string.and_ui_research_analytics), Modifier.weight(1f))
            Switch(
                checked = app.researchAnalyticsGate.isOpen,
                onCheckedChange = { on ->
                    if (!on) {
                        app.consentStore.revokeResearchAnalytics()
                    } else {
                        app.keyValueStore.putBoolean(
                            life.neurone.core.analytics.ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY,
                            true,
                        )
                        app.researchAnalyticsGate.configure()
                    }
                },
            )
        }

        Spacer(Modifier.height(24.dp))
        Text(stringResource(R.string.and_ui_neurone_home_0_1_0), style = MaterialTheme.typography.bodySmall)
    }
}
