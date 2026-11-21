package life.neurone.shared.ui

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
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.NPCompositeLayer
import life.neurone.core.protocol.NPBounds
import life.neurone.core.protocol.NPCompositeProtocol
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.name
import org.jetbrains.compose.resources.stringResource
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.*

// Port of iOS ProtocolComposerView. Builds a composite protocol from ordered layers, each
// referencing an existing single protocol with a start offset. Saved via library.save().
// Deep per-layer duration/intensity tuning beyond start offset is a follow-up.

/** The most minutes a layer's start offset can be: the offset is held as whole seconds in an Int. */
internal const val MAX_START_OFFSET_MINUTES = Int.MAX_VALUE / 60

/**
 * The seconds a typed number of minutes means as a layer's start offset, or null when it is not an offset the layer can
 * hold: blank, or too large to represent. 0 is an offset (the layer starts with the sequence); only a time below 0 is
 * not (`NPBounds.ProtocolFields.layerStartOffsetSeconds`). Null is refused by the screen and never replaced by a figure
 * of its own: a blank field was once saved as 0, and a number with too many digits as 0 too, so the offset a person
 * typed was not the one saved (`NP-SW-001` SWR-UI-01 (f)).
 */
internal fun startOffsetSecondsOrNull(minutesText: String): Int? {
    val minutes = minutesText.toLongOrNull() ?: return null
    if (minutes > MAX_START_OFFSET_MINUTES) return null
    val seconds = (minutes * 60).toInt()
    return seconds.takeIf { it >= (NPBounds.ProtocolFields.layerStartOffsetSeconds.min?.value ?: 0.0) }
}

@Composable
fun ProtocolComposerScreen(
    library: NPProtocolLibrary,
    onSaved: () -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val defaultName = stringResource(Res.string.protocol_composer_new_composite)
    val untitledName = stringResource(Res.string.protocol_composer_untitled_composite)
    var name by remember { mutableStateOf(defaultName) }
    var layers by remember { mutableStateOf(listOf<NPCompositeLayer>()) }
    // What was typed into each layer's start offset, by layer id. A layer with no entry has not been edited and shows
    // the offset it holds; the offset is changed only by a valid edit, and a refused one blocks Save.
    val offsetText = remember { mutableStateMapOf<Any, String>() }
    val offsetRefused = layers.any { l -> offsetText[l.id]?.let { startOffsetSecondsOrNull(it) == null } == true }

    val singles = remember {
        library.allProtocols.filterIsInstance<NPProtocolEntry.Single>().map { it.protocol.name }
    }

    fun save() {
        if (offsetRefused) return
        val composite = NPCompositeProtocol(
            name = name.ifBlank { untitledName },
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
            TextButton(onClick = onCancel) { Text(stringResource(Res.string.common_cancel)) }
            Text(stringResource(Res.string.protocol_composer_compose_protocol), style = MaterialTheme.typography.titleMedium)
            Button(onClick = { save() }, enabled = layers.isNotEmpty() && !offsetRefused) { Text(stringResource(Res.string.protocol_composer_save)) }
        }

        Spacer(Modifier.height(12.dp))
        OutlinedTextField(name, { name = it }, label = { Text(stringResource(Res.string.clinician_grant_name_placeholder)) }, modifier = Modifier.fillMaxWidth())

        Spacer(Modifier.height(16.dp))
        Text(stringResource(Res.string.protocol_layers), style = MaterialTheme.typography.titleSmall)
        if (layers.isEmpty()) {
            Text(stringResource(Res.string.protocol_composer_add_protocols_below_to_build_the_sequence), style = MaterialTheme.typography.bodySmall)
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
                            Text(stringResource(Res.string.web_remove))
                        }
                    }
                    val typed = offsetText[layer.id]
                    val refused = typed != null && startOffsetSecondsOrNull(typed) == null
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(stringResource(Res.string.protocol_composer_start_min), style = MaterialTheme.typography.bodySmall)
                        Spacer(Modifier.width(8.dp))
                        OutlinedTextField(
                            value = typed ?: (layer.startOffsetSeconds / 60).toString(),
                            onValueChange = { v ->
                                val text = v.filter { it.isDigit() }
                                offsetText[layer.id] = text
                                startOffsetSecondsOrNull(text)?.let { seconds ->
                                    layers = layers.mapIndexed { i, l ->
                                        if (i == index) l.copy(startOffsetSeconds = seconds) else l
                                    }
                                }
                            },
                            isError = refused,
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                            modifier = Modifier.width(96.dp),
                        )
                    }
                    if (refused) {
                        val message = if (typed.isNullOrBlank()) {
                            stringResource(Res.string.common_fill_required_fields)
                        } else {
                            stringResource(Res.string.web_param_out_of_range, "0", MAX_START_OFFSET_MINUTES.toString())
                        }
                        Text(message, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }

        Spacer(Modifier.height(16.dp))
        Text(stringResource(Res.string.protocol_composer_add_a_layer), style = MaterialTheme.typography.titleSmall)
        for (protocolName in singles) {
            OutlinedButton(
                onClick = { layers = layers + NPCompositeLayer(protocolName = protocolName) },
                modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
            ) { Text("+ $protocolName") }
        }
    }
}
