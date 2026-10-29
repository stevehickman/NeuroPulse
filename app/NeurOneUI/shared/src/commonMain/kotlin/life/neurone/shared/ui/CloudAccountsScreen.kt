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
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
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
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import life.neurone.core.cloudauth.AccountFamily
import life.neurone.core.cloudauth.CloudService
import life.neurone.shared.AppServices
import life.neurone.shared.cloud.CloudNotice
import life.neurone.shared.cloud.CloudNoticeKind
import life.neurone.shared.cloud.CloudServiceRow
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

// The cloud services the person can sign in to (SWR-MG-20). The state and the actions are CloudAccountsModel; this draws them.
// Nothing here shows a token, a provider's own error text or an exception message: each outcome is a kind of notice with a string of its own.

/** The name the person knows a service by. A key per service, resolved where it is drawn (`CLAUDE.md` §17.3). */
private fun serviceName(service: CloudService): StringResource = when (service) {
    CloudService.GOOGLE_DRIVE -> Res.string.cloud_service_google_drive
    CloudService.GOOGLE_CLOUD_STORAGE -> Res.string.cloud_service_google_cloud_storage
    CloudService.ONEDRIVE -> Res.string.cloud_service_onedrive
    CloudService.AZURE_BLOB -> Res.string.cloud_service_azure_blob
    CloudService.ICLOUD_CLOUDKIT -> Res.string.cloud_service_icloud_cloudkit
    CloudService.AWS_S3 -> Res.string.cloud_service_aws_s3
    CloudService.DROPBOX -> Res.string.cloud_service_dropbox
    CloudService.WEBDAV -> Res.string.cloud_service_webdav
    CloudService.AZURE_BLOB_SAS -> Res.string.cloud_service_azure_blob_sas
}

@Composable
fun CloudAccountsScreen(app: AppServices, onDone: () -> Unit, modifier: Modifier = Modifier) {
    val model = app.cloudAccounts
    Column(modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp)) {
        OutlinedButton(onClick = onDone) { Text(stringResource(Res.string.common_back)) }
        Spacer(Modifier.height(12.dp))
        Text(stringResource(Res.string.cloud_accounts_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))
        if (model == null) {
            Text(stringResource(Res.string.cloud_accounts_unavailable), style = MaterialTheme.typography.bodyMedium)
            return@Column
        }
        Text(stringResource(Res.string.cloud_accounts_body), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(16.dp))

        val rows by model.rows.collectAsState()
        val busy by model.busy.collectAsState()
        val notice by model.notice.collectAsState()
        val scope = rememberCoroutineScope()
        LaunchedEffect(model) { model.refresh() }

        notice?.let {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(noticeText(it), Modifier.weight(1f), color = if (it.isProblem()) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface)
                TextButton(onClick = { model.dismissNotice() }) { Text(stringResource(Res.string.common_close)) }
            }
            Spacer(Modifier.height(8.dp))
        }

        rows.forEach { row ->
            ServiceCard(
                row = row,
                enabled = busy == null,
                onSignIn = { scope.launch { model.signIn(row.service) } },
                onSignOut = { scope.launch { model.signOut(row.service) } },
                onSaveKeys = { id, secret, token -> scope.launch { model.addAwsKeys(id, secret, token) } },
                onSaveLogin = { url, user, password -> scope.launch { model.addWebDavLogin(url, user, password) } },
                onSaveSas = { sas -> scope.launch { model.addAzureSas(sas) } },
            )
            Spacer(Modifier.height(12.dp))
        }
    }
}

