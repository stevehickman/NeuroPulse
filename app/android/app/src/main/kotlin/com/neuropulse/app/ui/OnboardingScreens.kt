package com.neuropulse.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.neuropulse.app.R

/**
 * Minimum age gate — parity with iOS AgeGateView.swift (NP-PRIV-001 Rev B
 * MEDIUM-03). Must appear before ANY screen that collects or displays
 * personal data. Checkbox is never pre-ticked; Continue is disabled until
 * checked. OI-PA-01 (legal counsel confirmation of the 16-year threshold)
 * remains open, shared with iOS.
 */
@Composable
fun AgeGateScreen(onConfirmed: () -> Unit) {
    var checked by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier.fillMaxSize().padding(24.dp),
        verticalArrangement = Arrangement.Center,
    ) {
        Text("Welcome to NeuroPulse", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Text(
            "NeuroPulse is designed for users 16 years of age or older. " +
                "Please confirm your age to continue.",
            style = MaterialTheme.typography.bodyLarge,
        )
        Spacer(Modifier.height(24.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Checkbox(checked = checked, onCheckedChange = { checked = it })
            Text(stringResource(R.string.age_gate_confirmation))
        }
        Spacer(Modifier.height(24.dp))
        Button(onClick = onConfirmed, enabled = checked) {
            Text("Continue")
        }
    }
}

/**
 * Biometric data written release — parity with iOS BIPA consent flow
 * (NP-PRIV-001 Rev B HIGH-01). Shown to ALL users regardless of location
 * (OI-PA-03 resolution: locale gate removed). EEG neurofeedback and
 * closed-loop adaptive stimulation stay disabled until accepted.
 */
@Composable
fun BipaConsentScreen(onAccepted: () -> Unit) {
    var checked by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier.fillMaxSize().padding(24.dp).verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.Center,
    ) {
        Text("Your brainwave data", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Text(
            "NeuroPulse records EEG (brainwave) signals during sessions. EEG patterns are " +
                "biometric information. Your recordings are encrypted on your device with a key " +
                "only you hold — NeuroPulse cannot read them, ever.\n\n" +
                "Retention: your recordings stay on your device until you delete them. " +
                "NeuroPulse's biometric data retention and destruction policy is available at " +
                "neuropulse.com/biometric-policy.\n\n" +
                "By continuing you provide written release for NeuroPulse to process EEG data " +
                "on your device for session delivery and neurofeedback.",
            style = MaterialTheme.typography.bodyLarge,
        )
        Spacer(Modifier.height(24.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Checkbox(checked = checked, onCheckedChange = { checked = it })
            Text("I have read and agree to the biometric data release.")
        }
        Spacer(Modifier.height(24.dp))
        Button(onClick = onAccepted, enabled = checked) {
            Text("Agree and continue")
        }
        Spacer(Modifier.height(16.dp))
        Text(
            stringResource(R.string.regulatory_footer),
            style = MaterialTheme.typography.bodySmall,
        )
    }
}
