package life.neurone.shared.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import life.neurone.core.migration.LinkSendSession
import life.neurone.core.migration.LinkSendState
import life.neurone.shared.AppServices
import life.neurone.shared.migration.MessageGroup
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

// Move to another phone on the same Wi-Fi (SWR-MG-10; issue #569 package 6u). The old phone makes the backup into memory,
// shows the code and serves the bundle to the phone that proves it knows the code. The pairing, the proof, the sealed link
// and the claim are the core's; the screen draws the step the session is on.

private enum class SendPhase { Backup, Code, Clear }

@Composable
fun MigrationLinkSendScreen(app: AppServices, onDone: () -> Unit, modifier: Modifier = Modifier) {
    val place = app.linkPlace
    var phase by remember { mutableStateOf(SendPhase.Backup) }
    when {
        place == null -> Column(modifier = modifier.fillMaxSize().padding(24.dp)) {
            Text(stringResource(Res.string.migration_link_not_available))
            Spacer(Modifier.height(16.dp))
            OutlinedButton(onClick = onDone) { Text(stringResource(Res.string.common_back)) }
        }
        phase == SendPhase.Backup -> MigrationBackupScreen(
            app = app,
            onDone = onDone,
            modifier = modifier,
            linkPlace = place,
            onBackedUp = { phase = SendPhase.Code },
        )
        phase == SendPhase.Code -> CodePhase(app, onDone, onSent = { phase = SendPhase.Clear }, modifier)
        else -> MigrationClearScreen(app = app, onDone = onDone, modifier = modifier, place = place)
    }
}

@Composable
private fun CodePhase(app: AppServices, onDone: () -> Unit, onSent: () -> Unit, modifier: Modifier) {
    val place = app.linkPlace!!
    var attempt by remember { mutableStateOf(0) }
    val session = remember(attempt) { LinkSendSession(place.store, app.platform.linkTransport!!) }
    val step by session.step.collectAsState()
    LaunchedEffect(session) { session.run() }

    Column(modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp)) {
        Text(stringResource(Res.string.migration_link_send_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(16.dp))
        when (val s = step) {
            LinkSendState.Idle -> Busy(Res.string.migration_link_waiting)
            is LinkSendState.Waiting -> {
                Text(stringResource(Res.string.migration_link_code_title), style = MaterialTheme.typography.titleMedium)
                Text(stringResource(Res.string.migration_link_code_body), style = MaterialTheme.typography.bodySmall)
                Spacer(Modifier.height(12.dp))
                Card(Modifier.fillMaxWidth()) {
                    SelectionContainer {
                        Text(
                            s.display,
                            modifier = Modifier.padding(16.dp),
                            style = MaterialTheme.typography.displaySmall,
                            fontFamily = FontFamily.Monospace,
                        )
                    }
                }
                Spacer(Modifier.height(8.dp))
                Text(stringResource(Res.string.migration_link_tries_left, s.offer.triesLeft.toString()), style = MaterialTheme.typography.bodyMedium)
                Spacer(Modifier.height(12.dp))
                Busy(Res.string.migration_link_waiting)
            }
            LinkSendState.Sent -> {
                Text(stringResource(Res.string.migration_link_sent_title), style = MaterialTheme.typography.titleMedium)
                Text(stringResource(Res.string.migration_link_sent_body))
                Spacer(Modifier.height(12.dp))
                Button(onClick = onSent) { Text(stringResource(Res.string.migration_link_clear_button)) }
            }
            is LinkSendState.Spent -> {
                Text(stringResource(Res.string.migration_link_spent_title), style = MaterialTheme.typography.titleMedium)
                Text(words(MessageGroup.REASONS, s.reason))
                Spacer(Modifier.height(12.dp))
                Button(onClick = { attempt++ }) { Text(stringResource(Res.string.migration_link_new_code)) }
            }
            LinkSendState.Failed -> {
                Text(stringResource(Res.string.migration_link_unavailable))
                Spacer(Modifier.height(12.dp))
                Button(onClick = { attempt++ }) { Text(stringResource(Res.string.migration_restore_try_again)) }
            }
        }
        Spacer(Modifier.height(24.dp))
        OutlinedButton(onClick = onDone) { Text(stringResource(Res.string.common_back)) }
    }
}
