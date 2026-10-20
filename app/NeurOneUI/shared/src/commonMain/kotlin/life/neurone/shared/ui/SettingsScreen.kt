package life.neurone.shared.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import org.jetbrains.compose.resources.stringResource
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.shared.AppServices
import life.neurone.shared.migration.EraseDeviceModel
import life.neurone.shared.migration.EraseStep
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.*
import life.neurone.core.protocol.PersistedKeys

// Screen skeletons — structure and privacy wiring in place; visual completion
// tracked as follow-up work in app/android/ISA.md (Out of Scope note).
// Every stimulation-adjacent screen carries the regulatory footer (parity
// with iOS App Store constraint).

// SessionScreen lives in SessionScreen.kt (full renderer of connection + live session
// state). ProtocolMenuScreen lives in ProtocolMenuScreen.kt.

// HistoryScreen lives in HistoryScreen.kt (list + detail + Adaptive Adjustments card).
// ConsumablesScreen lives in ConsumablesScreen.kt (wired to the core ConsumableTracker).
// ConsentDashboardScreen lives in ConsentDashboardScreen.kt (the CLAUDE.md §6 consent
// surface: clinician grants, research consent, study audit trail, invitations).

@Composable
fun SettingsScreen(
    app: AppServices,
    modifier: Modifier = Modifier,
    /** True until hardware setup has finished once: the wizard opens at once and Session is blocked. */
    setupRequired: Boolean = false,
    onSetupFinished: () -> Unit = {},
) {
    var showBipa by remember { mutableStateOf(false) }
    var showResearch by remember { mutableStateOf(false) }
    var showOta by remember { mutableStateOf(false) }
    var showSetup by remember { mutableStateOf(setupRequired) }
    var showLimits by remember { mutableStateOf(false) }
    var showProfiles by remember { mutableStateOf(false) }
    var showOwner by remember { mutableStateOf(false) }
    var showMigration by remember { mutableStateOf(false) }
    var showRestore by remember { mutableStateOf(false) }
    var showClear by remember { mutableStateOf(false) }
    var showLinkSend by remember { mutableStateOf(false) }
    var showLinkReceive by remember { mutableStateOf(false) }
    var eegGranted by remember {
        mutableStateOf(app.keyValueStore.getBoolean(OnboardingKeys.BIPA_ACCEPTED))
    }
    val firmwareVersion by app.gattManager.hubFirmwareVersion.collectAsState()
    val otaStatus by app.gattManager.otaStatus.collectAsState()

    when {
        showLinkSend -> MigrationLinkSendScreen(app = app, onDone = { showLinkSend = false }, modifier = modifier)
        showLinkReceive -> MigrationLinkReceiveScreen(app = app, onDone = { showLinkReceive = false }, modifier = modifier)
        showRestore -> MigrationRestoreScreen(
            app = app,
            onDone = { showRestore = false },
            modifier = modifier,
        )
        showClear -> MigrationClearScreen(
            app = app,
            onDone = { showClear = false },
            modifier = modifier,
        )
        showMigration -> MigrationBackupScreen(
            app = app,
            onDone = { showMigration = false },
            modifier = modifier,
        )
        showOwner -> WarrantyOwnerScreen(
            app = app,
            onDone = { showOwner = false },
            modifier = modifier,
        )
        showProfiles -> ProfilesScreen(
            store = app.limitsStore,
            onDone = { showProfiles = false },
            modifier = modifier,
        )
        showLimits -> LimitsSettingsScreen(
            store = app.limitsStore,
            onDone = { showLimits = false },
            modifier = modifier,
        )
        showSetup -> SetupWizardScreen(
            app = app,
            onFinish = {
                showSetup = false
                onSetupFinished()
            },
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
            onManageProfiles = { showProfiles = true },
            onManageOwner = { showOwner = true },
            onManageMigration = { showMigration = true },
            onRestore = { showRestore = true },
            onClear = { showClear = true },
            onLinkSend = { showLinkSend = true },
            onLinkReceive = { showLinkReceive = true },
            modifier = modifier,
        )
    }
}