@Composable
private fun ServiceCard(
    row: CloudServiceRow,
    enabled: Boolean,
    onSignIn: () -> Unit,
    onSignOut: () -> Unit,
    onSaveKeys: (String, String, String?) -> Unit,
    onSaveLogin: (String, String, String) -> Unit,
    onSaveSas: (String) -> Unit,
) {
    val isAws = row.service.family == AccountFamily.AWS
    val isWebDav = row.service.family == AccountFamily.WEBDAV
    val isSas = row.service.family == AccountFamily.AZURE_SAS
    var enteringKeys by remember { mutableStateOf(false) }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text(stringResource(serviceName(row.service)), style = MaterialTheme.typography.titleMedium)
            Text(
                stringResource(
                    when {
                        !row.configured -> Res.string.cloud_status_not_configured
                        row.needsSignIn -> Res.string.cloud_status_needs_sign_in
                        row.signedIn -> Res.string.cloud_status_signed_in
                        else -> Res.string.cloud_status_signed_out
                    },
                ),
                style = MaterialTheme.typography.bodySmall,
            )
            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (row.configured) {
                    if (!row.signedIn) {
                        if (isAws || isWebDav || isSas) {
                            Button(enabled = enabled, onClick = { enteringKeys = true }) {
                                Text(stringResource(if (isAws) Res.string.cloud_button_add_keys else if (isSas) Res.string.cloud_button_add_sas else Res.string.cloud_button_add_login))
                            }
                        } else {
                            Button(enabled = enabled, onClick = onSignIn) {
                                Text(stringResource(if (row.needsSignIn) Res.string.cloud_button_sign_in_again else Res.string.cloud_button_sign_in))
                            }
                        }
                    }
                    if (row.account != null) {
                        OutlinedButton(enabled = enabled, onClick = onSignOut) { Text(stringResource(Res.string.cloud_button_sign_out)) }
                    }
                }
            }
            if (isSas && enteringKeys && !row.signedIn) {
                AzureSasForm(
                    enabled = enabled,
                    onSave = { sas ->
                        onSaveSas(sas)
                        enteringKeys = false
                    },
                    onCancel = { enteringKeys = false },
                )
            }
            if (isWebDav && enteringKeys && !row.signedIn) {
                WebDavLoginForm(
                    enabled = enabled,
                    onSave = { url, user, password ->
                        onSaveLogin(url, user, password)
                        enteringKeys = false
                    },
                    onCancel = { enteringKeys = false },
                )
            }
            if (isAws && enteringKeys && !row.signedIn) {
                AwsKeysForm(
                    enabled = enabled,
                    onSave = { id, secret, token ->
                        onSaveKeys(id, secret, token)
                        enteringKeys = false
                    },
                    onCancel = { enteringKeys = false },
                )
            }
        }
    }
}

/** The fields are emptied when the form goes away: nothing here keeps what was typed once it has been handed on. */
@Composable
private fun AwsKeysForm(enabled: Boolean, onSave: (String, String, String?) -> Unit, onCancel: () -> Unit) {
    var accessKeyId by remember { mutableStateOf("") }
    var secret by remember { mutableStateOf("") }
    var sessionToken by remember { mutableStateOf("") }
    Spacer(Modifier.height(8.dp))
    Text(stringResource(Res.string.cloud_aws_note), style = MaterialTheme.typography.bodySmall)
    OutlinedTextField(
        value = accessKeyId, onValueChange = { accessKeyId = it }, singleLine = true, modifier = Modifier.fillMaxWidth(),
        label = { Text(stringResource(Res.string.cloud_aws_access_key_id)) },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Ascii),
    )
    OutlinedTextField(
        value = secret, onValueChange = { secret = it }, singleLine = true, modifier = Modifier.fillMaxWidth(),
        label = { Text(stringResource(Res.string.cloud_aws_secret_key)) },
        visualTransformation = PasswordVisualTransformation(),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
    )
    OutlinedTextField(
        value = sessionToken, onValueChange = { sessionToken = it }, singleLine = true, modifier = Modifier.fillMaxWidth(),
        label = { Text(stringResource(Res.string.cloud_aws_session_token)) },
        visualTransformation = PasswordVisualTransformation(),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
    )
    Spacer(Modifier.height(8.dp))
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Button(enabled = enabled && accessKeyId.isNotBlank() && secret.isNotBlank(), onClick = { onSave(accessKeyId, secret, sessionToken.ifBlank { null }) }) {
            Text(stringResource(Res.string.cloud_button_save_keys))
        }
        OutlinedButton(onClick = onCancel) { Text(stringResource(Res.string.common_cancel)) }
    }
}

/** The field is emptied when the form goes away: nothing here keeps what was pasted once it has been handed on. */
@Composable
private fun AzureSasForm(enabled: Boolean, onSave: (String) -> Unit, onCancel: () -> Unit) {
    var sas by remember { mutableStateOf("") }
    Spacer(Modifier.height(8.dp))
    Text(stringResource(Res.string.cloud_azure_sas_note), style = MaterialTheme.typography.bodySmall)
    OutlinedTextField(
        value = sas, onValueChange = { sas = it }, singleLine = true, modifier = Modifier.fillMaxWidth(),
        label = { Text(stringResource(Res.string.cloud_azure_sas_field)) },
        visualTransformation = PasswordVisualTransformation(),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
    )
    Spacer(Modifier.height(8.dp))
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Button(enabled = enabled && sas.isNotBlank(), onClick = { onSave(sas) }) { Text(stringResource(Res.string.cloud_button_save_sas)) }
        OutlinedButton(onClick = onCancel) { Text(stringResource(Res.string.common_cancel)) }
    }
}

