package life.neurone.app.ui

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
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.NPTimingMode
import androidx.compose.ui.res.stringResource
import life.neurone.app.R

// Port of iOS ProtocolEditorView (metadata form). Quick name/description/tags/duration edit
// for a single user protocol; deep per-modality parameter editing is done in the script editor
// (ModalityEditorView's full widget set is a separate follow-up). Modalities are preserved.

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
    var minutes by remember {
        mutableStateOf(
            when (val t = existing.timingMode) {
                is NPTimingMode.Duration -> (t.seconds / 60).toString()
                is NPTimingMode.IntervalCount -> "20"
            },
        )
    }

    val untitledName = stringResource(R.string.protocol_editor_untitled)

    fun save() {
        val updated = existing.copy(
            name = name.ifBlank { untitledName },
            description = description,
            tags = tags.split(",").map { it.trim() }.filter { it.isNotEmpty() },
            timingMode = NPTimingMode.Duration((minutes.toIntOrNull() ?: 20).coerceAtLeast(1) * 60),
        )
        library.save(NPProtocolEntry.Single(updated))
        onSaved()
    }

    Column(
        modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
    ) {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onCancel) { Text(stringResource(R.string.common_cancel)) }
            Text(stringResource(R.string.protocol_editor_edit_protocol), style = MaterialTheme.typography.titleMedium)
            Button(onClick = { save() }) { Text(stringResource(R.string.protocol_composer_save)) }
        }

        Spacer(Modifier.height(12.dp))
        OutlinedTextField(name, { name = it }, label = { Text(stringResource(R.string.clinician_grant_name_placeholder)) }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(description, { description = it }, label = { Text(stringResource(R.string.ui_field_description)) }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(tags, { tags = it }, label = { Text(stringResource(R.string.protocol_editor_tags_comma_separated)) }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(
            minutes,
            { minutes = it.filter { c -> c.isDigit() } },
            label = { Text(stringResource(R.string.protocol_editor_duration_minutes)) },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.fillMaxWidth(),
        )

        Spacer(Modifier.height(16.dp))
        Text(
            stringResource(R.string.protocol_editor_modalities) + existing.modalities.filter { it.enabled }
                .joinToString(", ") { it.modalityType.rawValue }.ifEmpty { "none" },
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onEditModalities, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.and_modality_edit_modalities))
        }
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onEditScript, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.protocol_editor_edit_as_script))
        }
    }
}
