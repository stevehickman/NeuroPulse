package life.neurone.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import life.neurone.app.R
import life.neurone.app.ble.ConnectionState
import life.neurone.core.models.CervicalFaultRecord
import life.neurone.core.models.CervicalFaultStatus
import life.neurone.core.models.CervicalPadStatus
import life.neurone.core.models.PacerPhase
import life.neurone.core.models.SessionState
import life.neurone.core.models.SessionStatus

// Port of iOS SessionView (app/ios/NeurOne/Views/SessionView.swift), Mode 1 display.
// Pure renderer of (connectionState, session): connection banner, status card, live
// metrics (coherence, RMSSD, impedance N/8), breathing pacer, protocol picker entry,
// and a stop control with confirmation. Live data flows once an Android BleCentral is
// wired (OI-AND-BLE-01); every value here comes from GATT-parsed SessionState.

@Composable
fun SessionScreen(
    connectionState: ConnectionState,
    session: SessionState,
    onConnect: () -> Unit,
    onChooseProtocol: () -> Unit,
    onStop: () -> Unit,
    modifier: Modifier = Modifier,
    cervicalPadAlert: CervicalPadStatus? = null,
    onAcknowledgeCervicalPadAlert: () -> Unit = {},
    cervicalFaultStatus: CervicalFaultStatus? = null,
    unacknowledgedCervicalFaults: List<CervicalFaultRecord> = emptyList(),
    onAcknowledgeCervicalFaults: () -> Unit = {},
    cardiacWarningAcknowledged: Boolean = true,
    onAcknowledgeCardiacWarning: () -> Unit = {},
    /** Sends the re-enable confirmation; false when the hub is not awaiting one. */
    onConfirmCervicalResume: () -> Boolean = { false },
) {
    var showStopConfirm by remember { mutableStateOf(false) }
    val isRunning = session.status == SessionStatus.RUNNING

    Column(
        modifier = modifier
            .fillMaxWidth()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
    ) {
        Text(stringResource(R.string.session_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(12.dp))

        ConnectionBanner(connectionState)
        Spacer(Modifier.height(16.dp))

        if (connectionState == ConnectionState.CONNECTED) {
            SessionStatusCard(session.status)
            Spacer(Modifier.height(16.dp))

            if (cervicalFaultStatus?.reenableState == CervicalFaultStatus.ReenableState.AWAIT_CONFIRM) {
                CervicalResumeCard(onConfirmCervicalResume)
                Spacer(Modifier.height(16.dp))
            }

            if (isRunning) {
                LiveMetricsGrid(session)
                Spacer(Modifier.height(16.dp))
                BreathingPacer(session.pacerPhase, session.pacerElapsedPercent)
                Spacer(Modifier.height(16.dp))
                Button(
                    onClick = { showStopConfirm = true },
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFD32F2F)),
                ) { Text(stringResource(R.string.session_end_button)) }
            } else {
                Button(onClick = onChooseProtocol, modifier = Modifier.fillMaxWidth()) {
                    Text(stringResource(R.string.session_choose_protocol_button))
                }
            }
        } else {
            Text(stringResource(R.string.ble_enable_bluetooth))
            Spacer(Modifier.height(12.dp))
            val connecting = connectionState == ConnectionState.SCANNING ||
                connectionState == ConnectionState.CONNECTING
            Button(
                onClick = onConnect,
                modifier = Modifier.fillMaxWidth(),
                enabled = !connecting,
            ) { Text(if (connecting) stringResource(R.string.session_conn_connecting) else stringResource(R.string.session_connect_to_hub)) }
            Spacer(Modifier.height(8.dp))
            // Browsing protocols is allowed while disconnected — each row shows its own
            // availability ("No device connected." / "Requires: …" / EEG-consent message).
            OutlinedButton(
                onClick = onChooseProtocol,
                modifier = Modifier.fillMaxWidth(),
            ) { Text(stringResource(R.string.session_browse_protocols)) }
        }

        Spacer(Modifier.height(24.dp))
        Text(
            stringResource(R.string.regulatory_footer),
            style = MaterialTheme.typography.bodySmall,
        )
    }

    if (showStopConfirm) {
        AlertDialog(
            onDismissRequest = { showStopConfirm = false },
            title = { Text(stringResource(R.string.session_stop_confirm_title)) },
            text = { Text(stringResource(R.string.session_stimulation_will_ramp_down_and_stop)) },
            confirmButton = {
                TextButton(onClick = {
                    showStopConfirm = false
                    onStop()
                }) { Text(stringResource(R.string.session_end_button)) }
            },
            dismissButton = {
                TextButton(onClick = { showStopConfirm = false }) { Text(stringResource(R.string.common_cancel)) }
            },
        )
    }

    // OI-ACC-07: the hub refused or stopped cervical VNS on a failed gel pad. The safety MCU
    // has already acted; this only tells the wearer which pad, and where.
    val padMessage = cervicalPadAlert?.let { cervicalPadMessage(it) }
    if (cervicalPadAlert != null && padMessage != null) {
        AlertDialog(
            onDismissRequest = onAcknowledgeCervicalPadAlert,
            title = { Text(stringResource(R.string.cvns_pad_alert_title)) },
            text = {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    NeckPadDiagram(cervicalPadAlert.failingSides)
                    Spacer(Modifier.height(16.dp))
                    Text(padMessage)
                }
            },
            confirmButton = {
                TextButton(onClick = onAcknowledgeCervicalPadAlert) { Text(stringResource(R.string.common_ok)) }
            },
        )
    }

    // NP-SW-FAULTMSG-001 P4: faults from sessions that ran without the app. No dismiss on
    // outside tap — the wearer must read it; acknowledging releases nothing on the device.
    if (unacknowledgedCervicalFaults.isNotEmpty()) {
        AlertDialog(
            onDismissRequest = { },
            title = { Text(stringResource(R.string.cvns_fault_summary_title)) },
            text = {
                Column {
                    Text(stringResource(R.string.cvns_fault_summary_intro))
                    unacknowledgedCervicalFaults.forEach { fault ->
                        Spacer(Modifier.height(12.dp))
                        Text(cervicalFaultMessage(fault))
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = onAcknowledgeCervicalFaults) {
                    Text(stringResource(R.string.cvns_fault_acknowledge))
                }
            },
        )
    } else if (cervicalFaultStatus?.outstanding == true && !cardiacWarningAcknowledged) {
        // Per-user cardiac scope: someone on this device has an outstanding cardiac cutoff.
        // Everyone who connects sees this once per connection — it never says who — so
        // switching profiles cannot hide a block.
        AlertDialog(
            onDismissRequest = onAcknowledgeCardiacWarning,
            title = { Text(stringResource(R.string.cvns_blanket_warning_title)) },
            text = { Text(stringResource(R.string.cvns_blanket_warning_body)) },
            confirmButton = {
                TextButton(onClick = onAcknowledgeCardiacWarning) { Text(stringResource(R.string.common_ok)) }
            },
        )
    }
}

