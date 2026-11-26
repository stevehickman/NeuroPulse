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
import androidx.compose.runtime.LaunchedEffect
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
import life.neurone.core.analytics.GeneralShdrContent
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
// The offered set is kept in the owner's store, per helmet, never in the person's archive, which travels with the person's
// protocols (REQ-SHDR-10, OI-PRIV-60). Nothing here says whether a protocol has been run: which protocols the wearer has run is
// the wearer's (CLAUDE.md §5.1), and the owner may be a clinic. Whether a clinic may see it with the wearer's consent is a
// decision not yet made (docs/human-input/oi-priv-60-owner-sees-usage.md).

/** One family of protocols, as the owner sees it: a name, how many versions it has, and whether the owner offers it on this helmet. */
private class FamilyRow(val key: String, val first: NPProtocolEntry, val versions: Int, val offered: Boolean)

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

    // The helmet health reports choice is the connected helmet's, and each helmet has its own (`REQ-SHDR-08`).
    val hub by gatt.hubIdentifier.collectAsState()
    var fleetOn by remember(hub) { mutableStateOf(app.warrantyAnalyticsGate.isOpen(hub)) }
    // REQ-SHDR-12: the owner's general-consent prompt for this helmet, if one is due. It is recorded as put the first time it is
    // shown, so it shows once per owner and helmet, and leaving it unanswered changes nothing. A prompt whose added items this
    // build cannot all name in words is not shown and not recorded (the shipped history is held to the table by a test).
    val prompts = app.generalShdrConsentPrompts
    var prompt by remember(hub) {
        mutableStateOf(prompts.due(hub)?.takeIf { p -> p.added.all { it in GENERAL_CONSENT_ITEM_NAMES } })
    }
    LaunchedEffect(hub, prompt) { prompt?.let { prompts.markShown(hub, it) } }
    // SWR-IND-02: the owner's stealth lock, per helmet like the reports choice.
    var stealthAllowed by remember(hub) { mutableStateOf(!app.stealthForbidden(hub)) }

    val offeredLabel = stringResource(Res.string.warranty_owner_offered_switch)

    val families = remember(version, hub) {
        val withdrawn = app.warrantyOwnerStore.withdrawnFamilies(hub)
        library.allProtocols.filter { it.isRunnable }.groupBy { it.familyKey() }.map { (key, members) ->
            FamilyRow(key, members.first(), members.size, key !in withdrawn)
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
        if (hub == null) {
            // The offered set is a helmet's (SWR-WO-01): with no helmet there is none to show or change.
            Text(stringResource(Res.string.warranty_owner_offered_no_helmet), style = MaterialTheme.typography.bodyMedium)
        } else if (families.isEmpty()) {
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
                        if (row.versions > 1) {
                            Text(stringResource(Res.string.warranty_owner_variants, row.versions.toString()), style = MaterialTheme.typography.bodySmall)
                        }
                    }
                    Switch(
                        // Named for a screen reader: the row's own text is a separate element.
                        modifier = Modifier.semantics { contentDescription = "$offeredLabel: ${row.first.name}" },
                        checked = row.offered,
                        onCheckedChange = { offered ->
                            // The owner's record for this helmet, never the person's archive (OI-PRIV-60).
                            hub?.let { app.warrantyOwnerStore.setOffered(it, row.key, offered) }
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
            helmetConnected = hub != null,
            isOn = fleetOn,
            onGrant = { app.warrantyAnalyticsGate.grant(hub); fleetOn = app.warrantyAnalyticsGate.isOpen(hub) },
            onRevoke = { app.warrantyAnalyticsGate.revoke(hub); fleetOn = app.warrantyAnalyticsGate.isOpen(hub) },
        )

        Spacer(Modifier.height(24.dp))
        StealthLockSection(
            hub = hub,
            allowed = stealthAllowed,
            onChange = { allowed ->
                hub?.let { app.setStealthForbidden(it, !allowed) }
                stealthAllowed = !app.stealthForbidden(hub)
            },
        )

        Spacer(Modifier.height(24.dp))
        OutlinedButton(onClick = onDone) { Text(stringResource(Res.string.common_back)) }
    }

    prompt?.let { p ->
        GeneralConsentPromptDialog(
            prompt = p,
            itemNames = p.added.map { stringResource(GENERAL_CONSENT_ITEM_NAMES.getValue(it)) },
            onAnswer = { answer ->
                prompts.answer(hub, p, answer)
                prompt = null
                fleetOn = app.warrantyAnalyticsGate.isOpen(hub)
            },
        )
    }
}

/**
 * Whether the wearers of this helmet may turn its lights off (`SWR-IND-02`, `OI-PRIV-37`). Allowed by default: stealth is the
 * wearer's choice, made with their profile. An owner who must see the helmet's state (a clinic supervising a session) turns it
 * off, and the app then asks the helmet for its lights on, whatever a profile chose. The hub cannot tell who asks, so the app
 * enforces this; a fault always shows red either way. Kept in the owner's store, per helmet, so with no helmet there is nothing
 * to set.
 */
@Composable
private fun StealthLockSection(hub: String?, allowed: Boolean, onChange: (Boolean) -> Unit) {
    val label = stringResource(Res.string.warranty_owner_stealth_switch)
    Text(stringResource(Res.string.warranty_owner_stealth_title), style = MaterialTheme.typography.titleMedium)
    Text(stringResource(Res.string.warranty_owner_stealth_body), style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))
    if (hub == null) {
        Text(stringResource(Res.string.warranty_owner_stealth_no_helmet), style = MaterialTheme.typography.bodyMedium)
        return
    }
    Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, style = MaterialTheme.typography.bodyLarge, modifier = Modifier.weight(1f))
        Switch(modifier = Modifier.semantics { contentDescription = label }, checked = allowed, onCheckedChange = onChange)
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
private fun FleetReportsSection(helmetConnected: Boolean, isOn: Boolean, onGrant: () -> Unit, onRevoke: () -> Unit) {
    var asking by remember { mutableStateOf(false) }
    var mayDecide by remember { mutableStateOf(false) }

    Text(stringResource(Res.string.warranty_fleet_title), style = MaterialTheme.typography.titleMedium)
    Text(stringResource(Res.string.warranty_fleet_body), style = MaterialTheme.typography.bodySmall)
    Text(stringResource(Res.string.warranty_fleet_per_helmet), style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))
    if (!helmetConnected) {
        // No helmet, no choice to show or make: a choice with no helmet would be a choice for whichever connects next.
        Text(stringResource(Res.string.warranty_fleet_no_helmet), style = MaterialTheme.typography.bodyLarge)
        return
    }
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
                    Spacer(Modifier.height(8.dp))
                    // The grant is recorded for the content set in force (REQ-SHDR-12 (d)), so the dialog says what that covers.
                    Text(stringResource(Res.string.general_consent_baseline))
                    val added = GeneralShdrContent.shipped.fields.mapNotNull { GENERAL_CONSENT_ITEM_NAMES[it] }
                    if (added.isNotEmpty()) {
                        Text(stringResource(Res.string.general_consent_added_heading))
                        added.forEach { Text(stringResource(it), modifier = Modifier.padding(start = 16.dp)) }
                    }
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