/** The fields are emptied when the form goes away: nothing here keeps what was typed once it has been handed on. */
@Composable
private fun WebDavLoginForm(enabled: Boolean, onSave: (String, String, String) -> Unit, onCancel: () -> Unit) {
    var address by remember { mutableStateOf("") }
    var user by remember { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    Spacer(Modifier.height(8.dp))
    Text(stringResource(Res.string.cloud_webdav_note), style = MaterialTheme.typography.bodySmall)
    OutlinedTextField(
        value = address, onValueChange = { address = it }, singleLine = true, modifier = Modifier.fillMaxWidth(),
        label = { Text(stringResource(Res.string.cloud_webdav_address)) },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
    )
    OutlinedTextField(
        value = user, onValueChange = { user = it }, singleLine = true, modifier = Modifier.fillMaxWidth(),
        label = { Text(stringResource(Res.string.cloud_webdav_user)) },
    )
    OutlinedTextField(
        value = password, onValueChange = { password = it }, singleLine = true, modifier = Modifier.fillMaxWidth(),
        label = { Text(stringResource(Res.string.cloud_webdav_password)) },
        visualTransformation = PasswordVisualTransformation(),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
    )
    Spacer(Modifier.height(8.dp))
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Button(enabled = enabled && address.isNotBlank() && user.isNotBlank() && password.isNotEmpty(), onClick = { onSave(address, user, password) }) {
            Text(stringResource(Res.string.cloud_button_save_login))
        }
        OutlinedButton(onClick = onCancel) { Text(stringResource(Res.string.common_cancel)) }
    }
}

private fun CloudNotice.isProblem(): Boolean = when (kind) {
    CloudNoticeKind.SIGNED_IN, CloudNoticeKind.SIGNED_OUT, CloudNoticeKind.SIGNED_OUT_NOT_REVOKED, CloudNoticeKind.KEYS_SAVED, CloudNoticeKind.LOGIN_SAVED, CloudNoticeKind.SAS_SAVED -> false
    else -> true
}

@Composable
private fun noticeText(notice: CloudNotice): String {
    val name = notice.service?.let { stringResource(serviceName(it)) } ?: ""
    return when (notice.kind) {
        CloudNoticeKind.SIGNED_IN -> stringResource(Res.string.cloud_notice_signed_in, name)
        CloudNoticeKind.SIGNED_OUT -> stringResource(Res.string.cloud_notice_signed_out, name)
        CloudNoticeKind.SIGNED_OUT_NOT_REVOKED -> stringResource(Res.string.cloud_notice_signed_out_not_revoked, name)
        CloudNoticeKind.NOT_CONFIGURED -> stringResource(Res.string.cloud_notice_not_configured, name)
        CloudNoticeKind.DENIED -> stringResource(Res.string.cloud_notice_denied, name)
        CloudNoticeKind.FAILED -> stringResource(Res.string.cloud_notice_failed, name)
        CloudNoticeKind.NETWORK -> stringResource(Res.string.cloud_notice_network, name)
        CloudNoticeKind.UNREADABLE -> stringResource(Res.string.cloud_notice_unreadable)
        CloudNoticeKind.KEYS_SAVED -> stringResource(Res.string.cloud_notice_keys_saved, name)
        CloudNoticeKind.KEYS_INVALID -> stringResource(Res.string.cloud_notice_keys_invalid)
        CloudNoticeKind.LOGIN_SAVED -> stringResource(Res.string.cloud_notice_login_saved, name)
        CloudNoticeKind.LOGIN_INVALID -> stringResource(Res.string.cloud_notice_login_invalid)
        CloudNoticeKind.SAS_SAVED -> stringResource(Res.string.cloud_notice_sas_saved, name)
        CloudNoticeKind.SAS_INVALID -> stringResource(Res.string.cloud_notice_sas_invalid)
    }
}