@Composable
private fun cervicalFaultMessage(fault: CervicalFaultRecord): String = when (fault.message) {
    CervicalFaultRecord.Message.HR_CHANGE -> stringResource(R.string.cvns_fault_hr_change)
    CervicalFaultRecord.Message.SIGNAL_LOST -> stringResource(R.string.cvns_fault_signal_lost)
    CervicalFaultRecord.Message.DEVICE -> stringResource(R.string.cvns_fault_device)
    CervicalFaultRecord.Message.PAD_LEFT -> stringResource(R.string.cvns_fault_pad_left)
    CervicalFaultRecord.Message.PAD_RIGHT -> stringResource(R.string.cvns_fault_pad_right)
    CervicalFaultRecord.Message.PAD_BOTH -> stringResource(R.string.cvns_fault_pad_both)
}

/**
 * Shown while the hub is waiting for the wearer to confirm resuming cervical stimulation after
 * a cardiac cutoff. Confirming sends the confirmation the hub requires (REQ-CVNS-09); the hub
 * then re-checks the pads, and only then does the safety MCU clear the cutoff. Mirrors iOS
 * CervicalResumeCard.
 */
@Composable
private fun CervicalResumeCard(onConfirm: () -> Boolean) {
    var askConfirm by remember { mutableStateOf(false) }
    var sent by remember { mutableStateOf<Boolean?>(null) }
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(
                stringResource(R.string.cvns_resume_title),
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.error,
            )
            Spacer(Modifier.height(8.dp))
            Text(stringResource(R.string.cvns_resume_body))
            sent?.let {
                Spacer(Modifier.height(8.dp))
                Text(
                    stringResource(if (it) R.string.cvns_resume_sent else R.string.cvns_resume_rejected),
                    style = MaterialTheme.typography.bodySmall,
                )
            }
            Spacer(Modifier.height(8.dp))
            OutlinedButton(onClick = { askConfirm = true }) { Text(stringResource(R.string.cvns_resume_button)) }
        }
    }
    if (askConfirm) {
        AlertDialog(
            onDismissRequest = { askConfirm = false },
            title = { Text(stringResource(R.string.cvns_resume_confirm_title)) },
            text = { Text(stringResource(R.string.cvns_resume_confirm_body)) },
            confirmButton = {
                TextButton(onClick = {
                    askConfirm = false
                    sent = onConfirm()
                }) { Text(stringResource(R.string.cvns_resume_button)) }
            },
            dismissButton = {
                TextButton(onClick = { askConfirm = false }) { Text(stringResource(R.string.common_cancel)) }
            },
        )
    }
}

