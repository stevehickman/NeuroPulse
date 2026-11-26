package life.neurone.shared.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import life.neurone.core.analytics.GeneralConsentAnswer
import life.neurone.core.analytics.GeneralConsentPrompt
import life.neurone.core.analytics.GeneralConsentPromptKind
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

// The warranty owner's two questions about general SHDR consent (REQ-SHDR-12; OI-NVRAM-26, OI-NVRAM-28). The owner may be a
// clinic, an institution or one person at home (CLAUDE.md §6.0); nothing here asks who is using the helmet or touches a user's
// consent.

/**
 * The plain words for each item a general-consent content set can add, by the id the set names (`GeneralShdrContent`). A Compose
 * resource is found by a static reference and not by a name looked up at run time, so this is the one table from an item id to
 * its words. An app update that ships a set the review gate fixed adds its items' words here and in `locales/<bcp47>.json` in the
 * same change; a test holds every id in the shipped history to this table. Empty until the gate rules: the baseline adds nothing.
 */
internal val GENERAL_CONSENT_ITEM_NAMES: Map<String, StringResource> = emptyMap()

/**
 * One of the warranty owner's general-consent prompts for the connected helmet. [itemNames] gives the words for each item in
 * [GeneralConsentPrompt.added]; the caller shows no prompt whose items it cannot all name.
 *
 * It is a fresh question and the default is no change: closing it, by the back gesture or a tap outside, answers `null`, which
 * changes nothing. "Agree" is offered only when it would change something (reports off, or content added), and it asks the person
 * to confirm they may decide for the helmet, as turning reports on does on the page; with reports off it repeats the relay notice
 * turning on shows (`NP-INFRA-001` §3.3a).
 */
@Composable
fun GeneralConsentPromptDialog(
    prompt: GeneralConsentPrompt,
    itemNames: List<String>,
    onAnswer: (GeneralConsentAnswer?) -> Unit,
) {
    var mayDecide by remember(prompt) { mutableStateOf(false) }
    val window = prompt.kind == GeneralConsentPromptKind.WINDOW_ENDED
    val agreeChanges = !prompt.reportsOn || prompt.added.isNotEmpty()

    AlertDialog(
        onDismissRequest = { onAnswer(null) },
        title = {
            Text(stringResource(if (window) Res.string.general_consent_window_title else Res.string.general_consent_content_title))
        },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState())) {
                Text(stringResource(if (window) Res.string.general_consent_window_body else Res.string.general_consent_content_body))
                Spacer(Modifier.height(8.dp))
                Text(
                    stringResource(if (prompt.reportsOn) Res.string.warranty_fleet_status_on else Res.string.warranty_fleet_status_off),
                    style = MaterialTheme.typography.bodyLarge,
                )
                Spacer(Modifier.height(8.dp))
                Text(stringResource(Res.string.general_consent_baseline), style = MaterialTheme.typography.bodyMedium)
                Spacer(Modifier.height(8.dp))
                if (itemNames.isEmpty()) {
                    Text(stringResource(Res.string.general_consent_nothing_added), style = MaterialTheme.typography.bodyMedium)
                } else {
                    Text(stringResource(Res.string.general_consent_added_heading), style = MaterialTheme.typography.bodyMedium)
                    itemNames.forEach { Text(it, style = MaterialTheme.typography.bodyMedium, modifier = Modifier.padding(start = 16.dp)) }
                }
                if (!prompt.reportsOn) {
                    Spacer(Modifier.height(8.dp))
                    Text(stringResource(Res.string.warranty_fleet_notice_body), style = MaterialTheme.typography.bodySmall)
                }
                Spacer(Modifier.height(8.dp))
                Text(stringResource(Res.string.general_consent_default), style = MaterialTheme.typography.bodySmall)
                if (agreeChanges) {
                    Spacer(Modifier.height(12.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(checked = mayDecide, onCheckedChange = { mayDecide = it })
                        Text(stringResource(Res.string.warranty_fleet_notice_confirm), style = MaterialTheme.typography.bodyMedium)
                    }
                }
            }
        },
        confirmButton = {
            if (agreeChanges) {
                TextButton(enabled = mayDecide, onClick = { onAnswer(GeneralConsentAnswer.AGREE) }) {
                    Text(stringResource(Res.string.general_consent_agree))
                }
            }
        },
        dismissButton = {
            Row {
                if (prompt.reportsOn) {
                    TextButton(onClick = { onAnswer(GeneralConsentAnswer.STOP) }) { Text(stringResource(Res.string.general_consent_stop)) }
                }
                TextButton(onClick = { onAnswer(GeneralConsentAnswer.KEEP) }) { Text(stringResource(Res.string.general_consent_keep)) }
            }
        },
    )
}
