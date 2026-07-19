package com.neurone.app.ui

import android.content.ActivityNotFoundException
import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.AssistChip
import androidx.compose.material3.AssistChipDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import com.neurone.core.protocol.NPBundledConditions
import com.neurone.core.protocol.NPResolvedCondition
import com.neurone.core.protocol.resolve

/**
 * Condition chips + the "leaving the app" confirmation — NP-COND-LINK-001 §4.
 *
 * Nothing here logs. Which conditions a user looks up is health-adjacent and
 * therefore UHDR-class under the §5.1 boundary rule ("when in doubt → UHDR"),
 * so it is not recorded anywhere — no analytics event, no SHDR field.
 * See NP-COND-LINK-001 §2.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun ConditionChips(
    conditions: List<String>,
    modifier: Modifier = Modifier,
) {
    if (conditions.isEmpty()) return

    val resolved = remember(conditions) { NPBundledConditions.resolve(conditions) }
    var selected by remember { mutableStateOf<NPResolvedCondition?>(null) }

    FlowRow(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        for (condition in resolved) {
            val allowed = condition.verdict.allowed
            AssistChip(
                onClick = { selected = condition },
                label = {
                    Text(
                        text = condition.name,
                        style = MaterialTheme.typography.labelMedium,
                        textDecoration = if (allowed) null else TextDecoration.LineThrough,
                    )
                },
                trailingIcon = if (allowed) {
                    { Text("↗", style = MaterialTheme.typography.labelSmall) }
                } else {
                    null
                },
                colors = AssistChipDefaults.assistChipColors(
                    labelColor = if (allowed) {
                        MaterialTheme.colorScheme.onSurface
                    } else {
                        MaterialTheme.colorScheme.onSurfaceVariant
                    },
                ),
            )
        }
    }

    selected?.let { condition ->
        ConditionLinkConfirmation(
            condition = condition,
            onDismiss = { selected = null },
        )
    }
}

@Composable
private fun ConditionLinkConfirmation(
    condition: NPResolvedCondition,
    onDismiss: () -> Unit,
) {
    val context = LocalContext.current
    val verdict = condition.verdict

    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Column {
                Text(condition.name, style = MaterialTheme.typography.titleMedium)
                condition.definition?.code?.let { code ->
                    Text(
                        "ICD-11 $code",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                condition.definition?.description?.let { Text(it) }

                if (verdict.allowed) {
                    Text(
                        "This opens an external site in your browser. NeurOne does not " +
                            "record which conditions you look up.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )

                    // The host, not the full URL — the host is the security-relevant
                    // part, and a full Wikipedia URL wraps badly on a phone dialog.
                    Surface(
                        color = MaterialTheme.colorScheme.surfaceVariant,
                        shape = MaterialTheme.shapes.small,
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Column(Modifier.padding(12.dp)) {
                            Text(
                                "DESTINATION",
                                style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                            Text(
                                verdict.host.orEmpty(),
                                style = MaterialTheme.typography.bodyLarge,
                            )
                        }
                    }
                } else {
                    Text(
                        buildString {
                            append(verdict.reason.blockedMessage)
                            if (condition.definition == null) {
                                append(" This condition is not in the registry.")
                            }
                        },
                        color = MaterialTheme.colorScheme.error,
                    )
                }
            }
        },
        confirmButton = {
            if (verdict.allowed) {
                TextButton(onClick = {
                    openInExternalBrowser(context, verdict.url)
                    onDismiss()
                }) {
                    Text("Open in browser")
                }
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(if (verdict.allowed) "Cancel" else "Close")
            }
        },
    )
}

/**
 * Hand an approved URL to the system browser.
 *
 * `url` is non-null exactly when the verdict is allowed, and it has already
 * passed NPConditionLinkPolicy — this is the only place a condition link reaches
 * the OS. FLAG_ACTIVITY_NEW_TASK keeps the browser out of NeurOne's task stack,
 * so the back gesture does not walk the user back through the external site.
 */
private fun openInExternalBrowser(
    context: android.content.Context,
    url: String?,
) {
    if (url.isNullOrEmpty()) return

    val intent = Intent(Intent.ACTION_VIEW, Uri.parse(url)).apply {
        addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    }
    try {
        context.startActivity(intent)
    } catch (_: ActivityNotFoundException) {
        // No browser installed. Nothing to fall back to, and nothing worth
        // logging — the user simply stays in the app.
    }
}