@Composable
private fun SettingsContent(
    app: AppServices,
    eegGranted: Boolean,
    onManageEeg: () -> Unit,
    onRevokeEeg: () -> Unit,
    onManageResearch: () -> Unit,
    onManageFirmware: () -> Unit,
    onDeviceSetup: () -> Unit,
    onManageLimits: () -> Unit,
    onManageProfiles: () -> Unit,
    onManageOwner: () -> Unit,
    onManageMigration: () -> Unit,
    onRestore: () -> Unit,
    onClear: () -> Unit,
    onLinkSend: () -> Unit,
    onLinkReceive: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp)) {
        Text(stringResource(Res.string.tab_settings), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(20.dp))

        // Biometric (EEG) consent
        Text(stringResource(Res.string.and_ui_brainwave_eeg_data_consent), style = MaterialTheme.typography.titleMedium)
        Text(
            if (eegGranted) stringResource(Res.string.and_ui_granted_eeg_neurofeedback_is_available)
            else stringResource(Res.string.and_ui_not_granted_eeg_neurofeedback_and_closed_loo),
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (!eegGranted) {
                Button(onClick = onManageEeg) { Text(stringResource(Res.string.and_ui_review_consent)) }
            } else {
                OutlinedButton(onClick = onRevokeEeg) { Text(stringResource(Res.string.dashboard_revoke_button)) }
            }
        }

        Spacer(Modifier.height(24.dp))

        // Research participation
        Text(stringResource(Res.string.consent_research_participation), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(Res.string.and_ui_manage_the_research_areas_and_studies_you_ta), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onManageResearch) { Text(stringResource(Res.string.and_ui_manage)) }

        Spacer(Modifier.height(24.dp))

        // Firmware / OTA
        Text(stringResource(Res.string.ota_firmware), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(Res.string.and_ui_view_your_hub_s_firmware_version_and_update), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onManageFirmware) { Text(stringResource(Res.string.and_ui_firmware_updates)) }

        Spacer(Modifier.height(24.dp))

        // Device setup wizard
        Text(stringResource(Res.string.and_ui_device_setup), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(Res.string.and_ui_re_run_the_guided_hardware_setup_for_your_he), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onDeviceSetup) { Text(stringResource(Res.string.and_ui_set_up_device)) }

        Spacer(Modifier.height(24.dp))

        // Who is using the device — the active individual profile (per-user cardiac scope)
        Text(stringResource(Res.string.profile_picker_title), style = MaterialTheme.typography.titleMedium)
        Text(
            app.limitsStore.activeProfile?.name ?: stringResource(Res.string.profile_picker_none_selected),
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onManageProfiles) { Text(stringResource(Res.string.limits_profiles)) }

        Spacer(Modifier.height(24.dp))

        // Dosage limits
        Text(stringResource(Res.string.limits_dosage_limits), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(Res.string.and_ui_set_global_caps_on_stimulation_dose_and_inte), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onManageLimits) { Text(stringResource(Res.string.and_ui_edit_dosage_limits)) }

        Spacer(Modifier.height(24.dp))

        // The helmet owner's settings (SWR-WO-01, SWR-SS-05): what is offered, and the pause limit.
        Text(stringResource(Res.string.settings_warranty_owner_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(Res.string.settings_warranty_owner_body), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onManageOwner) { Text(stringResource(Res.string.settings_warranty_owner_button)) }

        Spacer(Modifier.height(24.dp))

        // Back up, or move to a new phone (SWR-MG-02, SWR-MG-03).
        OutlinedButton(onClick = onManageMigration) { Text(stringResource(Res.string.settings_migration_button)) }
        OutlinedButton(onClick = onRestore) { Text(stringResource(Res.string.settings_restore_button)) }
        OutlinedButton(onClick = onClear) { Text(stringResource(Res.string.settings_clear_button)) }
        // A direct move between two phones on the same Wi-Fi, when this host can reach one (SWR-MG-10).
        if (app.platform.linkTransport != null) {
            OutlinedButton(onClick = onLinkSend) { Text(stringResource(Res.string.settings_link_send_button)) }
            OutlinedButton(onClick = onLinkReceive) { Text(stringResource(Res.string.settings_link_receive_button)) }
        }

        // Erase what this host keeps, where it offers that (the browser does): the person's own way to empty it.
        app.platform.eraseLocalData?.let { erase ->
            Spacer(Modifier.height(24.dp))
            EraseLocalDataSection(erase)
        }

        // Erase this computer's copy of everything the app keeps, where the host can end the run afterwards (SWR-MG-17).
        app.eraseDevice?.let { model ->
            app.platform.endRunAfterErase?.let { endRun ->
                Spacer(Modifier.height(24.dp))
                EraseDeviceSection(model, endRun)
            }
        }

        Spacer(Modifier.height(24.dp))

        // Research analytics gate
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Text(stringResource(Res.string.and_ui_research_analytics), Modifier.weight(1f))
            Switch(
                checked = app.researchAnalyticsGate.isOpen,
                onCheckedChange = { on ->
                    if (!on) {
                        app.consentStore.revokeResearchAnalytics()
                    } else {
                        app.keyValueStore.putBoolean(
                            life.neurone.core.protocol.PersistedKeys.RESEARCH_ANALYTICS_KEY,
                            true,
                        )
                        app.researchAnalyticsGate.configure()
                    }
                },
            )
        }

        Spacer(Modifier.height(24.dp))
        Text(stringResource(Res.string.and_ui_neurone_home_0_1_0), style = MaterialTheme.typography.bodySmall)
    }
}

