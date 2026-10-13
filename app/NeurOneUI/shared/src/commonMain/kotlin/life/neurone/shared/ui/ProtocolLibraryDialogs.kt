package life.neurone.shared.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.SaveResult
import life.neurone.core.protocol.UploadGate
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

// The screens that tell a person why the library refused something (SWR-PL-02/06, SWR-FM-03/05/06, SWR-HV-04). The
// refusals themselves are the shared core's, reached through NPProtocolLibrary; these dialogs word them from the locale
// files and offer the way forward the requirement names. Nothing here decides anything.

/**
 * A save the library refused because the protocol's family has been used (or already has a variant for this hardware
 * version). Nothing was saved. The offer is a copy of the whole family that keeps the person's edit
 * ([NPProtocolLibrary.saveAsClone]); [onResolved] is called once the copy is saved.
 */
@Composable
fun SaveRefusalDialog(
    library: NPProtocolLibrary,
    entry: NPProtocolEntry,
    result: SaveResult,
    onResolved: () -> Unit,
    onDismiss: () -> Unit,
) {
    val variantExists = result is SaveResult.RefusedVariantExists
    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(
                stringResource(
                    if (variantExists) Res.string.protocol_library_variant_exists_title else Res.string.protocol_library_used_title,
                ),
            )
        },
        text = {
            Text(
                stringResource(
                    if (variantExists) Res.string.protocol_library_variant_exists_body else Res.string.protocol_library_used_body,
                ),
            )
        },
        confirmButton = {
            TextButton(onClick = {
                library.saveAsClone(entry)
                onResolved()
            }) { Text(stringResource(Res.string.protocol_library_clone_action)) }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(Res.string.common_cancel)) } },
    )
}

/** A delete the library refused because the family was used (SWR-FM-05): the way forward is to archive it. */
@Composable
fun DeleteUsedDialog(onArchive: () -> Unit, onDismiss: () -> Unit) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(Res.string.protocol_library_delete_used_title)) },
        text = { Text(stringResource(Res.string.protocol_library_delete_used_body)) },
        confirmButton = { TextButton(onClick = onArchive) { Text(stringResource(Res.string.protocol_library_archive_action)) } },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(Res.string.common_cancel)) } },
    )
}

/**
 * An upload of a copy whose content is its source's (SWR-PL-06). Nothing was sent and nothing is marked used. The three
 * outcomes: send the original, go back and edit the copy, or cancel.
 */
@Composable
fun UnmodifiedCloneDialog(onUploadSource: () -> Unit, onEditClone: () -> Unit, onCancel: () -> Unit) {
    AlertDialog(
        onDismissRequest = onCancel,
        title = { Text(stringResource(Res.string.protocol_library_unmodified_title)) },
        text = { Text(stringResource(Res.string.protocol_library_unmodified_body)) },
        confirmButton = {
            Column {
                TextButton(onClick = onUploadSource) { Text(stringResource(Res.string.protocol_library_unmodified_upload_source)) }
                TextButton(onClick = onEditClone) { Text(stringResource(Res.string.protocol_library_unmodified_edit)) }
            }
        },
        dismissButton = { TextButton(onClick = onCancel) { Text(stringResource(Res.string.common_cancel)) } },
    )
}

/**
 * An upload to a helmet whose hardware version the protocol has no layout for (SWR-HV-04). Nothing was sent. Both layouts
 * are summarised by how many sockets they share and how many only one has; [variant], when the family has one for the
 * helmet, is offered and never chosen silently.
 */
@Composable
fun HardwareMismatchDialog(
    gate: UploadGate.HardwareMismatch,
    variant: NPProtocolEntry?,
    onUseVariant: (NPProtocolEntry) -> Unit,
    onDismiss: () -> Unit,
) {
    val shared = gate.sharedSockets
    val onlyProtocol = gate.onlyProtocolSockets
    val onlyHelmet = gate.onlyHelmetSockets
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(Res.string.protocol_library_mismatch_title)) },
        text = {
            Column {
                Text(
                    stringResource(
                        Res.string.protocol_library_mismatch_body,
                        gate.protocolVersions.joinToString(", "),
                        gate.helmetVersion.toString(),
                    ),
                )
                if (shared != null && onlyProtocol != null && onlyHelmet != null) {
                    Spacer(Modifier.height(8.dp))
                    Text(
                        stringResource(
                            Res.string.protocol_library_mismatch_diff,
                            shared.toString(), onlyProtocol.toString(), onlyHelmet.toString(),
                        ),
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
                if (variant == null) {
                    Spacer(Modifier.height(8.dp))
                    Text(stringResource(Res.string.protocol_library_mismatch_no_variant), style = MaterialTheme.typography.bodySmall)
                }
            }
        },
        confirmButton = {
            if (variant != null) {
                TextButton(onClick = { onUseVariant(variant) }) { Text(stringResource(Res.string.protocol_library_mismatch_use_variant)) }
            }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(Res.string.common_cancel)) } },
    )
}
