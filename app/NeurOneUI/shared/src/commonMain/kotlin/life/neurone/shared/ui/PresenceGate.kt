package life.neurone.shared.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.PresenceState
import life.neurone.shared.AppServices
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

/**
 * Puts the unlock screen in front of [content] while the person's presence has not been established (`REQ-PRES-01`, `-03`), and
 * feeds the lock the host's foreground signal (the timer that checks the timeout while the window stays open is in [AppServices], on its own scope: a delay in a composition would spin a test clock) so the background timeout applies the same on every platform. With no lock (the gate is
 * off, or the host offers none) it is [content] and nothing else, so nothing changes where the gate is not on.
 *
 * While locked, [content] is not composed at all: a screen that was composed would read the store and show the empty values a locked
 * UHDR reads as, and could write a default over the sealed ones.
 */
@Composable
fun PresenceGate(services: AppServices, content: @Composable () -> Unit) {
    val lock = services.presenceLock
    if (lock == null) {
        content()
        return
    }
    val foreground by services.platform.appInForeground.collectAsState()
    LaunchedEffect(foreground) { if (foreground) lock.onForeground() else lock.onBackground() }
    val state by lock.state.collectAsState()
    if (state == PresenceState.UNLOCKED) content() else UnlockScreen(lock)
}

/** Asks for the person's presence: once on show, and again on the button if the person cancelled or it failed. */
@Composable
internal fun UnlockScreen(lock: PresenceLock) {
    val scope = rememberCoroutineScope()
    var failed by remember { mutableStateOf(false) }
    var asking by remember { mutableStateOf(false) }
    val reason = stringResource(Res.string.presence_unlock_reason)
    suspend fun ask() {
        asking = true
        failed = !lock.unlock(reason)
        asking = false
    }
    LaunchedEffect(lock) { ask() }
    Column(
        Modifier.fillMaxSize().padding(32.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(stringResource(Res.string.presence_unlock_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(12.dp))
        Text(stringResource(Res.string.presence_unlock_body), style = MaterialTheme.typography.bodyLarge)
        if (failed) {
            Spacer(Modifier.height(12.dp))
            Text(stringResource(Res.string.presence_unlock_failed), color = MaterialTheme.colorScheme.error)
        }
        Spacer(Modifier.height(24.dp))
        Button(onClick = { scope.launch { ask() } }, enabled = !asking) { Text(stringResource(Res.string.presence_unlock_button)) }
    }
}

/**
 * The Settings section for the gate. Shown only where the host offers one. Turning it on is refused until a backup exists, and the
 * person is asked to make one first (`REQ-PRES-08`): the dialog opens the backup screen rather than turning the gate on.
 */
@Composable
internal fun PresenceSettingsSection(app: AppServices, onMakeBackup: () -> Unit) {
    if (!app.presenceAvailable) return
    val scope = rememberCoroutineScope()
    var on by remember { mutableStateOf(app.presenceEnabled) }
    var needBackup by remember { mutableStateOf(false) }
    var changed by remember { mutableStateOf(false) }
    var refused by remember { mutableStateOf(false) }

    Text(stringResource(Res.string.presence_settings_title), style = MaterialTheme.typography.titleMedium)
    Spacer(Modifier.height(4.dp))
    Text(stringResource(Res.string.presence_settings_body), style = MaterialTheme.typography.bodyMedium)
    Spacer(Modifier.height(8.dp))
    Text(stringResource(if (on) Res.string.presence_state_on else Res.string.presence_state_off), style = MaterialTheme.typography.bodyMedium)
    OutlinedButton(onClick = {
        scope.launch {
            refused = false
            when (app.setPresenceGate(!on)) {
                AppServices.PresenceChange.CHANGED -> { on = app.presenceEnabled; changed = true }
                AppServices.PresenceChange.BACKUP_REQUIRED -> needBackup = true
                AppServices.PresenceChange.REFUSED -> refused = true
            }
        }
    }) { Text(stringResource(if (on) Res.string.presence_disable_button else Res.string.presence_enable_button)) }
    if (changed) Text(stringResource(Res.string.presence_applies_next_open), style = MaterialTheme.typography.bodySmall)
    if (refused) Text(stringResource(Res.string.presence_change_failed), color = MaterialTheme.colorScheme.error)
    if (needBackup) {
        AlertDialog(
            onDismissRequest = { needBackup = false },
            title = { Text(stringResource(Res.string.presence_backup_title)) },
            text = { Text(stringResource(Res.string.presence_backup_body)) },
            confirmButton = { TextButton(onClick = { needBackup = false; onMakeBackup() }) { Text(stringResource(Res.string.presence_backup_make)) } },
            dismissButton = { TextButton(onClick = { needBackup = false }) { Text(stringResource(Res.string.presence_backup_later)) } },
        )
    }
}
