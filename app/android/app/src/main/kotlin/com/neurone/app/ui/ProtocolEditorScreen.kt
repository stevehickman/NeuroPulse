package com.neurone.app.ui

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
import com.neurone.core.protocol.NPProtocolDefinition
import com.neurone.core.protocol.NPProtocolEntry
import com.neurone.core.protocol.NPProtocolLibrary
import com.neurone.core.protocol.NPTimingMode

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

    fun save() {
        val updated = existing.copy(
            name = name.ifBlank { "Untitled" },
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
            TextButton(onClick = onCancel) { Text("Cancel") }
            Text("Edit protocol", style = MaterialTheme.typography.titleMedium)
            Button(onClick = { save() }) { Text("Save") }
        }

        Spacer(Modifier.height(12.dp))
        OutlinedTextField(name, { name = it }, label = { Text("Name") }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(description, { description = it }, label = { Text("Description") }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(tags, { tags = it }, label = { Text("Tags (comma-separated)") }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(
            minutes,
            { minutes = it.filter { c -> c.isDigit() } },
            label = { Text("Duration (minutes)") },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.fillMaxWidth(),
        )

        Spacer(Modifier.height(16.dp))
        Text(
            "Modalities: " + existing.modalities.filter { it.enabled }
                .joinToString(", ") { it.modalityType.rawValue }.ifEmpty { "none" },
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onEditModalities, modifier = Modifier.fillMaxWidth()) {
            Text("Edit modalities")
        }
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onEditScript, modifier = Modifier.fillMaxWidth()) {
            Text("Edit as script")
        }
    }
}
