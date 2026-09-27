package life.neurone.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import life.neurone.core.models.OtaPhase
import life.neurone.core.models.OtaStatusPacket
import androidx.annotation.StringRes
import androidx.compose.ui.res.stringResource
import life.neurone.app.R

// Port of iOS OTAView — firmware version + over-the-air update status. Pure renderer of the
// hub's FIRMWARE_VERSION + OTA_STATUS notifications. The actual download+flash flow (host-side
// FirmwareUpdateService) is a follow-up (OI-AND-OTA-01); this shows the live hub status.

@Composable
fun OtaScreen(
    firmwareVersion: String?,
    status: OtaStatusPacket?,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(modifier.fillMaxSize()) {
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onBack) { Text(stringResource(R.string.consent_back_button)) }
            Text(stringResource(R.string.ota_firmware), style = MaterialTheme.typography.titleLarge)
        }

        Column(Modifier.padding(24.dp)) {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text(stringResource(R.string.ota_hub_firmware_version), style = MaterialTheme.typography.labelMedium)
                    Text(firmwareVersion ?: stringResource(R.string.ota_unknown_connect_to_your_hub), style = MaterialTheme.typography.titleMedium)
                }
            }

            Spacer(Modifier.height(16.dp))

            if (status != null) {
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(16.dp)) {
                        Text(stringResource(R.string.ota_update_status), style = MaterialTheme.typography.labelMedium)
                        Text(stringResource(phaseLabel(status.phase)), style = MaterialTheme.typography.titleMedium)
                        if (status.phase?.isBusy == true) {
                            Spacer(Modifier.height(8.dp))
                            LinearProgressIndicator(
                                progress = { (status.progressPercent.coerceIn(0, 100)) / 100f },
                                modifier = Modifier.fillMaxWidth(),
                            )
                            Text("${status.progressPercent}%", style = MaterialTheme.typography.bodySmall)
                        }
                        if (status.isError) {
                            Spacer(Modifier.height(8.dp))
                            Text(stringResource(R.string.ota_error_code_0, status.errorCode), style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            } else {
                Text(
                    stringResource(R.string.ota_firmware_updates_install_automatically_over) +
                        stringResource(R.string.ota_version_is_available_no_action_is_needed),
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }
    }
}

@StringRes
private fun phaseLabel(phase: OtaPhase?): Int = when (phase) {
    OtaPhase.IDLE, null -> R.string.ota_up_to_date
    OtaPhase.RECEIVING -> R.string.ota_phase_transferring
    OtaPhase.VERIFYING -> R.string.ota_phase_verifying
    OtaPhase.COMMITTING -> R.string.ota_phase_applying
    OtaPhase.VERIFIED -> R.string.ota_phase_verified
    OtaPhase.REBOOTING -> R.string.ota_hub_is_restarting_do_not_disconnect
    OtaPhase.COMPLETE -> R.string.ota_phase_complete
    OtaPhase.FAILED -> R.string.ota_phase_failed
}
