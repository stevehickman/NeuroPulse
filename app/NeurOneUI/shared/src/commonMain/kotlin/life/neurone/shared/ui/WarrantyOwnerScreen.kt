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
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
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
import life.neurone.core.models.CardiacSensitivity
import life.neurone.core.models.PauseLimit
import life.neurone.core.models.PpxConfig
import life.neurone.core.models.PpxEvidence
import life.neurone.core.models.PpxSensitivity
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.familyKey
import life.neurone.core.protocol.isRunnable
import life.neurone.core.protocol.name
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
    val ppxPublished by gatt.ppxConfigPublished.collectAsState()
    val ppxConfig by gatt.ppxConfig.collectAsState()
    val cardiacPublished by gatt.cardiacSensitivityPublished.collectAsState()
    val cardiacSensitivity by gatt.cardiacSensitivity.collectAsState()
    val sensitivityPublished by gatt.ppxSensitivityPublished.collectAsState()
    val sensitivity by gatt.ppxSensitivity.collectAsState()

    var fleetOn by remember { mutableStateOf(app.warrantyAnalyticsGate.isOpen) }

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
        PpxConfigSection(
            published = ppxPublished,
            connected = connection == ConnectionState.CONNECTED,
            pairingRequired = pairingRequired,
            config = ppxConfig,
            onRead = { gatt.requestPpxConfig() },
            onSave = { stop, override -> gatt.writePpxConfig(stop, override) },
        )

        Spacer(Modifier.height(24.dp))
        PpxSensitivitySection(
            published = sensitivityPublished,
            connected = connection == ConnectionState.CONNECTED,
            pairingRequired = pairingRequired,
            sensitivity = sensitivity,
            onRead = { gatt.requestPpxSensitivity() },
            onSave = { level -> gatt.writePpxSensitivity(level) },
        )

        Spacer(Modifier.height(24.dp))
        CardiacSensitivitySection(
            published = cardiacPublished,
            connected = connection == ConnectionState.CONNECTED,
            pairingRequired = pairingRequired,
            sensitivity = cardiacSensitivity,
            onRead = { gatt.requestCardiacSensitivity() },
            onSave = { level -> gatt.writeCardiacSensitivity(level) },
        )

        Spacer(Modifier.height(24.dp))
        FleetReportsSection(
            isOn = fleetOn,
            onGrant = { app.warrantyAnalyticsGate.grant(); fleetOn = app.warrantyAnalyticsGate.isOpen },
            onRevoke = { app.warrantyAnalyticsGate.revoke(); fleetOn = app.warrantyAnalyticsGate.isOpen },
        )

        Spacer(Modifier.height(24.dp))
        OutlinedButton(onClick = onDone) { Text(stringResource(Res.string.common_back)) }
    }
}

/**
 * The warranty owner's choice to let the helmet send its health reports to the fleet service (CLAUDE.md §6.0: the consent
 * subject is the warranty owner, not the wearer, and this gate is independent of every research choice). `NP-INFRA-001` §3.3a
 * requires the notice before the first upload: the service sees the connection's network address while it receives the
 * record, does not keep it, and the record carries a device token and no name. Turning on is therefore a dialog that says so
 * and asks the person to confirm they may decide this for the helmet; turning off needs no dialog and takes effect at once.
 */
