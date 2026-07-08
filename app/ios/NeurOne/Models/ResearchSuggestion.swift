import Foundation

// Research suggestion portal model — CLAUDE.md §6.3.
// Three functions: (1) patient research agenda, (2) pre-identified subject pool, (3) crowdfunding catalyst.
// Server calls are stubs; backend infrastructure is out of scope for T1 (see ISA scope exclusion §2.3).

// MARK: - Suggestion

struct ResearchSuggestion: Codable, Identifiable {
    var id: UUID
    var title: String
    var body: String
    var categories: [ResearchCategory]
    var submittedAt: Date
    var authorDeviceToken: String    // opaque per §5.3 — no user identity

    // Server-side aggregates (refreshed from API; optimistic local updates on vote/pledge)
    var voteCount: Int
    var participationIntentCount: Int
    var pledgedAmount: Int           // cumulative dollars pledged across all users

    // Per-user state (local — never transmitted as individual fields)
    var didVote: Bool
    var hasParticipationIntent: Bool
    var pledgeAmount: Int?           // nil = no pledge

    var fundingStatus: FundingStatus

    enum FundingStatus: String, Codable {
        case open       // collecting intent + pledges
        case active     // formal campaign live — escrow charged if target met
        case funded     // target met
        case closed     // campaign ended — refunded if target not met
    }
}

// MARK: - Submitted idea draft (local, pre-server)

struct ResearchSuggestionDraft {
    var title: String = ""
    var body: String = ""
    var categories: Set<ResearchCategory> = []
    var hasParticipationIntent: Bool = false
    var pledgeAmount: Int = 0        // 0 = no pledge

    var isValid: Bool {
        !title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
        !body.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
        !categories.isEmpty
    }
}

// MARK: - Pledge tier helpers

enum PledgeTier: Int, CaseIterable {
    case none = 0
    case small = 10
    case medium = 25
    case large = 50
    case max = 100

    var label: String {
        switch self {
        case .none:   return "No pledge"
        case .small:  return "$10"
        case .medium: return "$25"
        case .large:  return "$50"
        case .max:    return "$100+"
        }
    }
}