/**
 * Erase everything this host keeps and start afresh. It cannot be undone, so the button stays disabled until the person has said
 * they understand that; the host does its own restart once [erase] returns.
 */
@Composable
internal fun EraseLocalDataSection(erase: () -> Unit) {
    var understood by remember { mutableStateOf(false) }
    Text(stringResource(Res.string.settings_erase_local_title), style = MaterialTheme.typography.titleMedium)
    Text(stringResource(Res.string.settings_erase_local_body), style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))
    Row(verticalAlignment = Alignment.CenterVertically) {
        Checkbox(checked = understood, onCheckedChange = { understood = it })
        Text(stringResource(Res.string.settings_erase_local_confirm))
    }
    Spacer(Modifier.height(8.dp))
    OutlinedButton(enabled = understood, onClick = erase) { Text(stringResource(Res.string.settings_erase_local_button)) }
}

/**
 * Erase this device's copy of everything the app keeps (SWR-MG-17). It cannot be undone, so the button stays disabled until the
 * person has said they understand that. When it has finished the person closes the app, which the host does ([endRun]): the run
 * still holds the old state in memory and must not carry on and write it back.
 */
@Composable
internal fun EraseDeviceSection(model: EraseDeviceModel, endRun: () -> Unit) {
    var understood by remember { mutableStateOf(false) }
    val step by model.step.collectAsState()
    val scope = rememberCoroutineScope()
    Text(stringResource(Res.string.settings_erase_device_title), style = MaterialTheme.typography.titleMedium)
    Text(stringResource(Res.string.settings_erase_device_body), style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))
    when (val s = step) {
        is EraseStep.Done -> {
            Text(stringResource(Res.string.settings_erase_device_done))
            if (s.keyKept) Text(stringResource(Res.string.settings_erase_device_key_kept), style = MaterialTheme.typography.bodySmall)
            Spacer(Modifier.height(8.dp))
            Button(onClick = endRun) { Text(stringResource(Res.string.settings_erase_device_close)) }
        }
        else -> {
            if (s is EraseStep.Failed) Text(stringResource(Res.string.settings_erase_device_failed), color = MaterialTheme.colorScheme.error)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(checked = understood, onCheckedChange = { understood = it })
                Text(stringResource(Res.string.settings_erase_device_confirm))
            }
            Spacer(Modifier.height(8.dp))
            OutlinedButton(
                enabled = understood && s !is EraseStep.Running,
                onClick = { scope.launch { model.erase() } },
            ) { Text(stringResource(Res.string.settings_erase_device_button)) }
        }
    }
}