@Composable
private fun FleetReportsSection(isOn: Boolean, onGrant: () -> Unit, onRevoke: () -> Unit) {
    var asking by remember { mutableStateOf(false) }
    var mayDecide by remember { mutableStateOf(false) }

    Text(stringResource(Res.string.warranty_fleet_title), style = MaterialTheme.typography.titleMedium)
    Text(stringResource(Res.string.warranty_fleet_body), style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))
    Text(
        stringResource(if (isOn) Res.string.warranty_fleet_status_on else Res.string.warranty_fleet_status_off),
        style = MaterialTheme.typography.bodyLarge,
    )
    Spacer(Modifier.height(8.dp))
    if (isOn) {
        OutlinedButton(onClick = onRevoke) { Text(stringResource(Res.string.warranty_fleet_turn_off)) }
    } else {
        Button(onClick = { mayDecide = false; asking = true }) { Text(stringResource(Res.string.warranty_fleet_turn_on)) }
    }

    if (asking) {
        AlertDialog(
            onDismissRequest = { asking = false },
            title = { Text(stringResource(Res.string.warranty_fleet_notice_title)) },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    Text(stringResource(Res.string.warranty_fleet_notice_body))
                    Spacer(Modifier.height(12.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(checked = mayDecide, onCheckedChange = { mayDecide = it })
                        Text(stringResource(Res.string.warranty_fleet_notice_confirm), style = MaterialTheme.typography.bodyMedium)
                    }
                }
            },
            confirmButton = {
                TextButton(enabled = mayDecide, onClick = { asking = false; onGrant() }) {
                    Text(stringResource(Res.string.warranty_fleet_notice_accept))
                }
            },
            dismissButton = { TextButton(onClick = { asking = false }) { Text(stringResource(Res.string.common_cancel)) } },
        )
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

/**
 * The seizure-response detector's two switches (REQ-FWHUB-71). What is shown is what the helmet holds, read back after every
 * write; a change is confirmed first because it is for everyone who uses the helmet (the screen cannot tell the owner from
 * anyone else holding the phone). The override cannot be turned on while the detector has no validation, and the helmet
 * refuses it too, so it is never set ahead of the evidence.
 */
@Composable
private fun PpxConfigSection(
    published: Boolean,
    connected: Boolean,
    pairingRequired: Boolean,
    config: PpxConfig?,
    onRead: () -> Unit,
    onSave: (stopOnOccipitalOnly: Boolean, manualOverride: Boolean) -> Boolean,
) {
    var confirming by remember { mutableStateOf<Pair<Boolean, Boolean>?>(null) }
    val stopLabel = stringResource(Res.string.ppx_stop_occipital_label)
    val overrideLabel = stringResource(Res.string.ppx_override_label)

    Text(stringResource(Res.string.ppx_title), style = MaterialTheme.typography.titleMedium)
    Text(stringResource(Res.string.ppx_body), style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))

    when {
        !connected -> Text(stringResource(Res.string.ppx_not_connected))
        !published -> Text(stringResource(Res.string.ppx_not_published))
        config == null -> {
            if (pairingRequired) Text(stringResource(Res.string.ppx_not_paired))
            OutlinedButton(onClick = onRead) { Text(stringResource(Res.string.ppx_read_again)) }
        }
        else -> {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text(
                        stringResource(
                            when (config.evidence) {
                                PpxEvidence.NONE -> Res.string.ppx_evidence_none
                                PpxEvidence.BENCH -> Res.string.ppx_evidence_bench
                                PpxEvidence.CLINICAL -> Res.string.ppx_evidence_clinical
                            },
                        ),
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    Spacer(Modifier.height(12.dp))
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
                        Column(Modifier.weight(1f)) {
                            Text(stopLabel, style = MaterialTheme.typography.bodyLarge)
                            Text(stringResource(Res.string.ppx_stop_occipital_hint), style = MaterialTheme.typography.bodySmall)
                        }
                        Switch(
                            modifier = Modifier.semantics { contentDescription = stopLabel },
                            checked = config.stopOnOccipitalOnly,
                            onCheckedChange = { confirming = it to config.manualOverride },
                        )
                    }
                    Spacer(Modifier.height(12.dp))
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
                        Column(Modifier.weight(1f)) {
                            Text(overrideLabel, style = MaterialTheme.typography.bodyLarge)
                            Text(stringResource(Res.string.ppx_override_hint), style = MaterialTheme.typography.bodySmall)
                        }
                        Switch(
                            modifier = Modifier.semantics { contentDescription = overrideLabel },
                            checked = config.manualOverride,
                            // It may always be turned off; it may be turned on only once the detector has some validation.
                            enabled = config.manualOverride || config.evidence != PpxEvidence.NONE,
                            onCheckedChange = { confirming = config.stopOnOccipitalOnly to it },
                        )
                    }
                    Spacer(Modifier.height(8.dp))
                    OutlinedButton(onClick = onRead) { Text(stringResource(Res.string.ppx_read_again)) }
                }
            }
        }
    }

    confirming?.let { (stop, override) ->
        AlertDialog(
            onDismissRequest = { confirming = null },
            title = { Text(stringResource(Res.string.ppx_confirm_title)) },
            text = {
                Column {
                    Text(stringResource(Res.string.ppx_confirm_body))
                    if (config != null && config.stopOnOccipitalOnly && !stop) {
                        Spacer(Modifier.height(8.dp))
                        Text(stringResource(Res.string.ppx_confirm_stop_off_body))
                    }
                    if (config != null && !config.manualOverride && override) {
                        Spacer(Modifier.height(8.dp))
                        Text(stringResource(Res.string.ppx_confirm_override_body))
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    confirming = null
                    onSave(stop, override)
                }) { Text(stringResource(Res.string.ppx_save)) }
            },
            dismissButton = {
                TextButton(onClick = { confirming = null }) { Text(stringResource(Res.string.common_cancel)) }
            },
        )
    }
}

