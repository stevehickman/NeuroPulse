package life.neurone.app.ui

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
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import life.neurone.app.R

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
    var showUnder16 by remember { mutableStateOf(false) }

    if (showUnder16) {
        Under16Screen(onBack = { showUnder16 = false })
        return
    }

    Column(
        modifier = Modifier.fillMaxSize().padding(24.dp),
        verticalArrangement = Arrangement.Center,
    ) {
        Text("Welcome to NeurOne", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Text(
            "NeurOne is designed for users 16 years of age or older. " +
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
        Spacer(Modifier.height(8.dp))
        TextButton(onClick = { showUnder16 = true }) { Text("I am under 16") }
    }
}

/**
 * Shown when a user indicates they are under 16 — parity with iOS Under16View. NeurOne
 * onboarding does not proceed for under-16 users; the age threshold (OI-PA-01) is pending
 * legal confirmation, shared with iOS.
 */
@Composable
fun Under16Screen(onBack: () -> Unit) {
    Column(
        modifier = Modifier.fillMaxSize().padding(24.dp),
        verticalArrangement = Arrangement.Center,
    ) {
        Text("Thanks for your interest", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Text(
            "NeurOne is intended for users 16 years of age or older. We're not able to set " +
                "up a device for younger users at this time. If you entered this by mistake, go " +
                "back and confirm your age.",
            style = MaterialTheme.typography.bodyLarge,
        )
        Spacer(Modifier.height(24.dp))
        TextButton(onClick = onBack) { Text("Back") }
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
            "NeurOne records EEG (brainwave) signals during sessions. EEG patterns are " +
                "biometric information. Your recordings are encrypted on your device with a key " +
                "only you hold — NeurOne cannot read them, ever.\n\n" +
                "Retention: your recordings stay on your device until you delete them. " +
                "NeurOne's biometric data retention and destruction policy is available at " +
                "neurone.life/biometric-policy.\n\n" +
                "By continuing you provide written release for NeurOne to process EEG data " +
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
