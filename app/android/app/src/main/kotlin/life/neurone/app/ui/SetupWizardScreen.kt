package life.neurone.app.ui

import android.content.Context
import androidx.annotation.StringRes
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import life.neurone.app.NeurOneApplication
import life.neurone.app.R
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
    app: NeurOneApplication,
    onFinish: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val flow = remember { SetupFlow(app.keyValueStore) }
    var step by remember { mutableStateOf(flow.currentStep) }
    var safetyChecked by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    val connectionState by app.gattManager.connectionState.collectAsState()
    val session by app.gattManager.session.collectAsState()
    val zoneModules by app.gattManager.zoneModules.collectAsState()

    val context = LocalContext.current

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
            stringResource(R.string.setup_device_setup_step_0_of_1, step.index + 1, SetupStep.COMPLETE.index + 1),
            style = MaterialTheme.typography.labelMedium,
        )
        Spacer(Modifier.height(8.dp))
        Text(stringResource(titleFor(step)), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(12.dp))
        Text(stringResource(instructionFor(step)), style = MaterialTheme.typography.bodyLarge)

        error?.let {
            Spacer(Modifier.height(12.dp))
            Text(it, color = Color(0xFFD32F2F), style = MaterialTheme.typography.bodyMedium)
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
                Text(stringResource(R.string.setup_i_have_read_the_contraindications_and_it_is))
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
                }) { Text(stringResource(R.string.consent_back_button)) }
            }
            Spacer(Modifier.weight(1f))
            Button(onClick = {
                error = null
                when {
                    step == SetupStep.COMPLETE -> onFinish()

                    step.requiresHardwareConfirmation -> {
                        if (connectionState != ConnectionState.CONNECTED) {
                            error = context.getString(R.string.setup_connect_to_your_hub_first)
                        } else when (step) {
                            SetupStep.ZONE_MODULES -> when (val r = flow.evaluateZoneModules(zoneModules)) {
                                SetupFlow.ZoneModuleResult.Passed -> advance()
                                SetupFlow.ZoneModuleResult.NoneDetected ->
                                    error = context.getString(R.string.setup_error_no_zone_modules)
                                is SetupFlow.ZoneModuleResult.Faulted -> error = context.getString(
                                    R.string.setup_error_zone_missing,
                                    r.socketIds.joinToString(", ") {
                                        context.getString(R.string.setup_socket_label, it)
                                    },
                                )
                            }
                            SetupStep.IMPEDANCE_CHECK -> {
                                app.gattManager.sendCalibration(CalibrationOpcode.IMPEDANCE_CHECK)
                                val r = flow.evaluateImpedance(session.impedancePassFlags)
                                if (r.passed) advance()
                                else error = context.getString(
                                    R.string.setup_only_0_of_8_electrodes_made_good_contact,
                                    r.passCount,
                                ) + context.getString(R.string.setup_adjust_the_fit_and_try_again)
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
                                error = context.getString(R.string.setup_please_confirm_the_safety_acknowledgement_to)
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
    val context = LocalContext.current
    var previous by remember { mutableStateOf(configuration) }
    var lastConfirmation by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(configuration) {
        configuration.presentSockets
            .lastOrNull { previous.status(it.socketId)?.isPresent != true }
            ?.let { lastConfirmation = spokenConfirmation(context, it) }
        previous = configuration
    }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        if (configuration.isEmpty) {
            Text(
                stringResource(R.string.setup_zone_waiting),
                style = MaterialTheme.typography.bodyMedium,
            )
        } else {
            Text(
                stringResource(
                    R.string.setup_zone_summary,
                    configuration.presentSockets.size,
                    configuration.sockets.size,
                ),
                style = MaterialTheme.typography.labelMedium,
            )
            configuration.orderedSockets.forEach { socket ->
                val spoken = spokenConfirmation(context, socket)
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
                    Text(socketLabel(context, socket.socketId), style = MaterialTheme.typography.bodyMedium)
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
        lastConfirmation?.let {
            Text(
                it,
                style = MaterialTheme.typography.bodySmall,
                modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite },
            )
        }
    }
}

/** Zone name leads when the socket has one; the raw id is the fallback (NP-HFE-002 §7.2). */
private fun socketLabel(context: Context, socketId: Int): String =
    NPZoneRegistry.primaryZone(socketId)
        ?.let { context.getString(R.string.zone_socket_position_label, socketId, it) }
        ?: context.getString(R.string.zone_socket_label, socketId)

private fun spokenConfirmation(context: Context, socket: ZoneModuleStatus): String {
    val place = NPZoneRegistry.primaryZone(socket.socketId)
        ?: context.getString(R.string.zone_socket_label, socket.socketId)
    return moduleTypeName(socket.moduleType)
        ?.let { context.getString(R.string.zone_announce_with_type, place, context.getString(it)) }
        ?: context.getString(R.string.zone_announce_no_type, place)
}

@StringRes
private fun moduleTypeName(type: ZoneModuleType): Int? = when (type) {
    ZoneModuleType.PBM_BASE -> R.string.zone_module_type_pbm_base
    ZoneModuleType.EEG -> R.string.zone_module_type_eeg
    ZoneModuleType.PBM_1064 -> R.string.zone_module_type_pbm_1064
    ZoneModuleType.PBM_1170 -> R.string.zone_module_type_pbm_1170
    ZoneModuleType.ABSENT, ZoneModuleType.UNKNOWN -> null
}

@StringRes
private fun titleFor(step: SetupStep): Int = when (step) {
    SetupStep.WELCOME -> R.string.setup_title_welcome
    SetupStep.BLE_CONFIRMATION -> R.string.setup_title_ble
    SetupStep.BOA_DIAL -> R.string.setup_title_boa_dial
    SetupStep.ELECTRODE_PODS -> R.string.setup_title_electrode_pods
    SetupStep.ZONE_MODULES -> R.string.setup_title_zone_modules
    SetupStep.IMPEDANCE_CHECK -> R.string.setup_title_impedance
    SetupStep.ADS1299_CALIBRATION -> R.string.setup_title_calibration
    SetupStep.HYDRATION_CAPS -> R.string.setup_title_hydration
    SetupStep.SAFETY_ACKNOWLEDGEMENT -> R.string.setup_title_safety
    SetupStep.PROTOCOL_SELECTION -> R.string.setup_title_protocol
    SetupStep.COMPLETE -> R.string.setup_title_complete
}

@StringRes
private fun instructionFor(step: SetupStep): Int = when (step) {
    SetupStep.WELCOME -> R.string.setup_instr_welcome
    SetupStep.BLE_CONFIRMATION -> R.string.setup_instr_ble
    SetupStep.BOA_DIAL -> R.string.setup_instr_boa_dial
    SetupStep.ELECTRODE_PODS -> R.string.setup_instr_electrode_pods
    SetupStep.ZONE_MODULES -> R.string.setup_instr_zone_modules
    SetupStep.IMPEDANCE_CHECK -> R.string.setup_instr_impedance
    SetupStep.ADS1299_CALIBRATION -> R.string.setup_instr_calibration
    SetupStep.HYDRATION_CAPS -> R.string.setup_instr_hydration
    SetupStep.SAFETY_ACKNOWLEDGEMENT -> R.string.setup_instr_safety
    SetupStep.PROTOCOL_SELECTION -> R.string.setup_instr_protocol
    SetupStep.COMPLETE -> R.string.setup_instr_complete
}

@StringRes
private fun primaryLabelFor(step: SetupStep): Int = when (step) {
    SetupStep.BLE_CONFIRMATION -> R.string.setup_confirm_ble_button
    SetupStep.ZONE_MODULES -> R.string.setup_confirm_zone_button
    SetupStep.IMPEDANCE_CHECK -> R.string.setup_check_signal_button
    SetupStep.ADS1299_CALIBRATION -> R.string.setup_calibrate_button
    SetupStep.COMPLETE -> R.string.setup_finish_button
    else -> R.string.setup_continue_button
}
