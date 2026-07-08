package com.neurone.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.neurone.core.models.ResearchCategory
import com.neurone.core.research.PledgeTier
import com.neurone.core.research.ResearchSuggestionDraft
import com.neurone.core.research.ResearchSuggestionStore

// Port of iOS ResearchSuggestionPortalView (CLAUDE.md §6.3): community research ideas with
// vote (function 1), participation intent (function 2), and pledge (function 3, intent only —
// no charge). Backend sync is a T1 stub; state is local. Reachable from the Privacy tab.

@Composable
fun ResearchPortalScreen(
    store: ResearchSuggestionStore,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var composing by remember { mutableStateOf(false) }
    var version by remember { mutableIntStateOf(0) } // bump to re-read the non-observable store

    if (composing) {
        NewSuggestion(
            onSubmit = { draft -> if (store.submit(draft)) { composing = false; version++ } },
            onCancel = { composing = false },
            modifier = modifier,
        )
        return
    }

    val suggestions = remember(version) { store.suggestions }

    Column(modifier.fillMaxSize().padding(16.dp)) {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onBack) { Text("Back") }
            Text("Research ideas", style = MaterialTheme.typography.titleLarge)
            TextButton(onClick = { composing = true }) { Text("New") }
        }
        Spacer(Modifier.height(8.dp))

        if (suggestions.isEmpty()) {
            Text(
                "Suggest a study you'd like to see, vote on others' ideas, and register interest " +
                    "in taking part. Pledges are intent only — you're never charged here.",
                style = MaterialTheme.typography.bodyMedium,
            )
            return@Column
        }

        Column(Modifier.verticalScroll(rememberScrollState())) {
            for (s in suggestions) {
                Card(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
                    Column(Modifier.padding(16.dp)) {
                        Text(s.title, style = MaterialTheme.typography.titleMedium)
                        Text(s.body, style = MaterialTheme.typography.bodySmall)
                        Text(
                            s.categories.joinToString(", ") { it.displayName },
                            style = MaterialTheme.typography.labelSmall,
                        )
                        Spacer(Modifier.height(8.dp))
                        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                            OutlinedButton(onClick = { store.toggleVote(s.id); version++ }) {
                                Text((if (s.didVote) "Voted " else "Vote ") + s.voteCount)
                            }
                            OutlinedButton(onClick = { store.toggleParticipationIntent(s.id); version++ }) {
                                Text(if (s.hasParticipationIntent) "Interested" else "I'd join")
                            }
                        }
                        Spacer(Modifier.height(4.dp))
                        Text(
                            "Pledged \$${s.pledgedAmount}" + (s.pledgeAmount?.let { " (you: \$$it)" } ?: ""),
                            style = MaterialTheme.typography.labelSmall,
                        )
                        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                            for (tier in PledgeTier.entries.filter { it != PledgeTier.NONE }) {
                                TextButton(onClick = { store.pledge(tier.amount, s.id); version++ }) {
                                    Text(tier.label)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun NewSuggestion(
    onSubmit: (ResearchSuggestionDraft) -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var title by remember { mutableStateOf("") }
    var body by remember { mutableStateOf("") }
    var categories by remember { mutableStateOf(setOf<ResearchCategory>()) }
    var intent by remember { mutableStateOf(false) }

    val draft = ResearchSuggestionDraft(title, body, categories, intent)

    Column(modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp)) {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onCancel) { Text("Cancel") }
            Text("New idea", style = MaterialTheme.typography.titleMedium)
            Button(onClick = { onSubmit(draft) }, enabled = draft.isValid) { Text("Submit") }
        }
        Spacer(Modifier.height(12.dp))
        OutlinedTextField(title, { title = it }, label = { Text("Title") }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(body, { body = it }, label = { Text("What should we study?") }, modifier = Modifier.fillMaxWidth())
        Spacer(Modifier.height(12.dp))
        Text("Research areas", style = MaterialTheme.typography.labelMedium)
        for (c in ResearchCategory.entries) {
            val checked = c in categories
            Row(
                Modifier
                    .fillMaxWidth()
                    .toggleable(value = checked, onValueChange = { on ->
                        categories = if (on) categories + c else categories - c
                    })
                    .padding(vertical = 2.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Checkbox(checked = checked, onCheckedChange = null)
                Spacer(Modifier.height(0.dp))
                Text(c.displayName, Modifier.padding(start = 8.dp))
            }
        }
        Spacer(Modifier.height(8.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Checkbox(checked = intent, onCheckedChange = { intent = it })
            Text("I would take part in this study")
        }
    }
}
