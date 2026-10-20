package life.neurone.shared.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.AssistChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.NPBundledConditions
import life.neurone.core.protocol.NPLinkReason
import life.neurone.core.protocol.NPResolvedCondition
import life.neurone.core.protocol.resolve
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

// Condition chips and the "leaving the app" confirmation (NP-COND-LINK-001 §4), ported from the web ConditionLinkDialog and the
// iOS ConditionLinkView. Choosing a condition never navigates directly: the destination host is a third party that would learn the
// person looked up that condition, so they see the host and decide. A link reaches the system browser only after it passed
// NPConditionLinkPolicy, and only from this dialog.
//
// Nothing here logs. Which conditions a person looks up is health-adjacent, so UHDR-class under the §5.1 boundary rule ("when in
// doubt, UHDR"); the cleanest way to honour that is to record nothing: no analytics event, no SHDR field (NP-COND-LINK-001 §2).

/** The words for a refused link; the core keeps English text only for its own diagnostics, so the screen resolves a locale key. */
private fun blockedText(reason: NPLinkReason): StringResource? = when (reason) {
    NPLinkReason.OK -> null
    NPLinkReason.NOT_ABSOLUTE -> Res.string.ui_condition_blocked_not_absolute
    NPLinkReason.SCHEME_NOT_HTTPS -> Res.string.ui_condition_blocked_not_https
    NPLinkReason.EMBEDDED_CREDENTIALS -> Res.string.ui_condition_blocked_credentials
    NPLinkReason.MISSING_HOST -> Res.string.ui_condition_blocked_missing_host
    NPLinkReason.LOCAL_OR_PRIVATE_HOST -> Res.string.ui_condition_blocked_private_host
    NPLinkReason.MALFORMED -> Res.string.ui_condition_blocked_malformed
}

/** One chip per condition a protocol names. A condition whose link is refused is still listed (struck through): it is valid metadata. */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun ConditionChips(conditions: List<String>, modifier: Modifier = Modifier) {
    if (conditions.isEmpty()) return
    val resolved = remember(conditions) { NPBundledConditions.resolve(conditions) }
    var selected by remember { mutableStateOf<NPResolvedCondition?>(null) }

    FlowRow(modifier, horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        resolved.forEach { c ->
            val allowed = c.verdict.allowed
            val description = if (allowed) {
                stringResource(Res.string.condition_link_a11y_opens, c.name, c.verdict.host ?: "")
            } else {
                c.name + ". " + (blockedText(c.verdict.reason)?.let { stringResource(it) } ?: "")
            }
            AssistChip(
                onClick = { selected = c },
                label = {
                    Text(
                        if (allowed) c.name + " ↗" else c.name,
                        style = MaterialTheme.typography.labelMedium,
                        textDecoration = if (allowed) null else TextDecoration.LineThrough,
                    )
                },
                modifier = Modifier.alpha(if (allowed) 1f else 0.55f).semantics { contentDescription = description },
            )
        }
    }

    selected?.let { ConditionLinkConfirmation(it, onDismiss = { selected = null }) }
}

@Composable
fun ConditionLinkConfirmation(condition: NPResolvedCondition, onDismiss: () -> Unit) {
    val verdict = condition.verdict
    val uriHandler = LocalUriHandler.current
    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Column {
                Text(condition.name)
                condition.definition?.code?.let {
                    Text(stringResource(Res.string.ui_condition_icd11_code, it), style = MaterialTheme.typography.labelMedium)
                }
            }
        },
        text = {
            Column(Modifier.fillMaxWidth()) {
                condition.definition?.description?.let {
                    Text(it, style = MaterialTheme.typography.bodyMedium)
                    Spacer(Modifier.height(12.dp))
                }
                if (verdict.allowed) {
                    Text(stringResource(Res.string.ui_condition_leaving_notice), style = MaterialTheme.typography.bodySmall)
                    Spacer(Modifier.height(12.dp))
                    // The host, not the whole address: the host is the security-relevant part.
                    Text(stringResource(Res.string.condition_link_destination), style = MaterialTheme.typography.labelSmall)
                    Text(verdict.host ?: "", style = MaterialTheme.typography.bodyLarge, modifier = Modifier.padding(top = 2.dp))
                } else {
                    val reason = blockedText(verdict.reason)?.let { stringResource(it) } ?: ""
                    val missing = if (condition.definition == null) " " + stringResource(Res.string.condition_link_not_in_registry) else ""
                    Text(reason + missing, color = MaterialTheme.colorScheme.tertiary, style = MaterialTheme.typography.bodyMedium)
                }
            }
        },
        confirmButton = {
            if (verdict.allowed) {
                TextButton(onClick = {
                    // `url` is non-null exactly when the verdict is allowed, and it has passed NPConditionLinkPolicy: this is the
                    // only place a condition link reaches the system.
                    verdict.url?.let { uriHandler.openUri(it) }
                    onDismiss()
                }) { Text(stringResource(Res.string.ui_condition_open_in_browser)) }
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(if (verdict.allowed) Res.string.common_cancel else Res.string.common_close))
            }
        },
    )
}