/**
 * The detector's sensitivity (REQ-FWHUB-68): a slider from the standard (validated) setting up to the most sensitive the helmet
 * offers, the range read from the helmet. There is nothing below standard to choose, by design. A change is confirmed, because it
 * is for everyone who uses the helmet and because more sensitive stops harmless sessions too.
 */
@Composable
private fun PpxSensitivitySection(
    published: Boolean,
    connected: Boolean,
    pairingRequired: Boolean,
    sensitivity: PpxSensitivity?,
    onRead: () -> Unit,
    onSave: (Int) -> Boolean,
) {
    var confirming by remember { mutableStateOf<Int?>(null) }
    var dragging by remember { mutableStateOf<Float?>(null) }
    val description = stringResource(Res.string.ppx_sens_slider_description)

    Text(stringResource(Res.string.ppx_sens_title), style = MaterialTheme.typography.titleMedium)
    Text(stringResource(Res.string.ppx_sens_body), style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))

    when {
        !connected -> Text(stringResource(Res.string.ppx_not_connected))
        !published -> Text(stringResource(Res.string.ppx_not_published))
        sensitivity == null -> {
            if (pairingRequired) Text(stringResource(Res.string.ppx_not_paired))
            OutlinedButton(onClick = onRead) { Text(stringResource(Res.string.ppx_read_again)) }
        }
        else -> {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    val shown = dragging?.toInt() ?: sensitivity.level
                    Text(
                        if (shown == 0) {
                            stringResource(Res.string.ppx_sens_standard)
                        } else {
                            stringResource(Res.string.ppx_sens_level, shown.toString(), sensitivity.maxLevel.toString())
                        },
                        style = MaterialTheme.typography.titleMedium,
                    )
                    if (sensitivity.maxLevel > 0) {
                        Slider(
                            value = dragging ?: sensitivity.level.toFloat(),
                            onValueChange = { dragging = it },
                            valueRange = 0f..sensitivity.maxLevel.toFloat(),
                            steps = (sensitivity.maxLevel - 1).coerceAtLeast(0),
                            onValueChangeFinished = {
                                val picked = dragging?.toInt()
                                dragging = null
                                if (picked != null && picked != sensitivity.level) confirming = picked
                            },
                            modifier = Modifier.semantics { contentDescription = description },
                        )
                    }
                    Spacer(Modifier.height(8.dp))
                    OutlinedButton(onClick = onRead) { Text(stringResource(Res.string.ppx_read_again)) }
                }
            }
        }
    }

    confirming?.let { level ->
        AlertDialog(
            onDismissRequest = { confirming = null },
            title = { Text(stringResource(Res.string.ppx_confirm_title)) },
            text = {
                Column {
                    Text(stringResource(Res.string.ppx_confirm_body))
                    Spacer(Modifier.height(8.dp))
                    val more = sensitivity != null && level > sensitivity.level
                    Text(stringResource(if (more) Res.string.ppx_sens_confirm_more else Res.string.ppx_sens_confirm_less))
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    confirming = null
                    onSave(level)
                }) { Text(stringResource(Res.string.ppx_save)) }
            },
            dismissButton = {
                TextButton(onClick = { confirming = null }) { Text(stringResource(Res.string.common_cancel)) }
            },
        )
    }
}

