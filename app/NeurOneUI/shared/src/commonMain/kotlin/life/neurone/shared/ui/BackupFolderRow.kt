package life.neurone.shared.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.shared.AppServices
import life.neurone.shared.migration.FolderProblem
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

// Where the backup is kept, on a host that lets the person choose (issue #569 package 6zz). The folder, the checks and the saved
// choice are common code (BackupFolderModel); this draws them. A host with a fixed place has no model and nothing is drawn.

@Composable
fun BackupFolderRow(app: AppServices, modifier: Modifier = Modifier) {
    val model = app.backupFolder ?: return
    // A cloud account is the place while one is chosen; the folder is not, so its choice is not offered then (SWR-MG-21).
    if (app.cloudBackup?.selected?.collectAsState()?.value != null) return
    val problem by model.problem.collectAsState()
    val folder by model.folder.collectAsState()
    val scope = rememberCoroutineScope()

    Column(modifier = modifier) {
        Spacer(Modifier.height(4.dp))
        Row {
            OutlinedButton(onClick = { scope.launch { model.choose() } }) { Text(stringResource(Res.string.migration_folder_choose)) }
            if (folder != model.defaultFolder) {
                Spacer(Modifier.width(8.dp))
                val label = if (model.defaultFolder == null) Res.string.migration_folder_none else Res.string.migration_folder_default
                OutlinedButton(onClick = { scope.launch { model.useDefault() } }) { Text(stringResource(label)) }
            }
        }
        problem?.let {
            Spacer(Modifier.height(4.dp))
            Text(
                stringResource(
                    when (it) {
                        FolderProblem.HoldsAppData -> Res.string.migration_folder_holds_app_data
                        FolderProblem.NotWritable -> Res.string.migration_folder_not_writable
                    },
                ),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.error,
            )
        }
    }
}
