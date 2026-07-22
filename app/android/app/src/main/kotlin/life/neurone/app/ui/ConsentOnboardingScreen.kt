package life.neurone.app.ui

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
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.Button
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import life.neurone.core.consent.ConsentStore
import life.neurone.core.models.ContactFrequency
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.ResearchConsentState

// Port of iOS ConsentOnboardingView — the a priori research-consent flow (CLAUDE.md §6.2).
// Four layers: L1 contact, L2 categories, L3 blanket (with irreversibility notice), L4
// results + community. All optional — every device function works without research consent.
// On finish the built ResearchConsentState is persisted via ConsentStore.updateResearchConsent.

@Composable
fun ConsentOnboardingScreen(
    store: ConsentStore,
    onComplete: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var step by remember { mutableIntStateOf(0) }
    var state by remember { mutableStateOf(ResearchConsentState()) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
    ) {
        Text("Research participation", style = MaterialTheme.typography.headlineMedium)
        Text("Step ${step + 1} of 4 — entirely optional", style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(16.dp))

        when (step) {
            0 -> LayerContact(state) { state = it }
            1 -> LayerCategories(state) { state = it }
            2 -> LayerBlanket(state) { state = it }
            else -> LayerResults(state) { state = it }
        }

        Spacer(Modifier.height(24.dp))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TextButton(onClick = {
                // Skip the rest — persist whatever has been chosen so far (may be none).
                store.updateResearchConsent(state)
                onComplete()
            }) { Text("Skip") }
            Spacer(Modifier.weight(1f))
            if (step > 0) {
                OutlinedButton(onClick = { step-- }) { Text("Back") }
            }
            Button(onClick = {
                if (step < 3) {
                    step++
                } else {
                    store.updateResearchConsent(state)
                    onComplete()
                }
            }) { Text(if (step < 3) "Continue" else "Finish") }
        }
    }
}

@Composable
private fun LayerContact(state: ResearchConsentState, onChange: (ResearchConsentState) -> Unit) {
    Text("Can we contact you about future research opportunities?", fontWeight = FontWeight.Medium)
    Spacer(Modifier.height(8.dp))
    SwitchRow("Contact me about research", state.contactConsentGranted) {
        onChange(state.copy(contactConsentGranted = it))
    }
    if (state.contactConsentGranted) {
        Spacer(Modifier.height(12.dp))
        Text("How often, at most?", style = MaterialTheme.typography.bodyMedium)
        ContactFrequency.entries.forEach { freq ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                RadioButton(
                    selected = state.contactFrequency == freq,
                    onClick = { onChange(state.copy(contactFrequency = freq)) },
                )
                Text(freq.name.lowercase().replaceFirstChar { it.uppercase() })
            }
        }
    }
}

@Composable
private fun LayerCategories(state: ResearchConsentState, onChange: (ResearchConsentState) -> Unit) {
    Text("Which research areas interest you?", fontWeight = FontWeight.Medium)
    Text("Each study is still a separate decision.", style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))
    ResearchCategory.entries.forEach { category ->
        val checked = state.categoryConsents[category] ?: false
        Row(
            Modifier
                .fillMaxWidth()
                .toggleable(value = checked, onValueChange = { on ->
                    onChange(state.copy(categoryConsents = state.categoryConsents + (category to on)))
                })
                .padding(vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Checkbox(checked = checked, onCheckedChange = null)
            Spacer(Modifier.width(8.dp))
            Text(category.displayName)
        }
    }
}

@Composable
private fun LayerBlanket(state: ResearchConsentState, onChange: (ResearchConsentState) -> Unit) {
    Text("Pre-approve all NeurOne-reviewed research?", fontWeight = FontWeight.Medium)
    Spacer(Modifier.height(8.dp))
    SwitchRow("Include my anonymized data in all reviewed studies", state.blanketConsentGranted) {
        onChange(state.copy(blanketConsentGranted = it))
    }
    Spacer(Modifier.height(12.dp))
    Text(
        "Once your anonymized data has been included in a published study, it cannot be " +
            "individually withdrawn from that dataset. However, because NeurOne anonymises " +
            "your data fresh from your device for each study, withdrawing consent immediately " +
            "and permanently stops any further data flowing to any future dataset — including " +
            "data from sessions that occurred before your withdrawal.",
        style = MaterialTheme.typography.bodySmall,
    )
}

@Composable
private fun LayerResults(state: ResearchConsentState, onChange: (ResearchConsentState) -> Unit) {
    Text("Stay in the loop", fontWeight = FontWeight.Medium)
    Spacer(Modifier.height(8.dp))
    SwitchRow("Tell me about study results (including null results)", state.resultsOptIn) {
        onChange(state.copy(resultsOptIn = it))
    }
    Spacer(Modifier.height(8.dp))
    SwitchRow("Let me suggest and vote on research ideas", state.suggestionPortalOptIn) {
        onChange(state.copy(suggestionPortalOptIn = it))
    }
}

@Composable
private fun SwitchRow(label: String, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(
        Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, Modifier.weight(1f))
        Switch(checked = checked, onCheckedChange = onChange)
    }
}
