package com.neuropulse.core.research

import com.neuropulse.core.common.KeyValueStore
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.json.Json
import java.time.LocalDate
import java.util.UUID

// Port of iOS ResearchSuggestionStore. Local suggestion state + optimistic vote/intent/pledge
// updates; backend sync is a T1 stub. Persists via KeyValueStore (parallel of iOS UserDefaults
// + a Keychain device token). Pure-JVM, so the vote/pledge accounting is unit-testable.
class ResearchSuggestionStore(
    private val store: KeyValueStore,
    private val json: Json = Json { ignoreUnknownKeys = true },
    private val today: () -> String = { LocalDate.now().toString() },
) {
    companion object {
        const val STORAGE_KEY = "np.research.suggestions"
        const val TOKEN_KEY = "np.research.device-token"
    }

    var suggestions: List<ResearchSuggestion> = emptyList()
        private set

    init {
        load()
    }

    /** Function 1 — submit a new idea. Returns false for an invalid draft (no insert). */
    fun submit(draft: ResearchSuggestionDraft): Boolean {
        if (!draft.isValid) return false
        val suggestion = ResearchSuggestion(
            title = draft.title.trim(),
            body = draft.body.trim(),
            categories = draft.categories.toList(),
            submittedAtDay = today(),
            authorDeviceToken = deviceToken(),
            participationIntentCount = if (draft.hasParticipationIntent) 1 else 0,
            pledgedAmount = draft.pledgeAmount,
            hasParticipationIntent = draft.hasParticipationIntent,
            pledgeAmount = if (draft.pledgeAmount > 0) draft.pledgeAmount else null,
        )
        suggestions = listOf(suggestion) + suggestions
        save()
        return true
    }

    /** Function 1 — community up-vote toggle. */
    fun toggleVote(id: String) = mutate(id) { s ->
        val wasVoting = s.didVote
        s.copy(didVote = !wasVoting, voteCount = s.voteCount + if (wasVoting) -1 else 1)
    }

    /** Function 2 — pre-identified subject pool intent toggle. */
    fun toggleParticipationIntent(id: String) = mutate(id) { s ->
        val was = s.hasParticipationIntent
        s.copy(
            hasParticipationIntent = !was,
            participationIntentCount = s.participationIntentCount + if (was) -1 else 1,
        )
    }

    /** Function 3 — crowdfunding pledge (intent only — no charge until a campaign activates). */
    fun pledge(amount: Int, id: String) = mutate(id) { s ->
        val previous = s.pledgeAmount ?: 0
        s.copy(pledgeAmount = amount, pledgedAmount = s.pledgedAmount - previous + amount)
    }

    private inline fun mutate(id: String, transform: (ResearchSuggestion) -> ResearchSuggestion) {
        suggestions = suggestions.map { if (it.id == id) transform(it) else it }
        save()
    }

    // MARK: Device token (opaque, persisted, no identity)

    private fun deviceToken(): String {
        store.getString(TOKEN_KEY)?.let { return it }
        val token = UUID.randomUUID().toString()
        store.putString(TOKEN_KEY, token)
        return token
    }

    // MARK: Persistence

    private fun load() {
        val blob = store.getString(STORAGE_KEY) ?: return
        suggestions = runCatching {
            json.decodeFromString(ListSerializer(ResearchSuggestion.serializer()), blob)
        }.getOrDefault(emptyList())
    }

    private fun save() {
        store.putString(STORAGE_KEY, json.encodeToString(ListSerializer(ResearchSuggestion.serializer()), suggestions))
    }
}
