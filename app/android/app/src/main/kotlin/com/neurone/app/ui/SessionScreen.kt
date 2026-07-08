package com.neurone.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.neurone.app.R
import com.neurone.app.ble.ConnectionState
import com.neurone.core.models.PacerPhase
import com.neurone.core.models.SessionState
import com.neurone.core.models.SessionStatus

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
) {
    var showStopConfirm by remember { mutableStateOf(false) }
    val isRunning = session.status == SessionStatus.RUNNING

    Column(
        modifier = modifier
            .fillMaxWidth()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
    ) {
        Text("Session", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(12.dp))

        ConnectionBanner(connectionState)
        Spacer(Modifier.height(16.dp))

        if (connectionState == ConnectionState.CONNECTED) {
            SessionStatusCard(session.status)
            Spacer(Modifier.height(16.dp))

            if (isRunning) {
                LiveMetricsGrid(session)
                Spacer(Modifier.height(16.dp))
                BreathingPacer(session.pacerPhase, session.pacerElapsedPercent)
                Spacer(Modifier.height(16.dp))
                Button(
                    onClick = { showStopConfirm = true },
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFD32F2F)),
                ) { Text("End Session") }
            } else {
                Button(onClick = onChooseProtocol, modifier = Modifier.fillMaxWidth()) {
                    Text("Choose Protocol")
                }
            }
        } else {
            Text(stringResource(R.string.bluetooth_off_message))
            Spacer(Modifier.height(12.dp))
            val connecting = connectionState == ConnectionState.SCANNING ||
                connectionState == ConnectionState.CONNECTING
            Button(
                onClick = onConnect,
                modifier = Modifier.fillMaxWidth(),
                enabled = !connecting,
            ) { Text(if (connecting) "Connecting…" else "Connect to Hub") }
            Spacer(Modifier.height(8.dp))
            // Browsing protocols is allowed while disconnected — each row shows its own
            // availability ("No device connected." / "Requires: …" / EEG-consent message).
            OutlinedButton(
                onClick = onChooseProtocol,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Browse Protocols") }
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
            title = { Text("End this session?") },
            text = { Text("Stimulation will ramp down and stop.") },
            confirmButton = {
                TextButton(onClick = {
                    showStopConfirm = false
                    onStop()
                }) { Text("End Session") }
            },
            dismissButton = {
                TextButton(onClick = { showStopConfirm = false }) { Text("Cancel") }
            },
        )
    }
}

@Composable
private fun ConnectionBanner(state: ConnectionState) {
    val (label, color) = when (state) {
        ConnectionState.CONNECTED -> "Connected to hub" to Color(0xFF2E7D32)
        ConnectionState.CONNECTING -> "Connecting…" to Color(0xFFF9A825)
        ConnectionState.SCANNING -> "Searching for hub…" to Color(0xFFF9A825)
        ConnectionState.DISCONNECTED -> "Not connected" to Color(0xFF757575)
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
        SessionStatus.IDLE -> "Idle — ready to start"
        SessionStatus.RUNNING -> "Session running"
        SessionStatus.PAUSED -> "Paused"
        SessionStatus.COMPLETED -> "Session complete"
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text("Status", style = MaterialTheme.typography.labelMedium)
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
            Text("Live metrics", style = MaterialTheme.typography.labelMedium)
            Spacer(Modifier.height(8.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Metric("Coherence", coherence?.let { "%.1f".format(it) } ?: "—", coherenceColor(coherence))
                Metric("RMSSD", rmssd?.let { "$it ms" } ?: "—", MaterialTheme.colorScheme.onSurface)
                Metric("Impedance", "$impedancePass/8", MaterialTheme.colorScheme.onSurface)
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
    val label = if (phase == PacerPhase.INHALE) "Breathe in" else "Breathe out"
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
