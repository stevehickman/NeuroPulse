package life.neurone.shared.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
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
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.shared.AppServices
import life.neurone.shared.migration.ClearStep
import life.neurone.shared.migration.MessageGroup
import life.neurone.shared.migration.MigrationClearModel
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

// Clear this phone after a move (SWR-MG-06, SWR-MG-08, SWR-MG-09; issue #569 package 6p). The erase cannot be undone, so the
// person confirms it, and the core goes ahead only against the new phone's confirmation of the backup on the place.

@Composable
fun MigrationClearScreen(app: AppServices, onDone: () -> Unit, modifier: Modifier = Modifier) {
    val platform = app.platform
    val model = remember {
        MigrationClearModel(platform.migrationPlace, app.keyValueStore, app.clearActions, platform.migrationFiles)
    }
    val step by model.step.collectAsState()
    val scope = rememberCoroutineScope()
    LaunchedEffect(model) { model.look() }

    Column(modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp)) {
        Text(stringResource(Res.string.migration_clear_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))
        Text(stringResource(Res.string.migration_clear_intro), style = MaterialTheme.typography.bodyMedium)
        Spacer(Modifier.height(16.dp))

        when (val s = step) {
            ClearStep.NotAvailable -> Text(stringResource(Res.string.migration_clear_not_available))
            ClearStep.Looking -> Busy(Res.string.migration_clear_looking)
            is ClearStep.Confirm -> {
                var understood by remember { mutableStateOf(false) }
                Text(stringResource(Res.string.migration_clear_ready, s.info.bundleHash.take(12)))
                Spacer(Modifier.height(12.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Checkbox(checked = understood, onCheckedChange = { understood = it })
                    Text(stringResource(Res.string.migration_clear_confirm))
                }
                Spacer(Modifier.height(8.dp))
                Button(enabled = understood, onClick = { scope.launch { model.clear() } }) {
                    Text(stringResource(Res.string.migration_clear_button))
                }
            }
            ClearStep.Running -> Busy(Res.string.migration_clear_running)
            is ClearStep.Done -> {
                Text(stringResource(Res.string.migration_clear_done_title), style = MaterialTheme.typography.titleMedium)
                if (s.removeBluetoothBond) {
                    Spacer(Modifier.height(8.dp))
                    Text(stringResource(Res.string.migration_clear_bond_notice))
                }
            }
            is ClearStep.Failed -> {
                Text(stringResource(Res.string.migration_clear_failed_title), style = MaterialTheme.typography.titleMedium)
                Text(words(MessageGroup.REASONS, s.reason))
                Spacer(Modifier.height(8.dp))
                Text(stringResource(Res.string.migration_clear_retry_note), style = MaterialTheme.typography.bodySmall)
                Spacer(Modifier.height(8.dp))
                OutlinedButton(onClick = { scope.launch { model.retry() } }) { Text(stringResource(Res.string.migration_restore_try_again)) }
            }
        }

        Spacer(Modifier.height(24.dp))
        OutlinedButton(onClick = { if (step != ClearStep.Running) onDone() }) { Text(stringResource(Res.string.common_back)) }
    }
}
