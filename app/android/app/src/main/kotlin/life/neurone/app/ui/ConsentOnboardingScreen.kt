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
import androidx.compose.material3.Divider
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
//
// LAYERS ARE NOT SCREENS. The four consent layers are unchanged; since Rev 37 they are
// presented across two screens:
//   S1 "What you get back" — L4 (results + portal) then L1 (contact consent)
//   S2 "What you share"    — L2 (categories) and L3 (blanket posture), two controls
//
// S1 comes first because L1 and L4 were always the same question — L1 asks whether we may
// contact you, L4 asks what about (§6.2.1). S2 keeps two controls because L2 is scope and L3
// is posture; "everything, but ask me" survives only while both do (§6.2.2). Select-all sets
// the nine categories and deliberately does NOT touch the blanket toggle (§6.2.3).
//
// All optional — every device function works without research consent. On finish the built
// ResearchConsentState is persisted via ConsentStore.updateResearchConsent, which is also
// where a blanket true→false transition triggers the research-analytics teardown (§6.2.5).

private const val SCREEN_COUNT = 2

@Composable
fun ConsentOnboardingScreen(
    store: ConsentStore,
    onComplete: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var step by remember { mutableIntStateOf(0) }
    // Seed from committed state: this screen is re-entrant as Research Preferences, so
    // starting blank would silently clear every prior consent decision on Finish.
    var state by remember { mutableStateOf(store.researchConsent) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
    ) {
        Text("Research participation", style = MaterialTheme.typography.headlineMedium)
        Text(
            "Step ${step + 1} of $SCREEN_COUNT — entirely optional",
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(16.dp))

        when (step) {
            0 -> ScreenWhatYouGetBack(state) { state = it }
            else -> ScreenWhatYouShare(state) { state = it }
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
                if (step < SCREEN_COUNT - 1) {
                    step++
                } else {
                    store.updateResearchConsent(state)
                    onComplete()
                }
            }) { Text(if (step < SCREEN_COUNT - 1) "Continue" else "Finish") }
        }
    }
}

/**
 * S1 — what you get back (L4, then L1).
 *
 * L4 leads, under the §6.2.4 copy rules: conditional framing ("if your data ever
 * contributes"), the exchange stated as symmetric rather than as a reward, and non-coercion
 * stated on the screen. L1 follows because a contact method is the precondition for
 * delivering any of it.
 */
@Composable
private fun ScreenWhatYouGetBack(
    state: ResearchConsentState,
    onChange: (ResearchConsentState) -> Unit,
) {
    Text(
        "If your data ever contributes to a study, here is what comes back to you.",
        fontWeight = FontWeight.Medium,
    )
    Spacer(Modifier.height(8.dp))
    Text(
        "A study that uses your data and never tells you what it found has taken something " +
            "and returned nothing. These two options are the other half of that exchange — " +
            "not a reward for taking part. You have not been asked to share anything yet; " +
            "that comes next.",
        style = MaterialTheme.typography.bodySmall,
    )
    Spacer(Modifier.height(12.dp))
    SwitchRow("Receive study results when they're published", state.resultsOptIn) {
        onChange(state.copy(resultsOptIn = it))
    }
    Text(
        "Plain-language summaries, including null results. Never marketing.",
        style = MaterialTheme.typography.bodySmall,
    )
    Spacer(Modifier.height(8.dp))
    SwitchRow("Join the research suggestion portal", state.suggestionPortalOptIn) {
        onChange(state.copy(suggestionPortalOptIn = it))
    }
    Text(
        "Submit study ideas, vote on priorities, express interest in participating.",
        style = MaterialTheme.typography.bodySmall,
    )
    Spacer(Modifier.height(12.dp))
    Text(
        "Turning these on grants no access to your data, and turning them off costs you " +
            "nothing. Every device function works identically either way.",
        style = MaterialTheme.typography.bodySmall,
    )

    Spacer(Modifier.height(20.dp))
    Divider()
    Spacer(Modifier.height(20.dp))

    Text("How would we reach you?", fontWeight = FontWeight.Medium)
    Spacer(Modifier.height(8.dp))
    Text(
        "One contact method covers everything above, plus any study invitations you choose " +
            "to receive on the next screen. Your participation is always voluntary.",
        style = MaterialTheme.typography.bodySmall,
    )
    Spacer(Modifier.height(8.dp))
    SwitchRow("Yes, you can contact me", state.contactConsentGranted) {
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

/**
 * S2 — what you share (L2 + L3, two controls on one screen).
 *
 * The two controls are separate because the axes are: L2 is scope, L3 is posture (§6.2.2).
 */
@Composable
private fun ScreenWhatYouShare(
    state: ResearchConsentState,
    onChange: (ResearchConsentState) -> Unit,
) {
    // ── L2: scope ────────────────────────────────────────────────────────
    Text("Which research areas interest you?", fontWeight = FontWeight.Medium)
    Text("Each study is still a separate decision.", style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))

    // Select-all sets all nine categories and deliberately does NOT enable the blanket
    // toggle below (§6.2.3). The usability gap that leaves is closed with the note, not by
    // coupling the state.
    SwitchRow("Select all nine areas", state.allCategoriesSelected) { on ->
        onChange(state.withAllCategories(on))
    }
    if (state.allCategoriesSelected && !state.blanketConsentGranted) {
        Text(
            "You will still be asked before each individual study. To stop being asked, " +
                "turn on the setting below.",
            style = MaterialTheme.typography.bodySmall,
        )
    }
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

    Spacer(Modifier.height(20.dp))
    Divider()
    Spacer(Modifier.height(20.dp))

    // ── L3: posture ──────────────────────────────────────────────────────
    Text("Do you want to be asked about each study?", fontWeight = FontWeight.Medium)
    Spacer(Modifier.height(8.dp))
    Text(
        "By default we ask you about every study separately, and you decide each time. You " +
            "can hand that decision over instead.",
        style = MaterialTheme.typography.bodySmall,
    )
    Spacer(Modifier.height(8.dp))
    SwitchRow(
        "Stop asking me — include my anonymised data in every reviewed study",
        state.blanketConsentGranted,
    ) {
        onChange(state.copy(blanketConsentGranted = it))
    }
    Text(
        "You will still get a notification about each study, but it will be news rather than " +
            "a question. You can opt out of any individual study, and you can switch this " +
            "back off at any time.",
        style = MaterialTheme.typography.bodySmall,
    )

    // Irreversibility notice — shown whenever the blanket control is on, on whichever screen
    // renders it (CLAUDE.md §6.2, L3 row).
    if (state.blanketConsentGranted) {
        Spacer(Modifier.height(12.dp))
        Text("Important — please read", fontWeight = FontWeight.Medium)
        Text(
            "Once your anonymized data has been included in a published study, it cannot be " +
                "individually withdrawn from that dataset. However, because NeurOne anonymises " +
                "your data fresh from your device for each study, withdrawing consent immediately " +
                "and permanently stops any further data flowing to any future dataset — including " +
                "data from sessions that occurred before your withdrawal.",
            style = MaterialTheme.typography.bodySmall,
        )
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
