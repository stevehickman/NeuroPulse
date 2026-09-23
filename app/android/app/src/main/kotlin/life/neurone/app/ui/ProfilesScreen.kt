package life.neurone.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import life.neurone.app.R
import life.neurone.core.protocol.NPIndividualProfile
import life.neurone.core.protocol.NPLimitsStore

// Port of the iOS LimitsSettingsView "Individual" tab's profile list: add a profile, choose which
// one is active, delete one. The active profile names the person using the device — the
// composition root forwards it to the hub as the active user, so a cervical VNS cardiac cutoff is
// held for that person only (NP-SW-FAULTMSG-001 §9.2). Deactivating names nobody: the device then
// keeps assuming whoever it last knew. Per-profile limit editing is a follow-up (OI-AND-LIMITS-01).

@Composable
fun ProfilesScreen(
    store: NPLimitsStore,
    onDone: () -> Unit,
    modifier: Modifier = Modifier,
) {
    // NPLimitsStore is not observable; mirror what this screen shows and refresh after each change.
    var profiles by remember { mutableStateOf(store.profiles) }
    var activeId by remember { mutableStateOf(store.activeProfileId) }
    var showAdd by remember { mutableStateOf(false) }
    var newName by remember { mutableStateOf("") }

    fun refresh() {
        profiles = store.profiles
        activeId = store.activeProfileId
    }

    Column(modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp)) {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onDone) { Text(stringResource(R.string.common_back)) }
            Text(stringResource(R.string.profile_picker_title), style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.padding(horizontal = 24.dp))
        }
        Spacer(Modifier.height(8.dp))
        Text(stringResource(R.string.profile_picker_hint), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(16.dp))

        Button(onClick = { showAdd = true }, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.limits_add_profile))
        }
        Spacer(Modifier.height(16.dp))

        if (profiles.isEmpty()) {
            Text(
                stringResource(R.string.limits_no_individual_profiles_configured),
                style = MaterialTheme.typography.bodyMedium,
            )
        } else {
            val active = profiles.firstOrNull { it.id == activeId }
            Text(stringResource(R.string.limits_active_profile), style = MaterialTheme.typography.titleSmall)
            Spacer(Modifier.height(4.dp))
            if (active != null) {
                ProfileRow(
                    profile = active,
                    isActive = true,
                    onToggleActive = { store.setActiveProfile(null); refresh() },
                    onDelete = { store.deleteProfile(active.id); refresh() },
                )
            } else {
                Text(stringResource(R.string.profile_picker_none_selected), style = MaterialTheme.typography.bodyMedium)
            }

            val inactive = profiles.filter { it.id != activeId }
            if (inactive.isNotEmpty()) {
                Spacer(Modifier.height(16.dp))
                Text(stringResource(R.string.limits_profiles), style = MaterialTheme.typography.titleSmall)
                inactive.forEach { profile ->
                    Spacer(Modifier.height(4.dp))
                    ProfileRow(
                        profile = profile,
                        isActive = false,
                        onToggleActive = { store.setActiveProfile(profile.id); refresh() },
                        onDelete = { store.deleteProfile(profile.id); refresh() },
                    )
                }
            }
        }
    }

    if (showAdd) {
        AlertDialog(
            onDismissRequest = { showAdd = false; newName = "" },
            title = { Text(stringResource(R.string.limits_add_profile)) },
            text = {
                Column {
                    Text(stringResource(R.string.limits_enter_a_name_for_this_individual_profile))
                    Spacer(Modifier.height(8.dp))
                    OutlinedTextField(
                        value = newName,
                        onValueChange = { newName = it },
                        label = { Text(stringResource(R.string.limits_profile_name)) },
                        singleLine = true,
                    )
                }
            },
            confirmButton = {
                TextButton(
                    enabled = newName.isNotBlank(),
                    onClick = {
                        store.saveProfile(NPIndividualProfile(name = newName.trim()))
                        newName = ""
                        showAdd = false
                        refresh()
                    },
                ) { Text(stringResource(R.string.ui_add)) }
            },
            dismissButton = {
                TextButton(onClick = { showAdd = false; newName = "" }) {
                    Text(stringResource(R.string.common_cancel))
                }
            },
        )
    }
}

@Composable
private fun ProfileRow(
    profile: NPIndividualProfile,
    isActive: Boolean,
    onToggleActive: () -> Unit,
    onDelete: () -> Unit,
) {
    Card(Modifier.fillMaxWidth()) {
        Row(
            Modifier.fillMaxWidth().padding(12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(Modifier.weight(1f)) {
                Text(profile.name, style = MaterialTheme.typography.titleMedium)
                if (isActive) {
                    Text(
                        stringResource(R.string.limits_active),
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.primary,
                    )
                }
                if (profile.notes.isNotEmpty()) {
                    Text(profile.notes, style = MaterialTheme.typography.bodySmall)
                }
            }
            Column(horizontalAlignment = Alignment.End) {
                OutlinedButton(onClick = onToggleActive) {
                    Text(stringResource(if (isActive) R.string.limits_deactivate else R.string.limits_set_active))
                }
                TextButton(onClick = onDelete) {
                    Text(stringResource(R.string.ui_delete), color = MaterialTheme.colorScheme.error)
                }
            }
        }
    }
}
