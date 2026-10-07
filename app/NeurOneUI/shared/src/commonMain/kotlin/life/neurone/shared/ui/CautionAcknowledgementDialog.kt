package life.neurone.shared.ui

import org.jetbrains.compose.resources.StringResource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.toMutableStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import org.jetbrains.compose.resources.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.selection.toggleable
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.*
import life.neurone.core.protocol.NPZoneCaution
import kotlin.math.roundToLong

/**
 * The acknowledgement screen for a protocol in the zone model's caution zone (docs/reference/safety-zones.md).
 *
 * A caution is not a refusal: the compiler runs the protocol only when the request lists the id of every caution it
 * is in. Each caution is its own checkbox, because an id names one dose and an acknowledgement is of that dose, not of
 * the protocol. Nothing is stored: the ids go to [onAcknowledge] for one compile and are dropped (an acknowledgement
 * is a decision about the person, CLAUDE.md §5).
 */
@Composable
fun CautionAcknowledgementDialog(
    protocolName: String,
    cautions: List<NPZoneCaution>,
    onAcknowledge: (List<String>) -> Unit,
    onCancel: () -> Unit,
) {
    // Ticks survive rotation but not a different set of cautions, whose ids differ.
    val ticked = rememberSaveable(cautions.map { it.ackId }, saver = androidx.compose.runtime.saveable.listSaver(
        save = { it.toList() }, restore = { it.toMutableStateList() },
    )) { mutableListOf<String>().toMutableStateList() }
    val all = cautions.isNotEmpty() && cautions.all { it.ackId in ticked }

    AlertDialog(
        // Tapping outside is a cancel, not an acknowledgement.
        onDismissRequest = onCancel,
        title = { Text(stringResource(Res.string.zone_ack_title)) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(stringResource(Res.string.zone_ack_intro, protocolName), style = MaterialTheme.typography.bodyMedium)
                cautions.forEach { c ->
                    val on = c.ackId in ticked
                    Row(
                        Modifier.fillMaxWidth().toggleable(
                            value = on,
                            role = Role.Checkbox,
                            onValueChange = { if (it) ticked.add(c.ackId) else ticked.remove(c.ackId) },
                        ),
                        verticalAlignment = Alignment.Top,
                    ) {
                        Checkbox(checked = on, onCheckedChange = null)
                        Column(Modifier.padding(start = 12.dp)) {
                            val modality = c.modality?.let { stringResource(modalityNameRes(it)) } ?: ""
                            Text(
                                stringResource(Res.string.zone_ack_item_heading, modality, stringResource(axisNameRes(c.axisNameKey))),
                                fontWeight = FontWeight.SemiBold,
                            )
                            Text(stringResource(Res.string.zone_ack_item_dose, shown(c.value), c.unit, shown(c.caution)))
                            Text(stringResource(Res.string.zone_ack_check_label), style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
                Text(stringResource(Res.string.zone_ack_progress, ticked.size.toString(), cautions.size.toString()),
                    style = MaterialTheme.typography.labelMedium)
                Text(stringResource(Res.string.zone_ack_safety_note), style = MaterialTheme.typography.bodySmall)
                Text(stringResource(Res.string.zone_ack_reask_note), style = MaterialTheme.typography.bodySmall)
                Text(stringResource(Res.string.zone_ack_local_note), style = MaterialTheme.typography.bodySmall)
            }
        },
        confirmButton = {
            TextButton(enabled = all, onClick = { onAcknowledge(cautions.map { it.ackId }) }) {
                Text(stringResource(Res.string.zone_ack_confirm))
            }
        },
        dismissButton = { TextButton(onClick = onCancel) { Text(stringResource(Res.string.zone_ack_cancel)) } },
    )
}

/** The zone model's axis names (`ZONE_AXIS_…`, from the core). */
internal fun axisNameRes(key: String): StringResource = when (key) {
    "ZONE_AXIS_PHASE_CHARGE_DENSITY" -> Res.string.zone_axis_phase_charge_density
    "ZONE_AXIS_SHANNON_K" -> Res.string.zone_axis_shannon_k
    "ZONE_AXIS_RMS_CURRENT_DENSITY" -> Res.string.zone_axis_rms_current_density
    "ZONE_AXIS_MEAN_CURRENT_DENSITY" -> Res.string.zone_axis_mean_current_density
    "ZONE_AXIS_SESSION_CHARGE_DENSITY" -> Res.string.zone_axis_session_charge_density
    else -> throw IllegalArgumentException(key)
}

/** Rounded for display only; the id keeps the exact dose. */
internal fun shown(n: Double): String {
    val r = (n * 100).roundToLong() / 100.0
    return if (r == r.toLong().toDouble()) r.toLong().toString() else r.toString()
}
