package com.neuropulse.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.neuropulse.core.protocol.NPProtocolDefinition
import com.neuropulse.core.protocol.NPProtocolEntry
import com.neuropulse.core.protocol.NPProtocolLibrary
import com.neuropulse.core.protocol.ProtocolAvailability
import com.neuropulse.core.protocol.id
import com.neuropulse.core.protocol.isComposite
import com.neuropulse.core.protocol.name

// Port of iOS ProtocolMenuView (app/ios/NeuroPulse/Views/Protocol/ProtocolMenuView.swift).
// Lists bundled + user protocols with availability (device tier + BIPA EEG-consent gate,
// ISC-90/ISC-45) and validation badges. Selecting an available protocol invokes [onSelect]
// (the caller uploads it via GATT once a session is wired). EEG-adaptive protocols are
// visibly disabled with the localized "brainwave data consent" message when consent is
// declined — every selection path funnels through library.availability(), exactly as iOS
// routes them through NPProtocolLibrary.availability(for:).

private sealed interface ProtocolEditor {
    data object New : ProtocolEditor
    data object Compose : ProtocolEditor
    data class Script(val text: String) : ProtocolEditor
    data class Form(val def: NPProtocolDefinition) : ProtocolEditor
}

@Composable
fun ProtocolMenuScreen(
    library: NPProtocolLibrary,
    consentGranted: Boolean,
    eegUnavailableMessage: String,
    onSelect: (NPProtocolEntry) -> Unit,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
) {
    // Reflect the current BIPA consent into the library so EEG-adaptive rows gate live.
    library.updateEEGConsent(consentGranted)

    var editor by remember { mutableStateOf<ProtocolEditor?>(null) }
    var version by remember { mutableIntStateOf(0) } // bump to re-read the (non-observable) library

    when (val e = editor) {
        is ProtocolEditor.New -> ProtocolScriptEditorScreen(
            library = library,
            initialText = NEW_PROTOCOL_TEMPLATE,
            onSaved = { editor = null; version++ },
            onCancel = { editor = null },
            modifier = modifier,
        )
        is ProtocolEditor.Compose -> ProtocolComposerScreen(
            library = library,
            onSaved = { editor = null; version++ },
            onCancel = { editor = null },
            modifier = modifier,
        )
        is ProtocolEditor.Script -> ProtocolScriptEditorScreen(
            library = library,
            initialText = e.text,
            onSaved = { editor = null; version++ },
            onCancel = { editor = null },
            modifier = modifier,
        )
        is ProtocolEditor.Form -> ProtocolEditorScreen(
            library = library,
            existing = e.def,
            onSaved = { editor = null; version++ },
            onCancel = { editor = null },
            onEditScript = {
                editor = ProtocolEditor.Script(library.exportScript(NPProtocolEntry.Single(e.def)))
            },
            modifier = modifier,
        )
        null -> {
            val protocols = remember(version) { library.allProtocols }
            Column(modifier.fillMaxSize()) {
                Row(
                    modifier = Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    TextButton(onClick = onBack) { Text("Back") }
                    Text("Protocols", style = MaterialTheme.typography.titleLarge)
                    Spacer(Modifier.weight(1f))
                    TextButton(onClick = { editor = ProtocolEditor.Compose }) { Text("Compose") }
                    TextButton(onClick = { editor = ProtocolEditor.New }) { Text("New") }
                }
                LazyColumn(
                    modifier = Modifier.fillMaxSize().padding(horizontal = 16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    items(protocols, key = { it.id }) { entry ->
                        val availability = library.availability(entry)
                        val validation = library.validationResult(entry)
                        val editable = library.canEdit(entry)
                        ProtocolRow(
                            entry = entry,
                            availability = availability,
                            errorCount = validation.errors.size,
                            warningCount = validation.warnings.size,
                            eegUnavailableMessage = eegUnavailableMessage,
                            onClick = { if (availability.isAvailable) onSelect(entry) },
                            onEdit = if (!editable) null else {
                                {
                                    editor = when (entry) {
                                        is NPProtocolEntry.Single -> ProtocolEditor.Form(entry.protocol)
                                        else -> ProtocolEditor.Script(library.exportScript(entry))
                                    }
                                }
                            },
                            onDelete = if (!editable) null else {
                                { library.delete(entry.id); version++ }
                            },
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ProtocolRow(
    entry: NPProtocolEntry,
    availability: ProtocolAvailability,
    errorCount: Int,
    warningCount: Int,
    eegUnavailableMessage: String,
    onClick: () -> Unit,
    onEdit: (() -> Unit)? = null,
    onDelete: (() -> Unit)? = null,
) {
    val available = availability.isAvailable
    Card(
        modifier = Modifier.fillMaxWidth().clickable(onClick = onClick),
    ) {
        Column(Modifier.padding(16.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Spacer(
                    Modifier
                        .size(10.dp)
                        .clip(CircleShape)
                        .background(if (available) Color(0xFF2E7D32) else Color(0xFFD32F2F)),
                )
                Spacer(Modifier.size(8.dp))
                Text(
                    entry.name,
                    style = MaterialTheme.typography.titleMedium,
                    color = if (available) MaterialTheme.colorScheme.onSurface
                    else MaterialTheme.colorScheme.onSurfaceVariant,
                    fontWeight = FontWeight.Medium,
                )
            }

            val reason = unavailableReason(availability, eegUnavailableMessage)
            if (reason != null) {
                Spacer(Modifier.height(4.dp))
                Text(reason, style = MaterialTheme.typography.bodySmall, color = Color(0xFFD32F2F))
            }

            if (errorCount > 0 || warningCount > 0 || entry.isComposite) {
                Spacer(Modifier.height(6.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    if (entry.isComposite) Badge("Composite", Color(0xFF5E35B1))
                    if (errorCount > 0) Badge("$errorCount error${plural(errorCount)}", Color(0xFFD32F2F))
                    if (warningCount > 0) Badge("$warningCount warning${plural(warningCount)}", Color(0xFFF9A825))
                }
            }

            // Edit / delete for user (non-read-only) protocols.
            if (onEdit != null || onDelete != null) {
                Spacer(Modifier.height(4.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    onEdit?.let { TextButton(onClick = it) { Text("Edit") } }
                    onDelete?.let { TextButton(onClick = it) { Text("Delete", color = Color(0xFFD32F2F)) } }
                }
            }
        }
    }
}

@Composable
private fun Badge(text: String, color: Color) {
    Text(
        text,
        style = MaterialTheme.typography.labelSmall,
        color = Color.White,
        modifier = Modifier
            .clip(MaterialTheme.shapes.small)
            .background(color)
            .padding(horizontal = 6.dp, vertical = 2.dp),
    )
}

private fun plural(n: Int): String = if (n == 1) "" else "s"

/** UI-side mapping of ProtocolAvailability → display reason (parity with iOS unavailableReason). */
private fun unavailableReason(availability: ProtocolAvailability, eegUnavailableMessage: String): String? =
    when (availability) {
        is ProtocolAvailability.Available -> null
        is ProtocolAvailability.EegConsentRequired -> eegUnavailableMessage
        is ProtocolAvailability.Unavailable ->
            if (availability.missingModalities.isEmpty()) "No device connected."
            else "Requires: " + availability.missingModalities.joinToString(", ") { it.rawValue }
    }