/**
 * The cervical VNS cardiac cutoff's sensitivity (REQ-FWHUB-74, REQ-CVNS-12): a slider from the standard (validated) cutoff up to the
 * most sensitive the helmet offers, the range read from the helmet. There is nothing below standard to choose, by design: a less
 * sensitive cutoff would weaken a hazard control. A change is confirmed, because it is for everyone who uses the helmet and because
 * more sensitive also stops sessions for harmless changes.
 */
@Composable
private fun CardiacSensitivitySection(
    published: Boolean,
    connected: Boolean,
    pairingRequired: Boolean,
    sensitivity: CardiacSensitivity?,
    onRead: () -> Unit,
    onSave: (Int) -> Boolean,
) {
    var confirming by remember { mutableStateOf<Int?>(null) }
    var dragging by remember { mutableStateOf<Float?>(null) }
    val description = stringResource(Res.string.cardiac_sens_slider_description)

    Text(stringResource(Res.string.cardiac_sens_title), style = MaterialTheme.typography.titleMedium)
    Text(stringResource(Res.string.cardiac_sens_body), style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))

    when {
        !connected -> Text(stringResource(Res.string.ppx_not_connected))
        !published -> Text(stringResource(Res.string.ppx_not_published))
        sensitivity == null -> {
            if (pairingRequired) Text(stringResource(Res.string.ppx_not_paired))
            OutlinedButton(onClick = onRead) { Text(stringResource(Res.string.ppx_read_again)) }
        }
        else -> {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    val shown = dragging?.toInt() ?: sensitivity.level
                    Text(
                        if (shown == 0) {
                            stringResource(Res.string.cardiac_sens_standard)
                        } else {
                            stringResource(Res.string.cardiac_sens_level, shown.toString(), sensitivity.maxLevel.toString())
                        },
                        style = MaterialTheme.typography.titleMedium,
                    )
                    if (sensitivity.maxLevel > 0) {
                        Slider(
                            value = dragging ?: sensitivity.level.toFloat(),
                            onValueChange = { dragging = it },
                            valueRange = 0f..sensitivity.maxLevel.toFloat(),
                            steps = (sensitivity.maxLevel - 1).coerceAtLeast(0),
                            onValueChangeFinished = {
                                val picked = dragging?.toInt()
                                dragging = null
                                if (picked != null && picked != sensitivity.level) confirming = picked
                            },
                            modifier = Modifier.semantics { contentDescription = description },
                        )
                    }
                    Spacer(Modifier.height(8.dp))
                    OutlinedButton(onClick = onRead) { Text(stringResource(Res.string.ppx_read_again)) }
                }
            }
        }
    }

    confirming?.let { level ->
        AlertDialog(
            onDismissRequest = { confirming = null },
            title = { Text(stringResource(Res.string.ppx_confirm_title)) },
            text = {
                Column {
                    Text(stringResource(Res.string.ppx_confirm_body))
                    Spacer(Modifier.height(8.dp))
                    val more = sensitivity != null && level > sensitivity.level
                    Text(stringResource(if (more) Res.string.cardiac_sens_confirm_more else Res.string.cardiac_sens_confirm_less))
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    confirming = null
                    onSave(level)
                }) { Text(stringResource(Res.string.ppx_save)) }
            },
            dismissButton = {
                TextButton(onClick = { confirming = null }) { Text(stringResource(Res.string.common_cancel)) }
            },
        )
    }
}
