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
import life.neurone.shared.migration.LinkReceiveModel
import life.neurone.shared.migration.LinkReceiveStep
import life.neurone.shared.migration.MessageGroup
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

// Receive from another phone on the same Wi-Fi (SWR-MG-10; issue #569 package 6u). The person types the code the old phone
// shows; the core pairs, reads the bundle's header from the other phone, and the screen then asks for the recovery code or
// passphrase it names. Every rule is the shared core's and every word for it is a locale key the core names.

@Composable
fun MigrationLinkReceiveScreen(app: AppServices, onDone: () -> Unit, modifier: Modifier = Modifier) {
    val model = remember { LinkReceiveModel(app.platform.linkTransport, app.keyValueStore, app.migrationItems) }
    val step by model.step.collectAsState()
    val scope = rememberCoroutineScope()

    Column(modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp)) {
        Text(stringResource(Res.string.migration_link_receive_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))
        Text(stringResource(Res.string.migration_link_receive_intro), style = MaterialTheme.typography.bodyMedium)
        Spacer(Modifier.height(16.dp))

        if (!model.available) {
            Text(stringResource(Res.string.migration_link_not_available))
        } else when (val s = step) {
            is LinkReceiveStep.EnterCode -> {
                var text by remember { mutableStateOf("") }
                OutlinedTextField(
                    value = text,
                    onValueChange = { text = it },
                    label = { Text(stringResource(Res.string.migration_link_code_label)) },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(8.dp))
                Button(onClick = { scope.launch { model.submitCode(text) } }) { Text(stringResource(Res.string.migration_link_find_button)) }
            }
            LinkReceiveStep.Working -> Busy(Res.string.migration_link_working)
            is LinkReceiveStep.EnterSecret -> {
                var text by remember { mutableStateOf("") }
                val recovery = s.method == BundleKeyMethod.RECOVERY_CODE
                OutlinedTextField(
                    value = text,
                    onValueChange = { text = it },
                    label = {
                        Text(stringResource(if (recovery) Res.string.migration_restore_code_label else Res.string.migration_backup_passphrase_label))
                    },
                    visualTransformation = if (recovery) VisualTransformation.None else PasswordVisualTransformation(),
                    isError = s.problem != null,
                    supportingText = if (s.problem != null) ({ ProblemText(s.problem) }) else null,
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(8.dp))
                Button(onClick = { model.submitSecret(text) }) { Text(stringResource(Res.string.migration_restore_button)) }
                Spacer(Modifier.height(8.dp))
                OutlinedButton(onClick = { model.cancel() }) { Text(stringResource(Res.string.common_back)) }
            }
            is LinkReceiveStep.Done -> RestoreReportView(s.report)
            is LinkReceiveStep.Failed -> {
                Text(stringResource(Res.string.migration_link_failed_title), style = MaterialTheme.typography.titleMedium)
                Text(words(MessageGroup.REASONS, s.reason))
                Spacer(Modifier.height(8.dp))
                OutlinedButton(onClick = { model.again() }) { Text(stringResource(Res.string.migration_restore_try_again)) }
            }
        }

        Spacer(Modifier.height(24.dp))
        OutlinedButton(onClick = { if (step != LinkReceiveStep.Working) onDone() }) { Text(stringResource(Res.string.common_back)) }
    }
}
