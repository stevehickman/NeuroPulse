package life.neurone.shared.ui

import org.jetbrains.compose.resources.StringResource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import org.jetbrains.compose.resources.stringResource
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import life.neurone.shared.AppServices
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.*
import life.neurone.shared.ble.ConnectionState
import life.neurone.core.ble.CalibrationOpcode
import life.neurone.core.models.ZoneModuleConfiguration
import life.neurone.core.models.ZoneModuleStatus
import life.neurone.core.models.ZoneModuleType
import life.neurone.core.protocol.NPZoneRegistry
import life.neurone.core.setup.SetupFlow
import life.neurone.core.setup.SetupStep

// Port of iOS SetupView / HardwareSetupManager wizard. Drives the tested core SetupFlow
// state machine (BLE → fit → pods → zone modules → impedance → ADS1299 cal → hydration →
// safety ack → protocol → complete). Hardware-confirmation steps require a connected hub;
// the safety step cannot be bypassed. Impedance/ADS1299 steps send calibration commands.
// Titles and instructions share the iOS keys (SETUP_TITLE_* / SETUP_INSTR_*).

@Composable
fun SetupWizardScreen(
    app: AppServices,
    onFinish: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val flow = remember { SetupFlow(app.keyValueStore) }
    var step by remember { mutableStateOf(flow.currentStep) }
    var safetyChecked by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<SetupError?>(null) }
    val connectionState by app.gattManager.connectionState.collectAsState()
    val session by app.gattManager.session.collectAsState()
    val zoneModules by app.gattManager.zoneModules.collectAsState()

    fun advance() {
        flow.advance()
        safetyChecked = false
        error = null
        step = flow.currentStep
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
    ) {
        Text(
            stringResource(Res.string.setup_device_setup_step_0_of_1, step.index + 1, SetupStep.COMPLETE.index + 1),
            style = MaterialTheme.typography.labelMedium,
        )
        Spacer(Modifier.height(8.dp))
        Text(stringResource(titleFor(step)), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(12.dp))
        Text(stringResource(instructionFor(step)), style = MaterialTheme.typography.bodyLarge)

        error?.let {
            Spacer(Modifier.height(12.dp))
            Text(it.text(), color = Color(0xFFD32F2F), style = MaterialTheme.typography.bodyMedium)
        }

        if (step == SetupStep.ZONE_MODULES) {
            Spacer(Modifier.height(16.dp))
            ZoneModuleStatusList(zoneModules)
        }

        if (step == SetupStep.SAFETY_ACKNOWLEDGEMENT) {
            Spacer(Modifier.height(16.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(
                    checked = safetyChecked,
                    onCheckedChange = {
                        safetyChecked = it
                        if (it) flow.acknowledgeSafety()
                    },
                )
                Text(stringResource(Res.string.setup_i_have_read_the_contraindications_and_it_is))
            }
        }

        Spacer(Modifier.height(24.dp))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            if (step.index > 0 && step != SetupStep.COMPLETE) {
                OutlinedButton(onClick = {
                    flow.back()
                    safetyChecked = false
                    error = null
                    step = flow.currentStep
                }) { Text(stringResource(Res.string.consent_back_button)) }
            }
            Spacer(Modifier.weight(1f))
            Button(onClick = {
                error = null
                when {
                    step == SetupStep.COMPLETE -> onFinish()

                    step.requiresHardwareConfirmation -> {
                        if (connectionState != ConnectionState.CONNECTED) {
                            error = SetupError.ConnectFirst
                        } else when (step) {
                            SetupStep.ZONE_MODULES -> when (val r = flow.evaluateZoneModules(zoneModules)) {
                                SetupFlow.ZoneModuleResult.Passed -> advance()
                                SetupFlow.ZoneModuleResult.NoneDetected ->
                                    error = SetupError.NoZoneModules
                                is SetupFlow.ZoneModuleResult.Faulted -> error = SetupError.ZoneMissing(r.socketIds)
                            }
                            SetupStep.IMPEDANCE_CHECK -> {
                                app.gattManager.sendCalibration(CalibrationOpcode.IMPEDANCE_CHECK)
                                val r = flow.evaluateImpedance(session.impedancePassFlags)
                                if (r.passed) advance()
                                else error = SetupError.WeakContact(r.passCount)
                            }
                            SetupStep.ADS1299_CALIBRATION -> {
                                app.gattManager.sendCalibration(CalibrationOpcode.ADS1299_SELF_CAL)
                                advance()
                            }
                            else -> advance() // BLE confirmed by connection
                        }
                    }

                    step == SetupStep.SAFETY_ACKNOWLEDGEMENT -> {
                        when (flow.advance()) {
                            is SetupFlow.AdvanceResult.BlockedBySafety ->
                                error = SetupError.SafetyUnconfirmed
                            else -> {
                                safetyChecked = false
                                step = flow.currentStep
                            }
                        }
                    }

                    else -> advance()
                }
            }) { Text(stringResource(primaryLabelFor(step))) }
        }
    }
}

