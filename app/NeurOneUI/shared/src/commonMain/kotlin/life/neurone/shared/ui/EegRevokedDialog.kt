package life.neurone.shared.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.shared.resources.*
import life.neurone.shared.session.UhdrFolder
import life.neurone.shared.session.deleteRecordings
import life.neurone.shared.session.recordingNames
import org.jetbrains.compose.resources.stringResource

// What stays after the biometric (EEG) release is withdrawn, and the offer to delete it (REQ-SW03-37; NP-PRIV-ANALYSIS-006 F-16).
// Withdrawing stops EEG from now on and deletes nothing by itself: the release told the person a recording stays on the device until
// they delete it, so the dialog says what stays and offers the deletion, which happens only when the person asks for it.

private enum class EegRevokedStep { Told, Deleted, NotAllDeleted }

/**
 * Shown once the EEG release is withdrawn. [folder] is the app's UHDR folder, or null on a host that keeps no recordings; the delete
 * action is offered only when the folder holds a recording.
 */
@Composable
fun EegRevokedDialog(folder: UhdrFolder?, onDismiss: () -> Unit) {
    var held by remember(folder) { mutableStateOf(0) }
    LaunchedEffect(folder) { held = folder?.let { runCatching { it.recordingNames().size }.getOrDefault(0) } ?: 0 }
    var step by remember { mutableStateOf(EegRevokedStep.Told) }
    var busy by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()

    AlertDialog(
        onDismissRequest = { if (!busy) onDismiss() },
        title = { Text(stringResource(Res.string.eeg_revoked_title)) },
        text = {
            Column {
                when (step) {
                    EegRevokedStep.Told -> {
                        Text(stringResource(Res.string.eeg_revoked_body), style = MaterialTheme.typography.bodyMedium)
                        if (held > 0) {
                            Spacer(Modifier.height(8.dp))
                            Text(stringResource(Res.string.eeg_revoked_delete_note), style = MaterialTheme.typography.bodySmall)
                        }
                    }
                    EegRevokedStep.Deleted -> Text(stringResource(Res.string.eeg_revoked_deleted))
                    EegRevokedStep.NotAllDeleted -> Text(stringResource(Res.string.eeg_revoked_delete_failed))
                }
            }
        },
        confirmButton = {
            if (step == EegRevokedStep.Told && held > 0 && folder != null) {
                TextButton(enabled = !busy, onClick = {
                    busy = true
                    scope.launch {
                        val left = folder.deleteRecordings()
                        step = if (left == 0) EegRevokedStep.Deleted else EegRevokedStep.NotAllDeleted
                        busy = false
                    }
                }) { Text(stringResource(Res.string.eeg_revoked_delete_button)) }
            }
        },
        dismissButton = {
            TextButton(enabled = !busy, onClick = onDismiss) {
                Text(
                    stringResource(
                        if (step == EegRevokedStep.Told && held > 0) Res.string.eeg_revoked_keep_button else Res.string.common_close,
                    ),
                )
            }
        },
    )
}
