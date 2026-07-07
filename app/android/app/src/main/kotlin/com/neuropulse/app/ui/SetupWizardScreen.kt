package com.neuropulse.app.ui

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
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.neuropulse.app.NeuroPulseApplication
import com.neuropulse.app.ble.ConnectionState
import com.neuropulse.core.ble.CalibrationOpcode
import com.neuropulse.core.setup.SetupFlow
import com.neuropulse.core.setup.SetupStep

// Port of iOS SetupView / HardwareSetupManager wizard. Drives the tested core SetupFlow
// state machine (BLE → fit → pods → zone modules → impedance → ADS1299 cal → hydration →
// safety ack → protocol → complete). Hardware-confirmation steps require a connected hub;
// the safety step cannot be bypassed. Impedance/ADS1299 steps send calibration commands.

@Composable
fun SetupWizardScreen(
    app: NeuroPulseApplication,
    onFinish: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val flow = remember { SetupFlow(app.keyValueStore) }
    var step by remember { mutableStateOf(flow.currentStep) }
    var safetyChecked by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    val connectionState by app.gattManager.connectionState.collectAsState()
    val session by app.gattManager.session.collectAsState()

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
            "Device setup — step ${step.index + 1} of ${SetupStep.COMPLETE.index + 1}",
            style = MaterialTheme.typography.labelMedium,
        )
        Spacer(Modifier.height(8.dp))
        Text(titleFor(step), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(12.dp))
        Text(instructionFor(step), style = MaterialTheme.typography.bodyLarge)

        error?.let {
            Spacer(Modifier.height(12.dp))
            Text(it, color = Color(0xFFD32F2F), style = MaterialTheme.typography.bodyMedium)
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
                Text("I have read the contraindications and it is safe for me to use NeuroPulse.")
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
                }) { Text("Back") }
            }
            Spacer(Modifier.weight(1f))
            Button(onClick = {
                error = null
                when {
                    step == SetupStep.COMPLETE -> onFinish()

                    step.requiresHardwareConfirmation -> {
                        if (connectionState != ConnectionState.CONNECTED) {
                            error = "Connect to your hub first."
                        } else when (step) {
                            SetupStep.IMPEDANCE_CHECK -> {
                                app.gattManager.sendCalibration(CalibrationOpcode.IMPEDANCE_CHECK)
                                val r = flow.evaluateImpedance(session.impedancePassFlags)
                                if (r.passed) advance()
                                else error = "Only ${r.passCount} of 8 electrodes made good contact. " +
                                    "Adjust the fit and try again."
                            }
                            SetupStep.ADS1299_CALIBRATION -> {
                                app.gattManager.sendCalibration(CalibrationOpcode.ADS1299_SELF_CAL)
                                advance()
                            }
                            else -> advance() // BLE / zone modules confirmed by connection
                        }
                    }

                    step == SetupStep.SAFETY_ACKNOWLEDGEMENT -> {
                        when (flow.advance()) {
                            is SetupFlow.AdvanceResult.BlockedBySafety ->
                                error = "Please confirm the safety acknowledgement to continue."
                            else -> {
                                safetyChecked = false
                                step = flow.currentStep
                            }
                        }
                    }

                    else -> advance()
                }
            }) { Text(primaryLabelFor(step)) }
        }
    }
}

private fun titleFor(step: SetupStep): String = when (step) {
    SetupStep.WELCOME -> "Welcome"
    SetupStep.BLE_CONFIRMATION -> "Connect your hub"
    SetupStep.BOA_DIAL -> "Adjust the fit"
    SetupStep.ELECTRODE_PODS -> "Seat the electrode pods"
    SetupStep.ZONE_MODULES -> "Insert the zone modules"
    SetupStep.IMPEDANCE_CHECK -> "Check electrode contact"
    SetupStep.ADS1299_CALIBRATION -> "Calibrate the amplifier"
    SetupStep.HYDRATION_CAPS -> "Remove the hydration caps"
    SetupStep.SAFETY_ACKNOWLEDGEMENT -> "Safety check"
    SetupStep.PROTOCOL_SELECTION -> "Choose your first protocol"
    SetupStep.COMPLETE -> "You're all set"
}

private fun instructionFor(step: SetupStep): String = when (step) {
    SetupStep.WELCOME -> "Let's get your NeuroPulse ready. This takes about five minutes."
    SetupStep.BLE_CONFIRMATION -> "Power on your hub and connect it over USB-C or Bluetooth."
    SetupStep.BOA_DIAL -> "Turn the occipital Boa dial until the headset is snug but comfortable (fits 52–62 cm)."
    SetupStep.ELECTRODE_PODS -> "Press each spring-decoupled pod against your scalp until it sits flush."
    SetupStep.ZONE_MODULES -> "Snap each zone module into its slot — you'll hear a confirmation tone for each."
    SetupStep.IMPEDANCE_CHECK -> "We'll measure electrode contact. At least 6 of 8 electrodes must make good contact."
    SetupStep.ADS1299_CALIBRATION -> "Running the ADS1299 internal-reference self-calibration."
    SetupStep.HYDRATION_CAPS -> "Peel the moisture-barrier caps off the electrode tips before your first session."
    SetupStep.SAFETY_ACKNOWLEDGEMENT -> "Please confirm you have read the contraindications for NeuroPulse Home."
    SetupStep.PROTOCOL_SELECTION -> "Pick your first session from the Protocols list — you can change it any time."
    SetupStep.COMPLETE -> "Setup is complete. Enjoy your first session."
}

private fun primaryLabelFor(step: SetupStep): String = when (step) {
    SetupStep.WELCOME -> "Begin"
    SetupStep.BLE_CONFIRMATION, SetupStep.ZONE_MODULES -> "Confirm"
    SetupStep.IMPEDANCE_CHECK -> "Run check"
    SetupStep.ADS1299_CALIBRATION -> "Calibrate"
    SetupStep.SAFETY_ACKNOWLEDGEMENT -> "Continue"
    SetupStep.PROTOCOL_SELECTION -> "Continue"
    SetupStep.COMPLETE -> "Finish"
    else -> "Next"
}
