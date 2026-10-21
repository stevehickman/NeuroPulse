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
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import life.neurone.core.models.OtaPhase
import life.neurone.core.models.OtaSession
import life.neurone.core.ota.FirmwareImage
import life.neurone.shared.ble.ConnectionState
import life.neurone.shared.ota.OtaError
import life.neurone.shared.ota.OtaManager
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.*

// Port of iOS OTAView — the hub's firmware version, the update the distribution host offers, and the progress of an update
// (DI-PERF-21). ISC-108: the installed and the available version together. ISC-113: the signing key's fingerprint is shown before
// the person confirms. The safety MCU's firmware is its own button and its own confirmation.

@Composable
fun OtaScreen(
    ota: OtaManager,
    firmwareVersion: String?,
    connection: ConnectionState,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val available by ota.availableUpdate.collectAsState()
    val unavailable by ota.updatesUnavailable.collectAsState()
    val phase by ota.phase.collectAsState()
    val percent by ota.progressPercent.collectAsState()
    val session by ota.session.collectAsState()
    val error by ota.lastError.collectAsState()
    var confirmSafety by remember { mutableStateOf(false) }
    val connected = connection == ConnectionState.CONNECTED
    val ready = connected && !phase.isBusy

    // ISC-107: look again each time the screen opens.
    androidx.compose.runtime.LaunchedEffect(Unit) { ota.checkForUpdates() }

    Column(modifier.fillMaxSize()) {
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onBack) { Text(stringResource(Res.string.consent_back_button)) }
            Text(stringResource(Res.string.ota_firmware), style = MaterialTheme.typography.titleLarge)
        }

        Column(Modifier.padding(24.dp).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(16.dp)) {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text(stringResource(Res.string.ota_hub_status), style = MaterialTheme.typography.labelMedium)
                    Text(
                        stringResource(
                            if (connected) Res.string.session_conn_indicator_connected else Res.string.session_conn_indicator_disconnected,
                        ),
                        style = MaterialTheme.typography.titleMedium,
                    )
                    Spacer(Modifier.height(8.dp))
                    Text(stringResource(Res.string.ota_version_installed), style = MaterialTheme.typography.labelMedium)
                    Text(
                        firmwareVersion ?: stringResource(
                            if (connected) Res.string.ota_version_reading else Res.string.ota_unknown_connect_to_your_hub,
                        ),
                        style = MaterialTheme.typography.titleMedium,
                    )
                    available?.let {
                        Spacer(Modifier.height(8.dp))
                        Text(stringResource(Res.string.ota_version_available), style = MaterialTheme.typography.labelMedium)
                        Text(it.version, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.primary)
                    }
                }
            }

            val image = available
            when {
                image != null -> AvailableUpdate(image, error, ready, onInstall = { ota.start(image) }, onSafety = { confirmSafety = true })
                unavailable -> Text(stringResource(Res.string.ota_updates_unavailable), style = MaterialTheme.typography.bodyMedium)
                else -> Text(stringResource(Res.string.ota_up_to_date), style = MaterialTheme.typography.bodyMedium)
            }

            session?.let { Progress(it, phase, percent, onAbort = { ota.abort() }) }

            // A failure outlives the offer it came from: a refused update is shown even when no update is offered any more.
            if (image == null) error?.let { Text(stringResource(errorText(it), *errorArgs(it)), color = MaterialTheme.colorScheme.error) }

            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text(stringResource(Res.string.ota_safety), style = MaterialTheme.typography.labelMedium)
                    Text(stringResource(Res.string.ota_rollback_note), style = MaterialTheme.typography.bodySmall)
                }
            }
        }
    }

    if (confirmSafety) {
        AlertDialog(
            onDismissRequest = { confirmSafety = false },
            title = { Text(stringResource(Res.string.ota_update_safety_mcu)) },
            text = { Text(stringResource(Res.string.ota_safety_mcu_warning)) },
            confirmButton = {
                TextButton(onClick = {
                    confirmSafety = false
                    available?.let { ota.startSafetyMcu(it) }
                }) { Text(stringResource(Res.string.ota_update_safety_mcu_2), color = MaterialTheme.colorScheme.error) }
            },
            dismissButton = { TextButton(onClick = { confirmSafety = false }) { Text(stringResource(Res.string.common_cancel)) } },
        )
    }
}