@Composable
private fun cervicalPadMessage(status: CervicalPadStatus): String? = when (status.message) {
    CervicalPadStatus.Message.PRE_LEFT -> stringResource(R.string.cvns_pad_alert_pre_left)
    CervicalPadStatus.Message.PRE_RIGHT -> stringResource(R.string.cvns_pad_alert_pre_right)
    CervicalPadStatus.Message.PRE_BOTH -> stringResource(R.string.cvns_pad_alert_pre_both)
    CervicalPadStatus.Message.MID_LEFT -> stringResource(R.string.cvns_pad_alert_mid_left)
    CervicalPadStatus.Message.MID_RIGHT -> stringResource(R.string.cvns_pad_alert_mid_right)
    CervicalPadStatus.Message.MID_BOTH -> stringResource(R.string.cvns_pad_alert_mid_both)
    null -> null
}

/**
 * Front view of head and neck, drawn as the wearer sees themself in a mirror: their left side
 * is on the left of the screen. Each side is labelled in words as well, so the diagram never
 * depends on the reader guessing the view. One pad marker per side; a side lights when the hub
 * reports a failing pad there. The pads sit on the neck module, not in a helmet socket.
 * Mirrors iOS NeckPadDiagram. Decorative for TalkBack — the message names the side in words.
 */
@Composable
private fun NeckPadDiagram(failingSides: Set<CervicalPadStatus.NeckSide>) {
    val outline = MaterialTheme.colorScheme.onSurfaceVariant
    val alert = MaterialTheme.colorScheme.error
    val leftFailing = CervicalPadStatus.NeckSide.LEFT in failingSides
    val rightFailing = CervicalPadStatus.NeckSide.RIGHT in failingSides
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier.clearAndSetSemantics { },
    ) {
        Canvas(modifier = Modifier.size(width = 160.dp, height = 140.dp)) {
            val cx = size.width / 2
            val stroke = Stroke(width = 2.dp.toPx())
            drawCircle(outline, radius = 36.dp.toPx(), center = Offset(cx, 34.dp.toPx()), style = stroke)
            drawRoundRect(
                outline,
                topLeft = Offset(cx - 22.dp.toPx(), 70.dp.toPx()),
                size = Size(44.dp.toPx(), 56.dp.toPx()),
                cornerRadius = CornerRadius(8.dp.toPx()),
                style = stroke,
            )
            fun pad(x: Float, failing: Boolean) {
                val topLeft = Offset(x - 7.dp.toPx(), 73.dp.toPx())
                val padSize = Size(14.dp.toPx(), 22.dp.toPx())
                val radius = CornerRadius(4.dp.toPx())
                if (failing) drawRoundRect(alert, topLeft, padSize, radius)
                drawRoundRect(if (failing) alert else outline, topLeft, padSize, radius, style = stroke)
            }
            pad(cx - 26.dp.toPx(), leftFailing)
            pad(cx + 26.dp.toPx(), rightFailing)
        }
        Row(modifier = Modifier.width(150.dp)) {
            Text(
                stringResource(R.string.cvns_pad_side_left),
                color = if (leftFailing) alert else outline,
                fontWeight = if (leftFailing) FontWeight.Bold else FontWeight.Normal,
                style = MaterialTheme.typography.labelMedium,
            )
            Spacer(Modifier.weight(1f))
            Text(
                stringResource(R.string.cvns_pad_side_right),
                color = if (rightFailing) alert else outline,
                fontWeight = if (rightFailing) FontWeight.Bold else FontWeight.Normal,
                style = MaterialTheme.typography.labelMedium,
            )
        }
        Text(
            stringResource(R.string.cvns_pad_diagram_caption),
            color = outline,
            style = MaterialTheme.typography.labelSmall,
        )
    }
}

