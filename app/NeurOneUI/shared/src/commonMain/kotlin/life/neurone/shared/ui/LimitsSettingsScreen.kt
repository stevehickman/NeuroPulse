package life.neurone.shared.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.key
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.NPBESTacsLimits
import life.neurone.core.protocol.NPLimitsSet
import life.neurone.core.protocol.NPLimitsStore
import life.neurone.core.protocol.NPPBMTranscranialLimits
import life.neurone.core.protocol.NPTDCSLimits
import life.neurone.core.protocol.NPVNSHRVLimits
import life.neurone.core.protocol.NPVisualStimLimits
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.*

// Port of iOS LimitsSettingsView: the three limit tiers (global, per-helmet, per-individual). Each tier
// edits the same safety-relevant dosage caps. They layer under the firmware hardware ceilings
// (NPHardwareLimits, which can never be exceeded) and are enforced by the validator, field by field,
// individual over helmet over global. Blank field = no configured cap for that value.
// A helmet is the connected hub (its stable hashed identity), so no serial is typed. The remaining
// modality fields are a follow-up (OI-AND-LIMITS-01).

private enum class LimitsTier(val label: StringResource) {
    GLOBAL(Res.string.web_tab_global),
    HELMET(Res.string.web_tab_helmet),
    INDIVIDUAL(Res.string.web_tab_individual),
}

@Composable
fun LimitsSettingsScreen(
    store: NPLimitsStore,
    /** The connected hub's identity, or null when none is connected. */
    hubId: String?,
    onDone: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var tier by remember { mutableStateOf(LimitsTier.GLOBAL) }
    // The tiers live in the store; this counter re-reads them after a save or a clear.
    var revision by remember { mutableStateOf(0) }
    Column(modifier.fillMaxSize()) {
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onDone) { Text(stringResource(Res.string.consent_done_button)) }
            Text(stringResource(Res.string.limits_dosage_limits), style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.width(64.dp))
        }
        TabRow(selectedTabIndex = tier.ordinal) {
            LimitsTier.entries.forEach {
                Tab(selected = tier == it, onClick = { tier = it }, text = { Text(stringResource(it.label)) })
            }
        }
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp)) {
            key(tier, revision) {
                when (tier) {
                    LimitsTier.GLOBAL -> {
                        val globalName = stringResource(Res.string.limits_chain_global)
                        Text(
                            stringResource(Res.string.limits_global_caps_layer_under_the_firmware_hardwar) +
                                stringResource(Res.string.limits_leave_a_field_blank_for_no_configured_cap),
                            style = MaterialTheme.typography.bodySmall,
                        )
                        Spacer(Modifier.height(12.dp))
                        LimitsTierEditor(
                            initial = store.globalLimits,
                            name = globalName,
                            clearLabel = stringResource(Res.string.limits_clear_all_global_limits),
                            exportScript = store::exportLimitsAsNPPS,
                            importScript = store::importLimitsFromNPPS,
                            onSave = { store.saveGlobalLimits(it); revision++ },
                            onClear = { store.clearGlobalLimits(); revision++ },
                        )
                    }
                    LimitsTier.HELMET -> HelmetTier(store, hubId) { revision++ }
                    LimitsTier.INDIVIDUAL -> IndividualTier(store) { revision++ }
                }
            }
        }
    }
}

@Composable
private fun HelmetTier(store: NPLimitsStore, hubId: String?, changed: () -> Unit) {
    if (hubId == null) {
        Text(stringResource(Res.string.limits_connect_a_helmet_to_set_its_limits), style = MaterialTheme.typography.bodyMedium)
    } else {
        Text(stringResource(Res.string.limits_saved_for_this_helmet), style = MaterialTheme.typography.titleSmall)
        Spacer(Modifier.height(8.dp))
        LimitsTierEditor(
            initial = store.helmetLimits[hubId],
            name = hubId,
            clearLabel = stringResource(Res.string.limits_clear_these_limits),
            exportScript = store::exportLimitsAsNPPS,
            importScript = store::importLimitsFromNPPS,
            onSave = { store.saveHelmetLimits(it, hubId); changed() },
            onClear = { store.clearHelmetLimits(hubId); changed() },
        )
    }
    val others = store.helmetLimits.keys.filter { it != hubId }
    if (others.isEmpty() && hubId == null && store.helmetLimits.isEmpty()) {
        Spacer(Modifier.height(12.dp))
        Text(stringResource(Res.string.limits_no_helmet_specific_limits_configured), style = MaterialTheme.typography.bodySmall)
    }
    if (others.isNotEmpty()) {
        Spacer(Modifier.height(16.dp))
        Text(stringResource(Res.string.limits_configured_helmets), style = MaterialTheme.typography.titleSmall)
        others.forEach { id ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Text(id.take(8), Modifier.weight(1f))
                TextButton(onClick = { store.clearHelmetLimits(id); changed() }) {
                    Text(stringResource(Res.string.limits_clear_these_limits), color = MaterialTheme.colorScheme.error)
                }
            }
        }
    }
}

@Composable
private fun IndividualTier(store: NPLimitsStore, changed: () -> Unit) {
    val profiles = store.profiles
    if (profiles.isEmpty()) {
        Text(stringResource(Res.string.limits_choose_a_profile_to_set_its_limits), style = MaterialTheme.typography.bodyMedium)
        return
    }
    var selectedId by remember { mutableStateOf(store.activeProfileId ?: profiles.first().id) }
    profiles.forEach { p ->
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            androidx.compose.material3.RadioButton(selected = p.id == selectedId, onClick = { selectedId = p.id })
            Text(p.name)
        }
    }
    val profile = profiles.firstOrNull { it.id == selectedId } ?: return
    Spacer(Modifier.height(8.dp))
    Text(stringResource(Res.string.limits_saved_for_this_person, profile.name), style = MaterialTheme.typography.titleSmall)
    Spacer(Modifier.height(8.dp))
    key(profile.id) {
        LimitsTierEditor(
            initial = store.individualLimits[profile.id],
            name = profile.name,
            clearLabel = stringResource(Res.string.limits_clear_these_limits),
            exportScript = store::exportLimitsAsNPPS,
            importScript = store::importLimitsFromNPPS,
            onSave = { store.saveIndividualLimits(it, profile.id); changed() },
            onClear = { store.clearIndividualLimits(profile.id); changed() },
        )
    }
}
