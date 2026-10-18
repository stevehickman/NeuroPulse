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
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
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
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.shared.AppServices
import life.neurone.shared.MigrationPlace
import life.neurone.shared.migration.BackupKind
import life.neurone.shared.migration.BackupStep
import life.neurone.shared.migration.LIST_MARK
import life.neurone.shared.migration.MessageGroup
import life.neurone.shared.migration.MigrationBackupModel
import life.neurone.shared.migration.PortableFilePlace
import life.neurone.shared.migration.SecretProblem
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

// Back up, or move to a new phone (SWR-MG-02, SWR-MG-03; issue #569 package 6o). The screen draws the step the model is on and
// says what the chosen kind leaves out BEFORE the backup is made. It decides nothing: which items a kind carries, what a
// secret must be and what ends a backup are the shared core's, and every word it shows for them is a locale key the core names.

@Composable
fun MigrationBackupScreen(
    app: AppServices,
    onDone: () -> Unit,
    modifier: Modifier = Modifier,
    /** Back up into this place instead of the host's own: the bundle for a direct move to another phone (SWR-MG-10). */
    linkPlace: MigrationPlace? = null,
    /** With [linkPlace]: what the person does once the backup is made, which is to be shown the code. */
    onBackedUp: (() -> Unit)? = null,
) {
    val transfer = app.platform.rememberFileTransfer()
    val hostPlace by app.backupPlace.collectAsState()
    val portable = remember(transfer, hostPlace == null) {
        if (linkPlace == null && hostPlace == null && transfer != null) PortableFilePlace(transfer) else null
    }
    val place = linkPlace ?: hostPlace ?: portable?.place
    val model = remember(place) { MigrationBackupModel(place, app.keyValueStore, app.migrationItems) }
    val step by model.step.collectAsState()
    val scope = rememberCoroutineScope()
    // A move to another phone is a migration and nothing else: there is no kind to choose.
    LaunchedEffect(model) { if (linkPlace != null) model.chooseKind(BackupKind.MIGRATE) }

    Column(modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp)) {
        Text(stringResource(Res.string.migration_backup_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))
        Text(stringResource(Res.string.migration_backup_intro), style = MaterialTheme.typography.bodyMedium)
        if (place != null && portable == null && linkPlace == null) {
            Spacer(Modifier.height(4.dp))
            Text(stringResource(Res.string.migration_backup_place, place.label), style = MaterialTheme.typography.bodySmall)
            BackupFolderRow(app)
        }
        Spacer(Modifier.height(16.dp))

        when (val s = step) {
            BackupStep.NoPlace -> Text(stringResource(Res.string.migration_backup_no_place))
            BackupStep.ChooseKind -> {
                if (model.canMigrate) {
                    KindCard(Res.string.migration_backup_kind_migrate_title, Res.string.migration_backup_kind_migrate_body) {
                        model.chooseKind(BackupKind.MIGRATE)
                    }
                } else {
                    // The core withholds this place from a migration; say so rather than offer what would be refused.
                    Text(words(MessageGroup.PROBE, "backup_only"), style = MaterialTheme.typography.bodySmall)
                }
                if (linkPlace == null) {
                    Spacer(Modifier.height(12.dp))
                    KindCard(Res.string.migration_backup_kind_uhdr_title, Res.string.migration_backup_kind_uhdr_body) {
                        model.chooseKind(BackupKind.UHDR)
                    }
                }
            }
            is BackupStep.Preview -> {
                ItemList(Res.string.migration_backup_carried_title, s.preview.carried)
                ItemList(Res.string.migration_backup_left_out_title, s.preview.omitted)
                ItemList(Res.string.migration_backup_never_moved_title, s.preview.neverMoved)
                ItemList(Res.string.migration_backup_not_sorted_title, s.preview.notYetSorted)
                Spacer(Modifier.height(16.dp))
                Button(onClick = { model.continueToMethod() }) { Text(stringResource(Res.string.migration_backup_continue)) }
            }
            is BackupStep.ChooseMethod -> {
                Text(stringResource(Res.string.migration_backup_method_title), style = MaterialTheme.typography.titleMedium)
                Spacer(Modifier.height(12.dp))
                Button(onClick = { model.useRecoveryCode() }) { Text(stringResource(Res.string.migration_backup_method_code)) }
                Spacer(Modifier.height(8.dp))
                OutlinedButton(onClick = { model.usePassphrase() }) {
                    Text(stringResource(Res.string.migration_backup_method_passphrase))
                }
            }
            is BackupStep.ShowCode -> {
                var saved by remember(s.code) { mutableStateOf(false) }
                Text(stringResource(Res.string.migration_backup_code_title), style = MaterialTheme.typography.titleMedium)
                Text(stringResource(Res.string.migration_backup_code_body), style = MaterialTheme.typography.bodySmall)
                Spacer(Modifier.height(12.dp))
                Card(Modifier.fillMaxWidth()) {
                    SelectionContainer {
                        Text(
                            s.code,
                            modifier = Modifier.padding(16.dp),
                            style = MaterialTheme.typography.titleLarge,
                            fontFamily = FontFamily.Monospace,
                        )
                    }
                }
                Spacer(Modifier.height(12.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Checkbox(checked = saved, onCheckedChange = { saved = it })
                    Text(stringResource(Res.string.migration_backup_code_saved))
                }
                Spacer(Modifier.height(8.dp))
                Button(enabled = saved, onClick = { scope.launch { model.confirmCodeSaved() } }) {
                    Text(stringResource(Res.string.migration_backup_continue))
                }
            }
            is BackupStep.EnterPassphrase -> {
                var text by remember { mutableStateOf("") }
                OutlinedTextField(
                    value = text,
                    onValueChange = { text = it },
                    label = { Text(stringResource(Res.string.migration_backup_passphrase_label)) },
                    visualTransformation = PasswordVisualTransformation(),
                    isError = s.problem != null,
                    supportingText = if (s.problem != null) ({ ProblemText(s.problem) }) else null,
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(8.dp))
                Button(onClick = { scope.launch { model.submitPassphrase(text) } }) {
                    Text(stringResource(Res.string.migration_backup_continue))
                }
            }
            BackupStep.Running -> Busy(Res.string.migration_backup_running)
            is BackupStep.Done -> {
                Text(stringResource(Res.string.migration_backup_done_title), style = MaterialTheme.typography.titleMedium)
                Text(stringResource(Res.string.migration_backup_done_body, s.bundleHash.take(12)))
                ItemList(Res.string.migration_backup_carried_title, s.written)
                ItemList(Res.string.migration_backup_left_out_title, s.omitted)
                if (onBackedUp != null) {
                    Spacer(Modifier.height(12.dp))
                    Button(onClick = onBackedUp) { Text(stringResource(Res.string.migration_link_show_code)) }
                }
                if (portable != null) {
                    var saved by remember { mutableStateOf<Boolean?>(null) }
                    Spacer(Modifier.height(12.dp))
                    Button(onClick = { scope.launch { saved = portable.saveBackup() } }) {
                        Text(stringResource(Res.string.migration_file_save))
                    }
                    saved?.let {
                        Text(stringResource(if (it) Res.string.migration_file_saved else Res.string.migration_file_not_saved))
                    }
                }
            }
            is BackupStep.Failed -> {
                Text(stringResource(Res.string.migration_backup_failed_title), style = MaterialTheme.typography.titleMedium)
                Text(words(MessageGroup.REASONS, s.reason))
            }
        }

        Spacer(Modifier.height(24.dp))
        OutlinedButton(
            onClick = {
                when (step) {
                    BackupStep.NoPlace, BackupStep.ChooseKind, is BackupStep.Done -> onDone()
                    BackupStep.Running -> {}
                    else -> model.back()
                }
            },
        ) { Text(stringResource(Res.string.common_back)) }
    }
}

