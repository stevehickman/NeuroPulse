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
        Text(stringResource(R.string.setup_title_welcome), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Text(
            stringResource(R.string.age_neurone_is_designed_for_users_16_years_of_ag) +
                stringResource(R.string.age_please_confirm_your_age_to_continue),
            style = MaterialTheme.typography.bodyLarge,
        )
        Spacer(Modifier.height(24.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Checkbox(checked = checked, onCheckedChange = { checked = it })
            Text(stringResource(R.string.age_gate_declaration))
        }
        Spacer(Modifier.height(24.dp))
        Button(onClick = onConfirmed, enabled = checked) {
            Text(stringResource(R.string.setup_continue_button))
        }
        Spacer(Modifier.height(8.dp))
        TextButton(onClick = { showUnder16 = true }) { Text(stringResource(R.string.age_gate_under_16)) }
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
        Text(stringResource(R.string.age_thanks_for_your_interest), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Text(
            stringResource(R.string.age_neurone_is_intended_for_users_16_years_of_ag) +
                stringResource(R.string.age_up_a_device_for_younger_users_at_this_time_i) +
                stringResource(R.string.age_back_and_confirm_your_age),
            style = MaterialTheme.typography.bodyLarge,
        )
        Spacer(Modifier.height(24.dp))
        TextButton(onClick = onBack) { Text(stringResource(R.string.consent_back_button)) }
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
        Text(stringResource(R.string.age_your_brainwave_data), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        Text(
            stringResource(R.string.age_neurone_records_eeg_brainwave_signals_during) +
                stringResource(R.string.age_biometric_information_your_recordings_are_en) +
                stringResource(R.string.age_only_you_hold_neurone_cannot_read_them_ever) +
                stringResource(R.string.age_retention_your_recordings_stay_on_your_devic) +
                stringResource(R.string.age_neurone_s_biometric_data_retention_and_destr) +
                stringResource(R.string.age_neurone_life_biometric_policy) +
                stringResource(R.string.age_by_continuing_you_provide_written_release_fo) +
                stringResource(R.string.age_on_your_device_for_session_delivery_and_neur),
            style = MaterialTheme.typography.bodyLarge,
        )
        Spacer(Modifier.height(24.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Checkbox(checked = checked, onCheckedChange = { checked = it })
            Text(stringResource(R.string.age_i_have_read_and_agree_to_the_biometric_data))
        }
        Spacer(Modifier.height(24.dp))
        Button(onClick = onAccepted, enabled = checked) {
            Text(stringResource(R.string.age_agree_and_continue))
        }
        Spacer(Modifier.height(16.dp))
        Text(
            stringResource(R.string.regulatory_footer),
            style = MaterialTheme.typography.bodySmall,
        )
    }
}
