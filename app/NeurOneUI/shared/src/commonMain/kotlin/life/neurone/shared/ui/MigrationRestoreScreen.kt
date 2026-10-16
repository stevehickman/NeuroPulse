package life.neurone.shared.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
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
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.core.data.BundleKeyMethod
import life.neurone.shared.AppServices
import life.neurone.shared.migration.BackupKind
import life.neurone.shared.migration.MessageGroup
import life.neurone.shared.migration.MigrationBackupModel
import life.neurone.shared.migration.MigrationRestoreModel
import life.neurone.shared.migration.RestoreStep
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

// Restore from a backup (SWR-MG-02, SWR-MG-03, SWR-MG-04; issue #569 package 6p). The screen finds the backup on the place,
// asks for what it is locked with, restores it and says what was restored and what was not. Every rule is the shared core's,
// and every word for it is a locale key the core names.

@Composable
fun MigrationRestoreScreen(app: AppServices, onDone: () -> Unit, modifier: Modifier = Modifier) {
    val model = remember { MigrationRestoreModel(app.platform.migrationPlace, app.keyValueStore) }
    val step by model.step.collectAsState()
    val scope = rememberCoroutineScope()
    LaunchedEffect(model) { model.look() }

    Column(modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp)) {
        Text(stringResource(Res.string.migration_restore_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))
        Text(stringResource(Res.string.migration_restore_intro), style = MaterialTheme.typography.bodyMedium)
        app.platform.migrationPlace?.let {
            Spacer(Modifier.height(4.dp))
            Text(stringResource(Res.string.migration_backup_place, it.label), style = MaterialTheme.typography.bodySmall)
        }
        Spacer(Modifier.height(16.dp))

        when (val s = step) {
            RestoreStep.NoPlace -> Text(stringResource(Res.string.migration_backup_no_place))
            RestoreStep.Looking -> Busy(Res.string.migration_restore_looking)
            is RestoreStep.EnterSecret -> {
                var text by remember { mutableStateOf("") }
                val recovery = s.info.method == BundleKeyMethod.RECOVERY_CODE
                Text(
                    stringResource(
                        if (s.info.kind == BackupKind.UHDR) Res.string.migration_restore_found_uhdr else Res.string.migration_restore_found_migrate,
                    ),
                    style = MaterialTheme.typography.titleMedium,
                )
                Spacer(Modifier.height(12.dp))
                OutlinedTextField(
                    value = text,
                    onValueChange = { text = it },
                    label = {
                        Text(
                            stringResource(if (recovery) Res.string.migration_restore_code_label else Res.string.migration_backup_passphrase_label),
                        )
                    },
                    visualTransformation = if (recovery) VisualTransformation.None else PasswordVisualTransformation(),
                    isError = s.problem != null,
                    supportingText = if (s.problem != null) ({ ProblemText(s.problem) }) else null,
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(8.dp))
                Button(onClick = { scope.launch { model.submitSecret(text) } }) { Text(stringResource(Res.string.migration_restore_button)) }
            }
            RestoreStep.Running -> Busy(Res.string.migration_restore_running)
            is RestoreStep.Done -> {
                val r = s.report
                Text(stringResource(Res.string.migration_restore_done_title), style = MaterialTheme.typography.titleMedium)
                ItemList(Res.string.migration_restore_restored_title, r.restored)
                ItemList(Res.string.migration_restore_missing_title, r.missing)
                ItemList(Res.string.migration_restore_omitted_title, r.omitted)
                ItemList(Res.string.migration_restore_not_applied_title, r.notApplied)
                if (r.consentNotRestored) {
                    Spacer(Modifier.height(12.dp))
                    Text(stringResource(Res.string.migration_restore_consent_notice))
                }
                if (r.kind == BackupKind.MIGRATE) {
                    Spacer(Modifier.height(12.dp))
                    Text(
                        stringResource(if (r.confirmed) Res.string.migration_restore_confirmed else Res.string.migration_restore_not_confirmed),
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
            }
            is RestoreStep.Failed -> {
                Text(stringResource(Res.string.migration_restore_failed_title), style = MaterialTheme.typography.titleMedium)
                Text(words(MessageGroup.REASONS, s.reason))
                Spacer(Modifier.height(8.dp))
                OutlinedButton(onClick = { scope.launch { model.retry() } }) { Text(stringResource(Res.string.migration_restore_try_again)) }
            }
        }

        Spacer(Modifier.height(24.dp))
        OutlinedButton(onClick = { if (step != RestoreStep.Running) onDone() }) { Text(stringResource(Res.string.common_back)) }
    }
}