/**
 * Live socket list for the zone-module step. Port of iOS `ZoneModuleStatusGrid`: renders the
 * sockets the hub reports, not a fixed five-slot grid (NP-HEX-ZM-001 §3.4).
 *
 * Insertion confirmation goes through the platform screen reader, as on iOS
 * (`ZoneModuleAnnouncer`, NP-HFE-002 §7.2). No in-app TTS: TalkBack keeps the user's rate,
 * voice and braille routing. A polite live region carries the latest seated socket, zone
 * first, and TalkBack reads it when it changes.
 */
@Composable
private fun ZoneModuleStatusList(configuration: ZoneModuleConfiguration) {
    var previous by remember { mutableStateOf(configuration) }
    var lastSeated by remember { mutableStateOf<ZoneModuleStatus?>(null) }

    LaunchedEffect(configuration) {
        configuration.presentSockets
            .lastOrNull { previous.status(it.socketId)?.isPresent != true }
            ?.let { lastSeated = it }
        previous = configuration
    }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        if (configuration.isEmpty) {
            Text(
                stringResource(Res.string.setup_zone_waiting),
                style = MaterialTheme.typography.bodyMedium,
            )
        } else {
            Text(
                stringResource(
                    Res.string.setup_zone_summary,
                    configuration.presentSockets.size,
                    configuration.sockets.size,
                ),
                style = MaterialTheme.typography.labelMedium,
            )
            configuration.orderedSockets.forEach { socket ->
                val spoken = spokenConfirmation(socket)
                Row(
                    Modifier
                        .fillMaxWidth()
                        .clearAndSetSemantics { contentDescription = spoken },
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        text = when {
                            socket.hasFault -> "⚠"
                            socket.isPresent -> "✓"
                            else -> "○"
                        },
                        color = when {
                            socket.hasFault -> Color(0xFFF57C00)
                            socket.isPresent -> Color(0xFF388E3C)
                            else -> MaterialTheme.colorScheme.onSurfaceVariant
                        },
                    )
                    Spacer(Modifier.width(8.dp))
                    Text(socketLabel(socket.socketId), style = MaterialTheme.typography.bodyMedium)
                    Spacer(Modifier.weight(1f))
                    moduleTypeName(socket.moduleType)?.takeIf { socket.isPresent }?.let {
                        Text(
                            stringResource(it),
                            style = MaterialTheme.typography.labelMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }
        }
        lastSeated?.let { seated ->
            Text(
                spokenConfirmation(seated),
                style = MaterialTheme.typography.bodySmall,
                modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite },
            )
        }
    }
}

/** Zone name leads when the socket has one; the raw id is the fallback (NP-HFE-002 §7.2). */
@Composable
private fun socketLabel(socketId: Int): String =
    NPZoneRegistry.primaryZone(socketId)
        ?.let { stringResource(Res.string.zone_socket_position_label, socketId, it) }
        ?: stringResource(Res.string.zone_socket_label, socketId)

@Composable
private fun spokenConfirmation(socket: ZoneModuleStatus): String {
    val place = NPZoneRegistry.primaryZone(socket.socketId)
        ?: stringResource(Res.string.zone_socket_label, socket.socketId)
    val type = moduleTypeName(socket.moduleType)?.let { stringResource(it) }
    return if (type != null) stringResource(Res.string.zone_announce_with_type, place, type)
    else stringResource(Res.string.zone_announce_no_type, place)
}

