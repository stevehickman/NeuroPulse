package com.neuropulse.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
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
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.unit.dp
import com.neuropulse.core.protocol.NPCompositeLayer
import com.neuropulse.core.protocol.NPCompositeProtocol
import com.neuropulse.core.protocol.NPProtocolEntry
import com.neuropulse.core.protocol.NPProtocolLibrary
import com.neuropulse.core.protocol.name

// Port of iOS ProtocolComposerView. Builds a composite protocol from ordered layers, each
// referencing an existing single protocol with a start offset. Saved via library.save().
// Deep per-layer duration/intensity tuning beyond start offset is a follow-up.

@Composable
fun ProtocolComposerScreen(
    library: NPProtocolLibrary,
    onSaved: () -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var name by remember { mutableStateOf("New Composite") }
    var layers by remember { mutableStateOf(listOf<NPCompositeLayer>()) }

    val singles = remember {
        library.allProtocols.filterIsInstance<NPProtocolEntry.Single>().map { it.protocol.name }
    }

    fun save() {
        val composite = NPCompositeProtocol(
            name = name.ifBlank { "Untitled Composite" },
            layers = layers,
        )
        library.save(NPProtocolEntry.Composite(composite))
        onSaved()
    }

    Column(modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp)) {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onCancel) { Text("Cancel") }
            Text("Compose protocol", style = MaterialTheme.typography.titleMedium)
            Button(onClick = { save() }, enabled = layers.isNotEmpty()) { Text("Save") }
        }

        Spacer(Modifier.height(12.dp))
        OutlinedTextField(name, { name = it }, label = { Text("Name") }, modifier = Modifier.fillMaxWidth())

        Spacer(Modifier.height(16.dp))
        Text("Layers", style = MaterialTheme.typography.titleSmall)
        if (layers.isEmpty()) {
            Text("Add protocols below to build the sequence.", style = MaterialTheme.typography.bodySmall)
        }
        layers.forEachIndexed { index, layer ->
            Card(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
                Column(Modifier.padding(12.dp)) {
                    Row(
                        Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Text(layer.protocolName, style = MaterialTheme.typography.bodyMedium)
                        TextButton(onClick = { layers = layers.filterIndexed { i, _ -> i != index } }) {
                            Text("Remove")
                        }
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text("Start (min):", style = MaterialTheme.typography.bodySmall)
                        Spacer(Modifier.width(8.dp))
                        OutlinedTextField(
                            value = (layer.startOffsetSeconds / 60).toString(),
                            onValueChange = { v ->
                                val mins = v.filter { it.isDigit() }.toIntOrNull() ?: 0
                                layers = layers.mapIndexed { i, l ->
                                    if (i == index) l.copy(startOffsetSeconds = mins * 60) else l
                                }
                            },
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                            modifier = Modifier.width(96.dp),
                        )
                    }
                }
            }
        }

        Spacer(Modifier.height(16.dp))
        Text("Add a layer", style = MaterialTheme.typography.titleSmall)
        for (protocolName in singles) {
            OutlinedButton(
                onClick = { layers = layers + NPCompositeLayer(protocolName = protocolName) },
                modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
            ) { Text("+ $protocolName") }
        }
    }
}