@Composable
private fun AvailableUpdate(
    image: FirmwareImage,
    error: OtaError?,
    ready: Boolean,
    onInstall: () -> Unit,
    onSafety: () -> Unit,
) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(stringResource(Res.string.ota_update_available), style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(stringResource(Res.string.ota_version_label, image.version), style = MaterialTheme.typography.titleMedium)
                Text(image.buildDate, style = MaterialTheme.typography.bodySmall)
            }
            if (image.releaseNotes.isNotEmpty()) Text(image.releaseNotes, style = MaterialTheme.typography.bodySmall)
            Text(stringResource(Res.string.ota_fingerprint_label), style = MaterialTheme.typography.labelSmall)
            Text(image.ed25519PublicKeyFingerprint, style = MaterialTheme.typography.bodySmall, fontFamily = FontFamily.Monospace)
            Text(stringResource(Res.string.ota_fingerprint_verify), style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.tertiary)
            error?.let { Text(stringResource(errorText(it), *errorArgs(it)), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error) }
            if (image.isSafetyMCUFirmware) {
                Button(
                    onClick = onSafety,
                    enabled = ready,
                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
                ) { Text(stringResource(Res.string.ota_update_safety_mcu_firmware)) }
            } else {
                Button(onClick = onInstall, enabled = ready) { Text(stringResource(Res.string.ota_install_update)) }
            }
        }
    }
}

// ISC-110: the byte count while it transfers. ISC-112: the hub restarts after the commit.
@Composable
private fun Progress(session: OtaSession, phase: OtaPhase, percent: Int, onAbort: () -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(stringResource(Res.string.ota_update_progress), style = MaterialTheme.typography.labelMedium)
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(stringResource(phaseLabel(phase)), style = MaterialTheme.typography.titleMedium)
                Text(stringResource(Res.string.ota_progress_percent, percent), style = MaterialTheme.typography.bodyMedium)
            }
            LinearProgressIndicator(
                progress = { percent.coerceIn(0, 100) / 100f },
                modifier = Modifier.fillMaxWidth(),
                color = if (phase == OtaPhase.FAILED) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary,
            )
            if (phase == OtaPhase.TRANSFERRING && session.totalBytes > 0) {
                Text(
                    stringResource(
                        Res.string.ota_progress_bytes,
                        OtaSession.formattedBytes(session.sentBytes),
                        OtaSession.formattedBytes(session.totalBytes),
                    ),
                    style = MaterialTheme.typography.bodySmall,
                )
            }
            when (phase) {
                OtaPhase.APPLYING -> Text(stringResource(Res.string.ota_hub_is_restarting_do_not_disconnect), color = MaterialTheme.colorScheme.tertiary)
                OtaPhase.COMPLETE -> Text(stringResource(Res.string.ota_update_complete_hub_restarted_successfully), color = MaterialTheme.colorScheme.primary)
                OtaPhase.VERIFYING, OtaPhase.VERIFIED -> Text(stringResource(Res.string.ota_verifying), style = MaterialTheme.typography.bodySmall)
                else -> Unit
            }
            if (phase.isBusy && phase != OtaPhase.APPLYING) {
                OutlinedButton(onClick = onAbort) { Text(stringResource(Res.string.ota_abort)) }
            }
        }
    }
}

private fun errorText(error: OtaError): StringResource = when (error) {
    OtaError.NotConnected -> Res.string.ota_error_not_connected
    OtaError.ImageLoadFailed -> Res.string.ota_error_image_load
    OtaError.SignatureInvalid -> Res.string.ota_error_signature
    OtaError.WrongImageKind -> Res.string.ota_error_wrong_image
    OtaError.TransferFailed -> Res.string.ota_error_transfer
    is OtaError.HubRejected -> Res.string.ota_error_hub_rejected
    OtaError.Timeout -> Res.string.ota_error_timeout
    OtaError.VerificationFailed -> Res.string.ota_error_verification
}

// The hub's error code is the one value a message carries; rendered as the hub prints it, 0x%04X.
private fun errorArgs(error: OtaError): Array<Any> =
    if (error is OtaError.HubRejected) arrayOf("0x" + error.code.toString(16).uppercase().padStart(4, '0')) else emptyArray()

// Same OTA_PHASE_* keys iOS `OTAPhase.description` uses.
private fun phaseLabel(phase: OtaPhase): StringResource = when (phase) {
    OtaPhase.IDLE -> Res.string.ota_phase_idle
    OtaPhase.PREPARING -> Res.string.ota_phase_preparing
    OtaPhase.TRANSFERRING -> Res.string.ota_phase_transferring
    OtaPhase.VERIFYING -> Res.string.ota_phase_verifying
    OtaPhase.VERIFIED -> Res.string.ota_phase_verified
    OtaPhase.APPLYING -> Res.string.ota_phase_applying
    OtaPhase.COMPLETE -> Res.string.ota_phase_complete
    OtaPhase.FAILED -> Res.string.ota_phase_failed
}
