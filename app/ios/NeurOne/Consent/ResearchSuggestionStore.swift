import Foundation
import Combine
import Security

// Research suggestion portal store — CLAUDE.md §6.3.
// Manages local suggestion state and stubs for server sync.
// Backend API is out of scope for T1; stubs are marked STUB for future implementation.

@MainActor
final class ResearchSuggestionStore: ObservableObject {

    @Published private(set) var suggestions: [ResearchSuggestion] = []
    @Published private(set) var isLoading = false
    @Published private(set) var lastError: String?

    private let localKey = "np.research.suggestions"

    init() { loadLocal() }

    // MARK: - Fetch (STUB — replace with real API call)

    func refresh() async {
        isLoading = true
        do {
            // STUB: GET /api/v1/research/suggestions
            // On success: suggestions = decoded; save to localKey
            // For now, surface whatever is locally cached.
        }
        isLoading = false
    }

    // MARK: - Submit new suggestion (function 1 — patient research agenda)

    func submit(_ draft: ResearchSuggestionDraft) async throws {
        guard draft.isValid else { return }
        // STUB: POST /api/v1/research/suggestions with title, body, categories, participationIntent
        // On success: refresh()
        // Optimistic local insert for T1:
        let suggestion = ResearchSuggestion(
            id: UUID(),
            title: draft.title,
            body: draft.body,
            categories: Array(draft.categories),
            submittedAt: Date(),
            authorDeviceToken: deviceToken(),
            voteCount: 0,
            participationIntentCount: draft.hasParticipationIntent ? 1 : 0,
            pledgedAmount: draft.pledgeAmount,
            didVote: false,
            hasParticipationIntent: draft.hasParticipationIntent,
            pledgeAmount: draft.pledgeAmount > 0 ? draft.pledgeAmount : nil,
            fundingStatus: .open
        )
        suggestions.insert(suggestion, at: 0)
        saveLocal()
    }

    // MARK: - Vote (function 1 — community votes)

    func toggleVote(on suggestionID: UUID) {
        guard let idx = suggestions.firstIndex(where: { $0.id == suggestionID }) else { return }
        let wasVoting = suggestions[idx].didVote
        suggestions[idx].didVote.toggle()
        suggestions[idx].voteCount += wasVoting ? -1 : 1
        saveLocal()
        // STUB: POST /api/v1/research/suggestions/{id}/vote or DELETE
    }

    // MARK: - Participation intent (function 2 — pre-identified subject pool)

    func toggleParticipationIntent(on suggestionID: UUID) {
        guard let idx = suggestions.firstIndex(where: { $0.id == suggestionID }) else { return }
        let wasIntending = suggestions[idx].hasParticipationIntent
        suggestions[idx].hasParticipationIntent.toggle()
        suggestions[idx].participationIntentCount += wasIntending ? -1 : 1
        saveLocal()
        // STUB: POST /api/v1/research/suggestions/{id}/intent or DELETE
    }

    // MARK: - Pledge (function 3 — crowdfunding catalyst)

    func pledge(amount: Int, on suggestionID: UUID) {
        guard let idx = suggestions.firstIndex(where: { $0.id == suggestionID }) else { return }
        let previous = suggestions[idx].pledgeAmount ?? 0
        suggestions[idx].pledgeAmount = amount
        suggestions[idx].pledgedAmount = suggestions[idx].pledgedAmount - previous + amount
        saveLocal()
        // STUB: POST /api/v1/research/suggestions/{id}/pledge { amount }
        // Note: pledge is intent only — no charge until campaign activates and target met.
    }

    // MARK: - Persistence

    private func loadLocal() {
        guard let data = UserDefaults.standard.data(forKey: localKey),
              let decoded = try? JSONDecoder().decode([ResearchSuggestion].self, from: data)
        else { return }
        suggestions = decoded
    }

    private func saveLocal() {
        if let data = try? JSONEncoder().encode(suggestions) {
            UserDefaults.standard.set(data, forKey: localKey)
        }
    }

    // Opaque token stored in the device Keychain — cannot be synced to iCloud or restored to a
    // new device, so it cannot collide across re-installs or device transfers.
    // kSecAttrAccessibleWhenUnlockedThisDeviceOnly + kSecAttrSynchronizable: false enforces this.
    private func deviceToken() -> String {
        let service = "np.research.device-token"
        let readQuery: [CFString: Any] = [
            kSecClass:          kSecClassGenericPassword,
            kSecAttrService:    service,
            kSecAttrAccessible: kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
            kSecReturnData:     true,
            kSecMatchLimit:     kSecMatchLimitOne
        ]
        var result: CFTypeRef?
        if SecItemCopyMatching(readQuery as CFDictionary, &result) == errSecSuccess,
           let data = result as? Data,
           let token = String(data: data, encoding: .utf8) {
            return token
        }
        let token = UUID().uuidString
        let addAttrs: [CFString: Any] = [
            kSecClass:              kSecClassGenericPassword,
            kSecAttrService:       service,
            kSecAttrAccessible:    kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
            kSecAttrSynchronizable: false,
            kSecValueData:         Data(token.utf8)
        ]
        SecItemAdd(addAttrs as CFDictionary, nil)
        return token
    }
}
