package life.neurone.shared.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import life.neurone.shared.migration.FileTransfer
import life.neurone.shared.resources.*
import life.neurone.shared.session.EdfDownloadModel
import life.neurone.shared.session.EdfProblem
import life.neurone.shared.session.EdfState
import org.jetbrains.compose.resources.stringResource

/**
 * The button that fetches a finished session's EDF+ recording from the hub (Mode 4), with its progress and outcome. The recording is the
 * person's raw waveform data (UHDR): it is stored in the app's own folder, unchanged, and nothing here reads or sends it. When the hub's
 * file does not arrive, the person can choose the file the hub saved and the app keeps a copy ([EdfDownloadModel.adopt]).
 */
@Composable
fun EdfDownloadSection(model: EdfDownloadModel, sessionId: Long, fileTransfer: FileTransfer?, modifier: Modifier = Modifier) {
    val state by model.state.collectAsState()
    val scope = rememberCoroutineScope()
    var running: Job? = null
    // Leaving the screen stops a download in progress (the Swift app cancels it too).
    DisposableEffect(model) { onDispose { running?.cancel() } }

    Column(modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        when (val s = state) {
            EdfState.Idle -> Button(onClick = { running = scope.launch { model.download(sessionId) } }, modifier = Modifier.fillMaxWidth()) {
                Text(stringResource(Res.string.session_download_button))
            }
            EdfState.Downloading -> Text(stringResource(Res.string.history_downloading), style = MaterialTheme.typography.bodyMedium)
            is EdfState.Downloaded -> {
                Text(stringResource(Res.string.session_downloaded_label), style = MaterialTheme.typography.titleSmall)
                Text(s.fileName, style = MaterialTheme.typography.bodySmall)
            }
            is EdfState.Failed -> {
                Text(
                    stringResource(
                        when (s.problem) {
                            EdfProblem.NOT_CONNECTED -> Res.string.edf_not_connected
                            EdfProblem.NO_DATA -> Res.string.edf_no_data
                            EdfProblem.NOT_EDF -> Res.string.edf_not_edf
                            EdfProblem.NOT_SAVED -> Res.string.edf_not_saved
                        },
                    ),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.error,
                )
                Spacer(Modifier.height(8.dp))
                Button(onClick = { running = scope.launch { model.download(sessionId) } }, modifier = Modifier.fillMaxWidth()) {
                    Text(stringResource(Res.string.session_download_retry))
                }
                if (fileTransfer != null && (s.problem == EdfProblem.NO_DATA || s.problem == EdfProblem.NOT_EDF)) {
                    Spacer(Modifier.height(8.dp))
                    OutlinedButton(
                        onClick = { running = scope.launch { fileTransfer.open()?.let { model.adopt(sessionId, it) } } },
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text(stringResource(Res.string.edf_choose_file)) }
                }
            }
        }
    }
}
