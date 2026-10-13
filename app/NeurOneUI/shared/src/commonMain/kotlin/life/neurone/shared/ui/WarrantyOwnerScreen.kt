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
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import life.neurone.core.models.PauseLimit
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.familyKey
import life.neurone.core.protocol.isRunnable
import life.neurone.shared.AppServices
import life.neurone.shared.ble.ConnectionState
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

// The warranty owner's settings (SWR-WO-01, SWR-SS-05): the choices that apply to everyone who uses the helmet, as opposed
// to a person's own. The owner may be a clinic, an institution or one person at home, and the app does not track which
// (CLAUDE.md §6.0), so nothing here asks who is using it or holds anything about a person: the offered set is a set of
// protocol families, and the pause limit is a number of minutes. A choice made for one person is in that person's records.
// The screen cannot tell the owner from anyone else holding the phone; a change to the helmet's setting asks the person to
// confirm that it is for everyone who uses the helmet.

/** One family of protocols, as the owner sees it: a name, how many versions it has and whether it has been used. */
private class FamilyRow(val key: String, val first: NPProtocolEntry, val versions: Int, val used: Boolean, val archived: Boolean)

@Composable
fun WarrantyOwnerScreen(app: AppServices, onDone: () -> Unit, modifier: Modifier = Modifier) {
    val library = app.protocolLibrary
    val gatt = app.gattManager
    var version by remember { mutableIntStateOf(0) } // bump to re-read the (non-observable) library
    val connection by gatt.connectionState.collectAsState()
    val published by gatt.pauseLimitPublished.collectAsState()
    val pauseLimit by gatt.pauseLimitSeconds.collectAsState()
    val pairingRequired by gatt.pairingRequired.collectAsState()

    val offeredLabel = stringResource(Res.string.warranty_owner_offered_switch)

    val families = remember(version) {
        library.allProtocols.filter { it.isRunnable }.groupBy { it.familyKey() }.map { (key, members) ->
            FamilyRow(key, members.first(), members.size, members.any { library.isUsed(it) }, library.isArchived(members.first()))
        }.sortedBy { it.first.name.lowercase() }
    }

    Column(modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp)) {
        Text(stringResource(Res.string.warranty_owner_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))
        Text(stringResource(Res.string.warranty_owner_intro), style = MaterialTheme.typography.bodyMedium)
        Spacer(Modifier.height(24.dp))

        Text(stringResource(Res.string.warranty_owner_offered_title), style = MaterialTheme.typography.titleMedium)
        Text(stringResource(Res.string.warranty_owner_offered_body), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(8.dp))
        if (families.isEmpty()) {
            Text(stringResource(Res.string.warranty_owner_no_protocols))
        } else {
            families.forEach { row ->
                Row(
                    modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(row.first.name, style = MaterialTheme.typography.bodyLarge)
                        val detail = buildList {
                            if (row.versions > 1) add(stringResource(Res.string.warranty_owner_variants, row.versions.toString()))
                            if (row.used) add(stringResource(Res.string.warranty_owner_used))
                        }.joinToString(" · ")
                        if (detail.isNotEmpty()) Text(detail, style = MaterialTheme.typography.bodySmall)
                    }
                    Switch(
                        // Named for a screen reader: the row's own text is a separate element.
                        modifier = Modifier.semantics { contentDescription = "$offeredLabel: ${row.first.name}" },
                        checked = !row.archived,
                        onCheckedChange = { offered ->
                            if (offered) library.unarchiveFamily(row.first) else library.archiveFamily(row.first)
                            version++
                        },
                    )
                }
            }
        }

        Spacer(Modifier.height(24.dp))
        PauseLimitSection(
            published = published,
            connected = connection == ConnectionState.CONNECTED,
            pairingRequired = pairingRequired,
            currentSeconds = pauseLimit,
            onRead = { gatt.requestPauseLimit() },
            onSave = { seconds -> gatt.writePauseLimit(seconds) },
        )

        Spacer(Modifier.height(24.dp))
        OutlinedButton(onClick = onDone) { Text(stringResource(Res.string.common_back)) }
    }
}

@Composable
private fun PauseLimitSection(
    published: Boolean,
    connected: Boolean,
    pairingRequired: Boolean,
    currentSeconds: Long?,
    onRead: () -> Unit,
    onSave: (Long) -> Boolean,
) {
    var minutesText by remember { mutableStateOf("") }
    var invalid by remember { mutableStateOf(false) }
    var confirming by remember { mutableStateOf<Long?>(null) }

    Text(stringResource(Res.string.pause_limit_title), style = MaterialTheme.typography.titleMedium)
    Text(stringResource(Res.string.pause_limit_body), style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))

    when {
        !connected -> Text(stringResource(Res.string.pause_limit_not_connected))
        !published -> Text(stringResource(Res.string.pause_limit_not_published))
        else -> {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    when {
                        currentSeconds == null && pairingRequired ->
                            Text(stringResource(Res.string.pause_limit_not_paired))
                        currentSeconds == null -> {}
                        currentSeconds == PauseLimit.NO_LIMIT_SECONDS ->
                            Text(stringResource(Res.string.pause_limit_no_limit), style = MaterialTheme.typography.titleMedium)
                        else -> Text(
                            // A limit set from elsewhere may not be a whole number of minutes; the figure is rounded up so it
                            // is never shown as shorter than it is.
                            stringResource(Res.string.pause_limit_current, ((currentSeconds + 59L) / 60L).toString()),
                            style = MaterialTheme.typography.titleMedium,
                        )
                    }
                    Spacer(Modifier.height(8.dp))
                    OutlinedButton(onClick = onRead) { Text(stringResource(Res.string.pause_limit_read_again)) }
                }
            }
            Spacer(Modifier.height(12.dp))
            OutlinedTextField(
                value = minutesText,
                onValueChange = { minutesText = it.filter(Char::isDigit); invalid = false },
                label = { Text(stringResource(Res.string.pause_limit_input_label)) },
                isError = invalid,
                supportingText = if (invalid) ({ Text(stringResource(Res.string.pause_limit_invalid)) }) else null,
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            Spacer(Modifier.height(8.dp))
            Button(onClick = {
                val seconds = minutesText.toLongOrNull()?.let { PauseLimit.secondsOfMinutes(it) }
                if (seconds == null) invalid = true else confirming = seconds
            }) { Text(stringResource(Res.string.pause_limit_save)) }
        }
    }

    confirming?.let { seconds ->
        AlertDialog(
            onDismissRequest = { confirming = null },
            title = { Text(stringResource(Res.string.pause_limit_confirm_title)) },
            text = { Text(stringResource(Res.string.pause_limit_confirm_body)) },
            confirmButton = {
                TextButton(onClick = {
                    confirming = null
                    onSave(seconds)
                }) { Text(stringResource(Res.string.pause_limit_save)) }
            },
            dismissButton = {
                TextButton(onClick = { confirming = null }) { Text(stringResource(Res.string.common_cancel)) }
            },
        )
    }
}
