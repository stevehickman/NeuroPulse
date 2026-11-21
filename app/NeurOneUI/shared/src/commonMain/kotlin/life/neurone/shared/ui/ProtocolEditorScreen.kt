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
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.NPBounds
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.NPTimingMode
import life.neurone.core.protocol.SaveResult
import org.jetbrains.compose.resources.stringResource
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.*

// Port of iOS ProtocolEditorView (metadata form). Quick name/description/tags/duration edit
// for a single user protocol; deep per-modality parameter editing is done in the script editor
// (ModalityEditorView's full widget set is a separate follow-up). Modalities are preserved.

/**
 * The seconds a typed number of minutes means, or null when it is not a duration the protocol can hold: blank, below
 * the shared timing minimum (`NPBounds.ProtocolFields.timingValue`, so 0 minutes), or too large to represent. Null is
 * refused by the screen and is never replaced by a figure of its own: the duration a person typed is the duration
 * they meant, and a substitute (the 20 minutes and the 1 minute this screen once saved) is one they did not choose
 * (`NP-SW-001` SWR-UI-01 (f)).
 */
internal fun durationSecondsOrNull(minutesText: String): Int? {
    val minutes = minutesText.toLongOrNull() ?: return null
    if (minutes > Int.MAX_VALUE / 60) return null
    val seconds = (minutes * 60).toInt()
    return seconds.takeIf { it >= (NPBounds.ProtocolFields.timingValue.min?.value ?: 0.0) }
}

@Composable
fun ProtocolEditorScreen(
    library: NPProtocolLibrary,
    existing: NPProtocolDefinition,
    onSaved: () -> Unit,
    onCancel: () -> Unit,
    onEditScript: () -> Unit,
    onEditModalities: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var name by remember { mutableStateOf(existing.name) }
    var description by remember { mutableStateOf(existing.description) }
    var tags by remember { mutableStateOf(existing.tags.joinToString(", ")) }
    // The duration field exists only for a protocol timed by duration. An interval-count protocol has no minutes to
    // show or to save, so its timing is left exactly as it is (it was once shown as "20" and saved as 20 minutes).
    val duration = existing.timingMode as? NPTimingMode.Duration
    val initialMinutes = duration?.let { (it.seconds / 60).toString() }
    var minutes by remember { mutableStateOf(initialMinutes ?: "") }
    // The timing is rewritten only when the field was edited, and then only to a valid duration. An untouched field
    // saves the timing it was given, so a duration that is not a whole number of minutes is not rounded down.
    val timingEdited = duration != null && minutes != initialMinutes
    val editedSeconds = if (timingEdited) durationSecondsOrNull(minutes) else null
    val durationRefused = timingEdited && editedSeconds == null

    val untitledName = stringResource(Res.string.protocol_editor_untitled)
    var refusal by remember { mutableStateOf<Pair<NPProtocolEntry, SaveResult>?>(null) }

    fun save() {
        if (durationRefused) return
        val updated = existing.copy(
            name = name.ifBlank { untitledName },
            description = description,
            tags = tags.split(",").map { it.trim() }.filter { it.isNotEmpty() },
            timingMode = editedSeconds?.let { NPTimingMode.Duration(it) } ?: existing.timingMode,
        )
        val entry = NPProtocolEntry.Single(updated)
        val result = library.save(entry)
        // A used family refuses the save; the dialog explains why and offers a copy that keeps this edit.
        if (result is SaveResult.RefusedFamilyUsed || result is SaveResult.RefusedVariantExists) refusal = entry to result else onSaved()
    }

    Column(
        modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
    ) {
        refusal?.let { (e, r) ->
            SaveRefusalDialog(library, e, r, onResolved = { refusal = null; onSaved() }, onDismiss = { refusal = null })
        }
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onCancel) { Text(stringResource(Res.string.common_cancel)) }
            Text(stringResource(Res.string.protocol_editor_edit_protocol), style = MaterialTheme.typography.titleMedium)
            Button(onClick = { save() }, enabled = !durationRefused) { Text(stringResource(Res.string.protocol_composer_save)) }
        }

        Spacer(Modifier.height(12.dp))
        OutlinedTextField(name, { name = it }, label = { Text(stringResource(Res.string.clinician_grant_name_placeholder)) }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(description, { description = it }, label = { Text(stringResource(Res.string.ui_field_description)) }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(tags, { tags = it }, label = { Text(stringResource(Res.string.protocol_editor_tags_comma_separated)) }, modifier = Modifier.fillMaxWidth())
        if (duration != null) {
            Spacer(Modifier.height(8.dp))
            OutlinedTextField(
                minutes,
                { minutes = it.filter { c -> c.isDigit() } },
                label = { Text(stringResource(Res.string.protocol_editor_duration_minutes)) },
                isError = durationRefused,
                supportingText = if (durationRefused) { { Text(stringResource(Res.string.validate_msg_general_duration_3)) } } else null,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
            )
        }

        Spacer(Modifier.height(16.dp))
        Text(
            stringResource(Res.string.protocol_editor_modalities) +
                // `map` is inline, so stringResource may be called inside it; joinToString's transform is not.
                existing.modalities.filter { it.enabled }.map { stringResource(modalityNameRes(it.modalityType)) }
                    .joinToString(", ")
                    .ifEmpty { stringResource(Res.string.ui_none) },
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onEditModalities, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(Res.string.and_modality_edit_modalities))
        }
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onEditScript, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(Res.string.protocol_editor_edit_as_script))
        }
    }
}
