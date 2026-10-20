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
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.NPPBMChannelElement
import life.neurone.core.protocol.NPWavelengthRulesStore
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

// Port of the web WavelengthRulesPane (HelmetConfig): which requested wavelengths each emitter channel may deliver
// (NP-NPPS-REF-001 §7a). Loosening or tightening a window changes what compiles, immediately. The store refuses an
// invalid window and keeps the last good one, so a half-typed number never reaches a compile.

private val channelLabel: Map<NPPBMChannelElement, StringResource> = mapOf(
    NPPBMChannelElement.LED_660 to Res.string.element_led_660,
    NPPBMChannelElement.LED_808 to Res.string.element_led_808,
    NPPBMChannelElement.LED_1064 to Res.string.element_led_1064,
)

@Composable
fun WavelengthRulesScreen(
    store: NPWavelengthRulesStore,
    onDone: () -> Unit,
    modifier: Modifier = Modifier,
) {
    // The store holds the rules; this counter re-reads them after an edit.
    var revision by remember { mutableStateOf(0) }
    var error by remember { mutableStateOf("") }
    var script by remember { mutableStateOf("") }
    val clipboard = LocalClipboardManager.current

    Column(modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            TextButton(onClick = onDone) { Text(stringResource(Res.string.consent_done_button)) }
            Text(stringResource(Res.string.web_pane_wavelength_rules), style = MaterialTheme.typography.titleMedium)
            TextButton(
                enabled = store.editedChannels.isNotEmpty(),
                onClick = { store.resetChannel(); error = ""; revision++ },
            ) { Text(stringResource(Res.string.web_wl_rules_reset_all)) }
        }
        Spacer(Modifier.height(8.dp))
        Text(stringResource(Res.string.web_wl_rules_note), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(12.dp))

        androidx.compose.runtime.key(revision) {
            NPPBMChannelElement.entries.forEach { element ->
                val rule = store.resolved.channels.firstOrNull { it.element == element } ?: return@forEach
                val default = store.defaults.channels.firstOrNull { it.element == element }
                Column(Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
                    Text(stringResource(channelLabel.getValue(element)), style = MaterialTheme.typography.titleSmall)
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        WindowField(stringResource(Res.string.web_wl_rules_from), rule.minNm) { n ->
                            error = store.setChannel(rule.copy(minNm = n)).joinToString("; ")
                        }
                        Spacer(Modifier.width(12.dp))
                        WindowField(stringResource(Res.string.web_wl_rules_to), rule.maxNm) { n ->
                            error = store.setChannel(rule.copy(maxNm = n)).joinToString("; ")
                        }
                    }
                    default?.let {
                        Text(
                            stringResource(Res.string.web_wl_rules_default_window, formatNumber(it.minNm), formatNumber(it.maxNm)),
                            style = MaterialTheme.typography.bodySmall,
                        )
                    }
                    if (element in store.editedChannels) {
                        TextButton(onClick = { store.resetChannel(element); error = ""; revision++ }) {
                            Text(stringResource(Res.string.web_wl_rules_reset))
                        }
                    }
                }
            }
        }

        if (error.isNotEmpty()) {
            Text(error, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall)
        }

        Spacer(Modifier.height(16.dp))
        OutlinedButton(onClick = { clipboard.setText(AnnotatedString(store.exportUserRules())) }) {
            Text(stringResource(Res.string.web_wl_rules_export))
        }

        Spacer(Modifier.height(16.dp))
        Text(stringResource(Res.string.web_wl_rules_import), style = MaterialTheme.typography.titleSmall)
        OutlinedTextField(
            value = script,
            onValueChange = { script = it },
            placeholder = { Text(stringResource(Res.string.web_wl_rules_enter_script)) },
            modifier = Modifier.fillMaxWidth().height(160.dp),
            textStyle = MaterialTheme.typography.bodySmall.copy(fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace),
        )
        Spacer(Modifier.height(8.dp))
        Button(
            enabled = script.isNotBlank(),
            onClick = {
                runCatching { store.importUserRules(script) }.fold(
                    onSuccess = { error = ""; script = ""; revision++ },
                    onFailure = { error = it.message ?: it.toString() },
                )
            },
        ) { Text(stringResource(Res.string.web_wl_rules_import_apply)) }
    }
}

/** One edge of a window. The number is committed as it parses; the store keeps the last window that validates. */
@Composable
private fun WindowField(label: String, value: Double, onNumber: (Double) -> Unit) {
    var text by remember { mutableStateOf(formatNumber(value)) }
    OutlinedTextField(
        value = text,
        onValueChange = { v ->
            val filtered = v.filter { it.isDigit() || it == '.' }
            text = filtered
            filtered.toDoubleOrNull()?.let(onNumber)
        },
        label = { Text(label) },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
        singleLine = true,
        modifier = Modifier.width(140.dp),
    )
}
