package com.neuropulse.app.ui

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
import com.neuropulse.core.models.OtaPhase
import com.neuropulse.core.models.OtaStatusPacket

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
            TextButton(onClick = onBack) { Text("Back") }
            Text("Firmware", style = MaterialTheme.typography.titleLarge)
        }

        Column(Modifier.padding(24.dp)) {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text("Hub firmware version", style = MaterialTheme.typography.labelMedium)
                    Text(firmwareVersion ?: "Unknown — connect to your hub", style = MaterialTheme.typography.titleMedium)
                }
            }

            Spacer(Modifier.height(16.dp))

            if (status != null) {
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(16.dp)) {
                        Text("Update status", style = MaterialTheme.typography.labelMedium)
                        Text(phaseLabel(status.phase), style = MaterialTheme.typography.titleMedium)
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
                            Text("Error code ${status.errorCode}", style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            } else {
                Text(
                    "Firmware updates install automatically over USB-C or Bluetooth when a new " +
                        "version is available. No action is needed.",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }
    }
}

private fun phaseLabel(phase: OtaPhase?): String = when (phase) {
    OtaPhase.IDLE, null -> "Up to date"
    OtaPhase.RECEIVING -> "Downloading update…"
    OtaPhase.VERIFYING -> "Verifying signature…"
    OtaPhase.COMMITTING -> "Installing…"
    OtaPhase.VERIFIED -> "Verified"
    OtaPhase.REBOOTING -> "Restarting hub…"
    OtaPhase.COMPLETE -> "Update complete"
    OtaPhase.FAILED -> "Update failed"
}
