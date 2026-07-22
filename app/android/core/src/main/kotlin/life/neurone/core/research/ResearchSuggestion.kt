package life.neurone.core.research

import life.neurone.core.models.ResearchCategory
import kotlinx.serialization.Serializable
import java.util.UUID

// Port of iOS ResearchSuggestion.swift (CLAUDE.md §6.3). Three functions: (1) patient
// research agenda, (2) pre-identified subject pool, (3) crowdfunding catalyst. Server sync is
// a stub for T1 — this is local state only. `authorDeviceToken` is an opaque per-device token
// (no user identity). Exact timestamps are avoided: day granularity only, consistent with the
// privacy posture elsewhere.

@Serializable
enum class FundingStatus { OPEN, ACTIVE, FUNDED, CLOSED }

@Serializable
data class ResearchSuggestion(
    val id: String = UUID.randomUUID().toString(),
    val title: String,
    val body: String,
    val categories: List<ResearchCategory>,
    val submittedAtDay: String,        // "yyyy-MM-dd"
    val authorDeviceToken: String,     // opaque — no user identity
    // Server-side aggregates (optimistic local updates on vote/pledge).
    val voteCount: Int = 0,
    val participationIntentCount: Int = 0,
    val pledgedAmount: Int = 0,        // cumulative dollars pledged across all users
    // Per-user state (local — never transmitted as individual fields).
    val didVote: Boolean = false,
    val hasParticipationIntent: Boolean = false,
    val pledgeAmount: Int? = null,     // null = no pledge
    val fundingStatus: FundingStatus = FundingStatus.OPEN,
)

// Local, pre-server draft.
data class ResearchSuggestionDraft(
    val title: String = "",
    val body: String = "",
    val categories: Set<ResearchCategory> = emptySet(),
    val hasParticipationIntent: Boolean = false,
    val pledgeAmount: Int = 0,         // 0 = no pledge
) {
    val isValid: Boolean
        get() = title.isNotBlank() && body.isNotBlank() && categories.isNotEmpty()
}

enum class PledgeTier(val amount: Int, val label: String) {
    NONE(0, "No pledge"),
    SMALL(10, "\$10"),
    MEDIUM(25, "\$25"),
    LARGE(50, "\$50"),
    MAX(100, "\$100+"),
}
