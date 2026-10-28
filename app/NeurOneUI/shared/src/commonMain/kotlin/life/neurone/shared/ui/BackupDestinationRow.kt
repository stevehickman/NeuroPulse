package life.neurone.shared.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.core.cloudauth.CloudService
import life.neurone.shared.AppServices
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

// Where the backup is kept: this device, or a cloud account the person signed in to (SWR-MG-21). The choice, the places and the checks are
// CloudBackupModel; this draws them. A host with no cloud sign-in has no model and nothing is drawn.

private fun serviceName(service: CloudService): StringResource = when (service) {
    CloudService.GOOGLE_DRIVE -> Res.string.cloud_service_google_drive
    CloudService.ONEDRIVE -> Res.string.cloud_service_onedrive
    CloudService.ICLOUD_CLOUDKIT -> Res.string.cloud_service_icloud_cloudkit
    else -> Res.string.cloud_accounts_title // not offered: only services that need nothing beyond the sign-in are listed
}

@Composable
fun BackupDestinationRow(app: AppServices, modifier: Modifier = Modifier) {
    val model = app.cloudBackup ?: return
    LaunchedEffect(model) { model.refresh() }
    val destinations by model.destinations.collectAsState()
    val selected by model.selected.collectAsState()
    val scope = rememberCoroutineScope()

    Column(modifier = modifier) {
        Spacer(Modifier.height(8.dp))
        Text(stringResource(Res.string.cloud_backup_title), style = MaterialTheme.typography.titleSmall)
        if (destinations.isEmpty()) {
            Text(stringResource(Res.string.cloud_backup_none), style = MaterialTheme.typography.bodySmall)
            return@Column
        }
        Spacer(Modifier.height(4.dp))
        val device: @Composable () -> Unit = { Text(stringResource(Res.string.cloud_backup_this_device)) }
        if (selected == null) Button(onClick = { model.useDevice() }, modifier = Modifier.fillMaxWidth()) { device() }
        else OutlinedButton(onClick = { model.useDevice() }, modifier = Modifier.fillMaxWidth()) { device() }
        destinations.forEach { d ->
            Spacer(Modifier.height(4.dp))
            val name: @Composable () -> Unit = { Text(stringResource(serviceName(d.service))) }
            if (selected == d.accountId) Button(onClick = {}, enabled = !d.needsSignIn, modifier = Modifier.fillMaxWidth()) { name() }
            else OutlinedButton(onClick = { scope.launch { model.choose(d.accountId) } }, enabled = !d.needsSignIn, modifier = Modifier.fillMaxWidth()) { name() }
            if (d.needsSignIn) {
                Text(
                    stringResource(Res.string.cloud_backup_needs_sign_in, stringResource(serviceName(d.service))),
                    style = MaterialTheme.typography.bodySmall,
                    color = if (selected == d.accountId) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface,
                )
            }
        }
        if (selected != null) {
            Spacer(Modifier.height(4.dp))
            Text(stringResource(Res.string.cloud_backup_limits), style = MaterialTheme.typography.bodySmall)
        }
    }
}
