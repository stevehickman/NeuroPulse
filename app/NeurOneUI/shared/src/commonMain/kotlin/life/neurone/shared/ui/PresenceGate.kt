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
import androidx.compose.material3.OutlinedTextField
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
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.core.common.PassphraseResult
import life.neurone.core.common.passphraseMinChars
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

/**
 * Asks for the person's presence: once on show, and again on the button if the person cancelled or it failed. On a host whose key is a
 * passphrase (`REQ-PRES-06`) it asks for the passphrase instead, says why, and never opens a system prompt.
 */
@Composable
internal fun UnlockScreen(lock: PresenceLock) {
    val scope = rememberCoroutineScope()
    var failed by remember { mutableStateOf(false) }
    var asking by remember { mutableStateOf(false) }
    var wrong by remember { mutableStateOf(false) }
    var passphrase by remember { mutableStateOf("") }
    val usesPassphrase = lock.passphraseKey != null
    val reason = stringResource(Res.string.presence_unlock_reason)
    suspend fun ask() {
        asking = true
        failed = !lock.unlock(reason)
        asking = false
    }
    suspend fun submit() {
        asking = true
        val result = lock.unlockWithPassphrase(passphrase)
        wrong = result != PassphraseResult.OK
        if (!wrong) passphrase = ""
        asking = false
    }
    LaunchedEffect(lock) { if (!usesPassphrase) ask() }
    Column(
        Modifier.fillMaxSize().padding(32.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(stringResource(Res.string.presence_unlock_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(12.dp))
        Text(stringResource(Res.string.presence_unlock_body), style = MaterialTheme.typography.bodyLarge)
        if (usesPassphrase) {
            Spacer(Modifier.height(12.dp))
            Text(stringResource(Res.string.presence_passphrase_notice), style = MaterialTheme.typography.bodyMedium)
            Spacer(Modifier.height(12.dp))
            OutlinedTextField(
                value = passphrase,
                onValueChange = { passphrase = it },
                label = { Text(stringResource(Res.string.presence_passphrase_label)) },
                visualTransformation = PasswordVisualTransformation(),
                singleLine = true,
            )
            if (wrong) Text(stringResource(Res.string.presence_passphrase_wrong), color = MaterialTheme.colorScheme.error)
        } else if (failed) {
            Spacer(Modifier.height(12.dp))
            Text(stringResource(Res.string.presence_unlock_failed), color = MaterialTheme.colorScheme.error)
        }
        Spacer(Modifier.height(24.dp))
        Button(
            onClick = { scope.launch { if (usesPassphrase) submit() else ask() } },
            enabled = !asking && (!usesPassphrase || passphrase.isNotEmpty()),
        ) { Text(stringResource(Res.string.presence_unlock_button)) }
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
    var choosing by remember { mutableStateOf(false) }
    var changing by remember { mutableStateOf(false) }
    val passphraseKey = app.presencePassphraseKey
    val unlockReason = stringResource(Res.string.presence_unlock_reason)

    Text(stringResource(Res.string.presence_settings_title), style = MaterialTheme.typography.titleMedium)
    Spacer(Modifier.height(4.dp))
    Text(stringResource(Res.string.presence_settings_body), style = MaterialTheme.typography.bodyMedium)
    // A host that cannot bind a key to a device credential says so, and is never silently ungated (REQ-PRES-06).
    if (passphraseKey != null) {
        Spacer(Modifier.height(4.dp))
        Text(stringResource(Res.string.presence_passphrase_notice), style = MaterialTheme.typography.bodyMedium)
    }
    Spacer(Modifier.height(8.dp))
    Text(stringResource(if (on) Res.string.presence_state_on else Res.string.presence_state_off), style = MaterialTheme.typography.bodyMedium)
    // There is no "turn off": sealed values would have to move back to the ungated key, which is not built (OI-PRES-09).
    if (!on) OutlinedButton(onClick = {
        scope.launch {
            refused = false
            if (passphraseKey != null && !on) {
                // The backup comes first, before a passphrase is asked for (REQ-PRES-08).
                if (app.hasRecoverableBackup()) choosing = true else needBackup = true
                return@launch
            }
            when (app.setPresenceGate(!on, reason = unlockReason)) {
                AppServices.PresenceChange.CHANGED -> { on = app.presenceEnabled; changed = true }
                AppServices.PresenceChange.BACKUP_REQUIRED -> needBackup = true
                else -> refused = true
            }
        }
    }) { Text(stringResource(Res.string.presence_enable_button)) }
    if (passphraseKey != null && on) {
        OutlinedButton(onClick = { changing = true }) { Text(stringResource(Res.string.presence_passphrase_change_button)) }
    }
    if (choosing) {
        PassphraseDialog(
            title = Res.string.presence_passphrase_set_title,
            askOld = false,
            onDismiss = { choosing = false },
            submit = { _, new ->
                when (app.setPresenceGate(true, new)) {
                    AppServices.PresenceChange.CHANGED -> { on = app.presenceEnabled; changed = true; choosing = false; null }
                    AppServices.PresenceChange.PASSPHRASE_TOO_SHORT -> PassphraseResult.TOO_SHORT
                    else -> PassphraseResult.FAILED
                }
            },
        )
    }
    if (changing && passphraseKey != null) {
        PassphraseDialog(
            title = Res.string.presence_passphrase_change_button,
            askOld = true,
            onDismiss = { changing = false },
            submit = { old, new ->
                when (val r = passphraseKey.changePassphrase(old, new)) {
                    PassphraseResult.OK -> { changing = false; null }
                    else -> r
                }
            },
        )
    }
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

/**
 * Two or three password fields for setting or changing the passphrase. [submit] returns null on success and the failure otherwise;
 * the dialog says which. The length rule is the core's, so the minimum shown is the one enforced.
 */
@Composable
private fun PassphraseDialog(
    title: org.jetbrains.compose.resources.StringResource,
    askOld: Boolean,
    onDismiss: () -> Unit,
    submit: suspend (old: String, new: String) -> PassphraseResult?,
) {
    val scope = rememberCoroutineScope()
    var old by remember { mutableStateOf("") }
    var new by remember { mutableStateOf("") }
    var again by remember { mutableStateOf("") }
    var problem by remember { mutableStateOf<PassphraseResult?>(null) }
    var mismatch by remember { mutableStateOf(false) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(title)) },
        text = {
            Column {
                if (askOld) PassphraseField(old, { old = it }, Res.string.presence_passphrase_old_label)
                PassphraseField(new, { new = it }, if (askOld) Res.string.presence_passphrase_new_label else Res.string.presence_passphrase_label)
                PassphraseField(again, { again = it }, Res.string.presence_passphrase_confirm_label)
                if (mismatch) Text(stringResource(Res.string.presence_passphrase_mismatch), color = MaterialTheme.colorScheme.error)
                when (problem) {
                    PassphraseResult.WRONG -> Text(stringResource(Res.string.presence_passphrase_wrong), color = MaterialTheme.colorScheme.error)
                    PassphraseResult.TOO_SHORT -> Text(
                        stringResource(Res.string.presence_passphrase_too_short, passphraseMinChars()),
                        color = MaterialTheme.colorScheme.error,
                    )
                    PassphraseResult.FAILED -> Text(stringResource(Res.string.presence_change_failed), color = MaterialTheme.colorScheme.error)
                    else -> {}
                }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                mismatch = new != again
                if (!mismatch) scope.launch { problem = submit(old, new) }
            }) { Text(stringResource(Res.string.presence_passphrase_save)) }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(Res.string.presence_passphrase_cancel)) } },
    )
}

@Composable
private fun PassphraseField(value: String, onChange: (String) -> Unit, label: org.jetbrains.compose.resources.StringResource) {
    OutlinedTextField(
        value = value,
        onValueChange = onChange,
        label = { Text(stringResource(label)) },
        visualTransformation = PasswordVisualTransformation(),
        singleLine = true,
    )
}