@Composable
internal fun Busy(label: StringResource) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
        CircularProgressIndicator()
        Text(stringResource(label))
    }
}

@Composable
internal fun ProblemText(problem: SecretProblem) {
    val res = MigrationBackupModel.messageKey(problem.group, problem.reason)?.let(::migrationText)
    if (res != null) Text(stringResource(res, problem.minChars?.toString() ?: ""))
}

@Composable
private fun KindCard(title: StringResource, body: StringResource, onChoose: () -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text(stringResource(title), style = MaterialTheme.typography.titleMedium)
            Text(stringResource(body), style = MaterialTheme.typography.bodySmall)
            Spacer(Modifier.height(8.dp))
            Button(onClick = onChoose) { Text(stringResource(Res.string.migration_backup_continue)) }
        }
    }
}

@Composable
internal fun ItemList(title: StringResource, codes: List<String>) {
    if (codes.isEmpty()) return
    Spacer(Modifier.height(12.dp))
    Text(stringResource(title), style = MaterialTheme.typography.titleSmall)
    codes.forEach { Text("$LIST_MARK " + words(MessageGroup.ITEMS, it), style = MaterialTheme.typography.bodyMedium) }
}

/** The words for a core code: its locale key from the core's table, found in the resources; the code itself if there is none. */
@Composable
internal fun words(group: MessageGroup, code: String): String {
    val res = MigrationBackupModel.messageKey(group, code)?.let(::migrationText)
    return if (res != null) stringResource(res) else code
}
