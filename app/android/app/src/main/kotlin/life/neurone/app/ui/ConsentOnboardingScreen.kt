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
import androidx.compose.ui.res.stringResource
import life.neurone.app.R

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
        Text(stringResource(R.string.consent_research_participation), style = MaterialTheme.typography.headlineMedium)
        Text(
            stringResource(R.string.consent_step_0_of_1_entirely_optional, step + 1, SCREEN_COUNT),
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
            }) { Text(stringResource(R.string.consent_skip_button)) }
            Spacer(Modifier.weight(1f))
            if (step > 0) {
                OutlinedButton(onClick = { step-- }) { Text(stringResource(R.string.consent_back_button)) }
            }
            Button(onClick = {
                if (step < SCREEN_COUNT - 1) {
                    step++
                } else {
                    store.updateResearchConsent(state)
                    onComplete()
                }
            }) { Text(if (step < SCREEN_COUNT - 1) stringResource(R.string.setup_continue_button) else stringResource(R.string.consent_finish)) }
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
        stringResource(R.string.consent_s1_heading),
        fontWeight = FontWeight.Medium,
    )
    Spacer(Modifier.height(8.dp))
    Text(
        stringResource(R.string.consent_a_study_that_uses_your_data_and_never_tells) +
            stringResource(R.string.consent_and_returned_nothing_these_two_options_are_t) +
            stringResource(R.string.consent_not_a_reward_for_taking_part_you_have_not_be) +
            stringResource(R.string.consent_that_comes_next),
        style = MaterialTheme.typography.bodySmall,
    )
    Spacer(Modifier.height(12.dp))
    SwitchRow(stringResource(R.string.consent_l4_results_toggle), state.resultsOptIn) {
        onChange(state.copy(resultsOptIn = it))
    }
    Text(
        stringResource(R.string.consent_l4_results_caption),
        style = MaterialTheme.typography.bodySmall,
    )
    Spacer(Modifier.height(8.dp))
    SwitchRow(stringResource(R.string.consent_l4_portal_toggle), state.suggestionPortalOptIn) {
        onChange(state.copy(suggestionPortalOptIn = it))
    }
    Text(
        stringResource(R.string.consent_submit_study_ideas_vote_on_priorities_expres),
        style = MaterialTheme.typography.bodySmall,
    )
    Spacer(Modifier.height(12.dp))
    Text(
        stringResource(R.string.consent_turning_these_on_grants_no_access_to_your_da) +
            stringResource(R.string.consent_nothing_every_device_function_works_identica),
        style = MaterialTheme.typography.bodySmall,
    )

    Spacer(Modifier.height(20.dp))
    Divider()
    Spacer(Modifier.height(20.dp))

    Text(stringResource(R.string.consent_s1_contact_heading), fontWeight = FontWeight.Medium)
    Spacer(Modifier.height(8.dp))
    Text(
        stringResource(R.string.consent_one_contact_method_covers_everything_above_p) +
            stringResource(R.string.consent_to_receive_on_the_next_screen_your_participa),
        style = MaterialTheme.typography.bodySmall,
    )
    Spacer(Modifier.height(8.dp))
    SwitchRow(stringResource(R.string.consent_l1_toggle), state.contactConsentGranted) {
        onChange(state.copy(contactConsentGranted = it))
    }
    if (state.contactConsentGranted) {
        Spacer(Modifier.height(12.dp))
        Text(stringResource(R.string.consent_how_often_at_most), style = MaterialTheme.typography.bodyMedium)
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
    Text(stringResource(R.string.consent_l2_heading), fontWeight = FontWeight.Medium)
    Text(stringResource(R.string.consent_each_study_is_still_a_separate_decision), style = MaterialTheme.typography.bodySmall)
    Spacer(Modifier.height(8.dp))

    // Select-all sets all nine categories and deliberately does NOT enable the blanket
    // toggle below (§6.2.3). The usability gap that leaves is closed with the note, not by
    // coupling the state.
    SwitchRow(stringResource(R.string.consent_s2_select_all_toggle), state.allCategoriesSelected) { on ->
        onChange(state.withAllCategories(on))
    }
    if (state.allCategoriesSelected && !state.blanketConsentGranted) {
        Text(
            stringResource(R.string.consent_you_will_still_be_asked_before_each_individu) +
                stringResource(R.string.consent_turn_on_the_setting_below),
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
    Text(stringResource(R.string.consent_s2_blanket_heading), fontWeight = FontWeight.Medium)
    Spacer(Modifier.height(8.dp))
    Text(
        stringResource(R.string.consent_by_default_we_ask_you_about_every_study_sepa) +
            stringResource(R.string.consent_can_hand_that_decision_over_instead),
        style = MaterialTheme.typography.bodySmall,
    )
    Spacer(Modifier.height(8.dp))
    SwitchRow(
        stringResource(R.string.consent_s2_blanket_toggle),
        state.blanketConsentGranted,
    ) {
        onChange(state.copy(blanketConsentGranted = it))
    }
    Text(
        stringResource(R.string.consent_you_will_still_get_a_notification_about_each) +
            stringResource(R.string.consent_a_question_you_can_opt_out_of_any_individual) +
            stringResource(R.string.consent_back_off_at_any_time),
        style = MaterialTheme.typography.bodySmall,
    )

    // Irreversibility notice — shown whenever the blanket control is on, on whichever screen
    // renders it (CLAUDE.md §6.2, L3 row).
    if (state.blanketConsentGranted) {
        Spacer(Modifier.height(12.dp))
        Text(stringResource(R.string.consent_important_label), fontWeight = FontWeight.Medium)
        Text(
            stringResource(R.string.consent_once_your_anonymized_data_has_been_included) +
                stringResource(R.string.consent_individually_withdrawn_from_that_dataset_how) +
                stringResource(R.string.consent_your_data_fresh_from_your_device_for_each_st) +
                stringResource(R.string.consent_and_permanently_stops_any_further_data_flowi) +
                stringResource(R.string.consent_data_from_sessions_that_occurred_before_your),
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