/** Why a step did not advance. Data, not text, because the words are read in composition (CLAUDE.md §17). */
private sealed interface SetupError {
    data object ConnectFirst : SetupError
    data object NoZoneModules : SetupError
    data class ZoneMissing(val socketIds: List<Int>) : SetupError
    data class WeakContact(val passCount: Int) : SetupError
    data object SafetyUnconfirmed : SetupError
}

@Composable
private fun SetupError.text(): String = when (this) {
    SetupError.ConnectFirst -> stringResource(Res.string.setup_connect_to_your_hub_first)
    SetupError.NoZoneModules -> stringResource(Res.string.setup_error_no_zone_modules)
    is SetupError.ZoneMissing -> stringResource(
        Res.string.setup_error_zone_missing,
        // `map` is inline, so stringResource may be called inside it; joinToString's transform is not.
        socketIds.map { stringResource(Res.string.setup_socket_label, it) }.joinToString(", "),
    )
    is SetupError.WeakContact ->
        stringResource(Res.string.setup_only_0_of_8_electrodes_made_good_contact, passCount) +
            stringResource(Res.string.setup_adjust_the_fit_and_try_again)
    SetupError.SafetyUnconfirmed -> stringResource(Res.string.setup_please_confirm_the_safety_acknowledgement_to)
}

private fun moduleTypeName(type: ZoneModuleType): StringResource? = when (type) {
    ZoneModuleType.PBM_BASE -> Res.string.zone_module_type_pbm_base
    ZoneModuleType.EEG -> Res.string.zone_module_type_eeg
    ZoneModuleType.PBM_1064 -> Res.string.zone_module_type_pbm_1064
    ZoneModuleType.PBM_1170 -> Res.string.zone_module_type_pbm_1170
    ZoneModuleType.ABSENT, ZoneModuleType.UNKNOWN -> null
}

private fun titleFor(step: SetupStep): StringResource = when (step) {
    SetupStep.WELCOME -> Res.string.setup_title_welcome
    SetupStep.BLE_CONFIRMATION -> Res.string.setup_title_ble
    SetupStep.BOA_DIAL -> Res.string.setup_title_boa_dial
    SetupStep.ELECTRODE_PODS -> Res.string.setup_title_electrode_pods
    SetupStep.ZONE_MODULES -> Res.string.setup_title_zone_modules
    SetupStep.IMPEDANCE_CHECK -> Res.string.setup_title_impedance
    SetupStep.ADS1299_CALIBRATION -> Res.string.setup_title_calibration
    SetupStep.HYDRATION_CAPS -> Res.string.setup_title_hydration
    SetupStep.SAFETY_ACKNOWLEDGEMENT -> Res.string.setup_title_safety
    SetupStep.PROTOCOL_SELECTION -> Res.string.setup_title_protocol
    SetupStep.COMPLETE -> Res.string.setup_title_complete
}

private fun instructionFor(step: SetupStep): StringResource = when (step) {
    SetupStep.WELCOME -> Res.string.setup_instr_welcome
    SetupStep.BLE_CONFIRMATION -> Res.string.setup_instr_ble
    SetupStep.BOA_DIAL -> Res.string.setup_instr_boa_dial
    SetupStep.ELECTRODE_PODS -> Res.string.setup_instr_electrode_pods
    SetupStep.ZONE_MODULES -> Res.string.setup_instr_zone_modules
    SetupStep.IMPEDANCE_CHECK -> Res.string.setup_instr_impedance
    SetupStep.ADS1299_CALIBRATION -> Res.string.setup_instr_calibration
    SetupStep.HYDRATION_CAPS -> Res.string.setup_instr_hydration
    SetupStep.SAFETY_ACKNOWLEDGEMENT -> Res.string.setup_instr_safety
    SetupStep.PROTOCOL_SELECTION -> Res.string.setup_instr_protocol
    SetupStep.COMPLETE -> Res.string.setup_instr_complete
}

private fun primaryLabelFor(step: SetupStep): StringResource = when (step) {
    SetupStep.BLE_CONFIRMATION -> Res.string.setup_confirm_ble_button
    SetupStep.ZONE_MODULES -> Res.string.setup_confirm_zone_button
    SetupStep.IMPEDANCE_CHECK -> Res.string.setup_check_signal_button
    SetupStep.ADS1299_CALIBRATION -> Res.string.setup_calibrate_button
    SetupStep.COMPLETE -> Res.string.setup_finish_button
    else -> Res.string.setup_continue_button
}
