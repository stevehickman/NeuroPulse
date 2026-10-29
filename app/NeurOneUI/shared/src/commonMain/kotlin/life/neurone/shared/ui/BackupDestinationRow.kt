package life.neurone.shared.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
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
import androidx.compose.runtime.setValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.core.cloudauth.CloudService
import life.neurone.core.cloudauth.TargetField
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
    CloudService.GOOGLE_CLOUD_STORAGE -> Res.string.cloud_service_google_cloud_storage
    CloudService.AZURE_BLOB -> Res.string.cloud_service_azure_blob
    CloudService.AWS_S3 -> Res.string.cloud_service_aws_s3
    CloudService.DROPBOX -> Res.string.cloud_service_dropbox
    CloudService.WEBDAV -> Res.string.cloud_service_webdav
    CloudService.AZURE_BLOB_SAS -> Res.string.cloud_service_azure_blob_sas
}

private fun fieldLabel(field: TargetField): StringResource = when (field) {
    TargetField.BUCKET, TargetField.GCS_BUCKET -> Res.string.cloud_backup_field_bucket
    TargetField.REGION -> Res.string.cloud_backup_field_region
    TargetField.STORAGE_ACCOUNT -> Res.string.cloud_backup_field_storage_account
    TargetField.CONTAINER -> Res.string.cloud_backup_field_container
    TargetField.ENDPOINT -> Res.string.cloud_backup_field_endpoint
}

@Composable
fun BackupDestinationRow(app: AppServices, modifier: Modifier = Modifier) {
    val model = app.cloudBackup ?: return
    LaunchedEffect(model) { model.refresh() }
    val destinations by model.destinations.collectAsState()
    val selected by model.selected.collectAsState()
    val target by model.target.collectAsState()
    val scope = rememberCoroutineScope()
    // The account whose bucket form is open, and whether the last Save was refused. Nothing typed outlives the form.
    var editing by remember { mutableStateOf<String?>(null) }
    var refused by remember { mutableStateOf(false) }

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
            else OutlinedButton(
                onClick = {
                    if (d.fields.isEmpty()) scope.launch { model.choose(d.accountId) } else { refused = false; editing = d.accountId }
                },
                enabled = !d.needsSignIn,
                modifier = Modifier.fillMaxWidth(),
            ) { name() }
            if (editing == d.accountId && d.fields.isNotEmpty() && !d.needsSignIn) {
                val values = remember(d.accountId) {
                    mutableStateOf(d.fields.associateWith { if (selected == d.accountId) target?.values?.get(it).orEmpty() else "" })
                }
                d.fields.forEach { f ->
                    OutlinedTextField(
                        value = values.value[f].orEmpty(),
                        onValueChange = { v -> values.value = values.value + (f to v); refused = false },
                        singleLine = true,
                        label = { Text(stringResource(fieldLabel(f))) },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
                if (refused) {
                    Text(stringResource(Res.string.cloud_backup_target_invalid), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(
                        enabled = d.fields.filter { !it.optional }.all { values.value[it].orEmpty().isNotBlank() },
                        onClick = { scope.launch { if (model.choose(d.accountId, values.value)) editing = null else refused = true } },
                    ) { Text(stringResource(Res.string.cloud_backup_target_save)) }
                    OutlinedButton(onClick = { editing = null }) { Text(stringResource(Res.string.common_cancel)) }
                }
            }
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
