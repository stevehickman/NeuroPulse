package com.neuropulse.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import com.neuropulse.core.protocol.NPPSError
import com.neuropulse.core.protocol.NPProtocolLibrary

// Port of iOS ProtocolScriptEditorView. Full-power NPPS text editor: authors compile through
// the same parser/serializer the whole app uses. On Save the script is parsed to an
// NPProtocolEntry and persisted via the library; parse errors show the offending line.

@Composable
fun ProtocolScriptEditorScreen(
    library: NPProtocolLibrary,
    initialText: String,
    onSaved: () -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var text by remember { mutableStateOf(initialText) }
    var error by remember { mutableStateOf<String?>(null) }

    fun save() {
        try {
            val entry = library.importScript(text)
            library.save(entry)
            onSaved()
        } catch (e: NPPSError) {
            error = "Line ${e.line}: ${e.messageText}"
        } catch (e: Exception) {
            error = e.message ?: "Could not parse the protocol script."
        }
    }

    Column(modifier.fillMaxSize().padding(16.dp)) {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onCancel) { Text("Cancel") }
            Text("Protocol script", style = MaterialTheme.typography.titleMedium)
            Button(onClick = { save() }) { Text("Save") }
        }

        error?.let {
            Spacer(Modifier.height(8.dp))
            Text(it, color = Color(0xFFD32F2F), style = MaterialTheme.typography.bodySmall)
        }

        Spacer(Modifier.height(8.dp))
        OutlinedTextField(
            value = text,
            onValueChange = { text = it; error = null },
            modifier = Modifier.fillMaxWidth().weight(1f),
            textStyle = TextStyle(fontFamily = FontFamily.Monospace),
            label = { Text("NPPS") },
        )
    }
}

/** A minimal valid NPPS template for a brand-new user protocol (no id → fresh UUID, editable). */
const val NEW_PROTOCOL_TEMPLATE: String = """protocol "New Protocol" {
    description: "My custom protocol"
    author: "You"
    version: "1.0"
    duration: 20m

    pbm_transcranial {
        intensity: 75%
        frequency: 10Hz
        duty_cycle: 25%
        zones: all
        wavelength: 660_808nm
    }
}
"""