@Composable
private fun ConnectionBanner(state: ConnectionState) {
    val (label, color) = when (state) {
        ConnectionState.CONNECTED -> stringResource(R.string.session_connected_to_hub) to Color(0xFF2E7D32)
        ConnectionState.CONNECTING -> stringResource(R.string.session_conn_connecting) to Color(0xFFF9A825)
        ConnectionState.SCANNING -> stringResource(R.string.session_conn_searching) to Color(0xFFF9A825)
        ConnectionState.DISCONNECTED -> stringResource(R.string.session_not_connected) to Color(0xFF757575)
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(MaterialTheme.shapes.small)
            .background(color.copy(alpha = 0.15f))
            .padding(12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Spacer(
            Modifier
                .height(10.dp)
                .padding(end = 8.dp)
                .clip(CircleShape)
                .background(color),
        )
        Text(label, color = color, fontWeight = FontWeight.Medium)
    }
}

@Composable
private fun SessionStatusCard(status: SessionStatus) {
    val label = when (status) {
        SessionStatus.IDLE -> stringResource(R.string.session_idle_ready_to_start)
        SessionStatus.RUNNING -> stringResource(R.string.session_session_running)
        SessionStatus.PAUSED -> stringResource(R.string.session_status_paused)
        SessionStatus.COMPLETED -> stringResource(R.string.session_session_complete)
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text(stringResource(R.string.session_status), style = MaterialTheme.typography.labelMedium)
            Text(label, style = MaterialTheme.typography.titleMedium)
        }
    }
}

@Composable
private fun LiveMetricsGrid(session: SessionState) {
    val coherence = session.hrv?.coherenceScore
    val rmssd = session.hrv?.rmssdMilliseconds
    val impedancePass = (0 until 8).count { (session.impedancePassFlags and (1 shl it)) != 0 }

    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text(stringResource(R.string.session_live_metrics), style = MaterialTheme.typography.labelMedium)
            Spacer(Modifier.height(8.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Metric(stringResource(R.string.session_metric_coherence), coherence?.let { "%.1f".format(it) } ?: "—", coherenceColor(coherence))
                Metric(stringResource(R.string.session_metric_rmssd), rmssd?.let { "$it ms" } ?: "—", MaterialTheme.colorScheme.onSurface)
                Metric(stringResource(R.string.session_impedance), "$impedancePass/8", MaterialTheme.colorScheme.onSurface)
            }
        }
    }
}

@Composable
private fun Metric(label: String, value: String, valueColor: Color) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(value, style = MaterialTheme.typography.titleLarge, color = valueColor)
        Text(label, style = MaterialTheme.typography.labelSmall)
    }
}

@Composable
private fun BreathingPacer(phase: PacerPhase, elapsedPercent: Int) {
    val label = if (phase == PacerPhase.INHALE) stringResource(R.string.session_breathe_in) else stringResource(R.string.session_breathe_out)
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text(label, style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.height(8.dp))
            LinearProgressIndicator(
                progress = { (elapsedPercent.coerceIn(0, 100)) / 100f },
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}

private fun coherenceColor(score: Float?): Color = when {
    score == null -> Color(0xFF757575)
    score >= 7f -> Color(0xFF2E7D32)
    score >= 4f -> Color(0xFFF9A825)
    else -> Color(0xFFEF6C00)
}
