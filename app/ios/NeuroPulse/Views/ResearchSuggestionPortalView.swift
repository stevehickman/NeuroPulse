// TODO(localization): strings below should use NSLocalizedString — see en.lproj/Localizable.strings
import SwiftUI

// Research suggestion portal — CLAUDE.md §6.3.
// Three functions:
//   (1) Patient research agenda — submit ideas, vote, comment
//   (2) Pre-identified subject pool — "Would participate" intent flag
//   (3) Crowdfunding catalyst — $10–$100+ pledges (intent only; no charge until campaign activates)

struct ResearchSuggestionPortalView: View {

    @StateObject private var store = ResearchSuggestionStore()
    @State private var showSubmitSheet = false
    @State private var selectedSuggestion: ResearchSuggestion?

    var body: some View {
        NavigationStack {
            Group {
                if store.suggestions.isEmpty && !store.isLoading {
                    emptyState
                } else {
                    suggestionList
                }
            }
            .navigationTitle(String(localized: "DASHBOARD_RESEARCH_PORTAL_LINK"))
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button {
                        showSubmitSheet = true
                    } label: {
                        Label("Submit Idea", systemImage: "plus")
                    }
                }
                ToolbarItem(placement: .navigationBarLeading) {
                    if store.isLoading {
                        ProgressView().scaleEffect(0.8)
                    }
                }
            }
            .sheet(isPresented: $showSubmitSheet) {
                SubmitSuggestionSheet(store: store)
            }
            .sheet(item: $selectedSuggestion) { suggestion in
                SuggestionDetailView(suggestion: suggestion, store: store)
            }
            .task { await store.refresh() }
            .refreshable { await store.refresh() }
        }
    }

    private var emptyState: some View {
        VStack(spacing: 20) {
            Image(systemName: "lightbulb")
                .font(.system(size: 48))
                .foregroundColor(.secondary)
            Text(String(localized: "PORTAL_EMPTY_TITLE"))
                .font(.title3.bold())
            Text(String(localized: "PORTAL_EMPTY_BODY"))
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
            Button("Submit an idea") { showSubmitSheet = true }
                .buttonStyle(.borderedProminent)
        }
        .padding(40)
    }

    private var suggestionList: some View {
        List(store.suggestions) { suggestion in
            Button {
                selectedSuggestion = suggestion
            } label: {
                SuggestionRow(suggestion: suggestion, store: store)
            }
            .foregroundColor(.primary)
        }
        .listStyle(.insetGrouped)
    }
}

// MARK: - Suggestion row

struct SuggestionRow: View {
    let suggestion: ResearchSuggestion
    let store: ResearchSuggestionStore

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text(suggestion.title).font(.subheadline.bold())
                        .fixedSize(horizontal: false, vertical: true)
                    Text(suggestion.categories.map(\.rawValue).joined(separator: " · "))
                        .font(.caption).foregroundColor(.secondary)
                }
                Spacer()
                fundingBadge
            }
            Text(suggestion.body)
                .font(.caption)
                .foregroundColor(.secondary)
                .lineLimit(2)
                .fixedSize(horizontal: false, vertical: true)
            rowActions
        }
        .padding(.vertical, 4)
    }

    private var fundingBadge: some View {
        Group {
            switch suggestion.fundingStatus {
            case .open:
                EmptyView()
            case .active:
                Label("Campaign live", systemImage: "dollarsign.circle.fill")
                    .font(.caption2).foregroundColor(.green)
            case .funded:
                Label("Funded", systemImage: "checkmark.seal.fill")
                    .font(.caption2).foregroundColor(.green)
            case .closed:
                Label("Closed", systemImage: "xmark.circle")
                    .font(.caption2).foregroundColor(.secondary)
            }
        }
    }

    private var rowActions: some View {
        HStack(spacing: 16) {
            // Function 1 — vote
            Button {
                store.toggleVote(on: suggestion.id)
            } label: {
                Label("\(suggestion.voteCount)", systemImage: suggestion.didVote ? "hand.thumbsup.fill" : "hand.thumbsup")
                    .font(.caption)
                    .foregroundColor(suggestion.didVote ? .accentColor : .secondary)
            }
            .buttonStyle(.borderless)

            // Function 2 — participation intent
            Button {
                store.toggleParticipationIntent(on: suggestion.id)
            } label: {
                Label(
                    suggestion.hasParticipationIntent ? "Would participate" : "\(suggestion.participationIntentCount) interested",
                    systemImage: suggestion.hasParticipationIntent ? "person.fill.checkmark" : "person"
                )
                .font(.caption)
                .foregroundColor(suggestion.hasParticipationIntent ? .green : .secondary)
            }
            .buttonStyle(.borderless)

            Spacer()

            // Function 3 — pledge indicator
            if let pledge = suggestion.pledgeAmount {
                Label("$\(pledge) pledged", systemImage: "dollarsign.circle")
                    .font(.caption2).foregroundColor(.orange)
            }
        }
    }
}

// MARK: - Suggestion detail

struct SuggestionDetailView: View {
    @State var suggestion: ResearchSuggestion
    let store: ResearchSuggestionStore
    @Environment(\.dismiss) private var dismiss
    @State private var selectedPledgeTier: PledgeTier = .none

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 24) {
                    headerSection
                    statsSection
                    participationSection
                    if suggestion.fundingStatus == .open || suggestion.pledgeAmount != nil {
                        pledgeSection
                    }
                }
                .padding()
            }
            .navigationTitle("Study Idea")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Close") { dismiss() }
                }
            }
        }
    }

    private var headerSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(suggestion.title).font(.title2.bold())
                .fixedSize(horizontal: false, vertical: true)
            Text(suggestion.categories.map(\.rawValue).joined(separator: " · "))
                .font(.caption).foregroundColor(.secondary)
            Text(suggestion.body).font(.body)
                .fixedSize(horizontal: false, vertical: true)
            Text("Suggested \(suggestion.submittedAt.formatted(.dateTime.month().day().year()))")
                .font(.caption2).foregroundColor(.secondary)
        }
    }

    private var statsSection: some View {
        HStack(spacing: 24) {
            stat(icon: "hand.thumbsup.fill", value: "\(suggestion.voteCount)", label: "Votes")
            stat(icon: "person.fill.checkmark", value: "\(suggestion.participationIntentCount)", label: "Interested")
            stat(icon: "dollarsign.circle.fill", value: "$\(suggestion.pledgedAmount)", label: "Pledged")
        }
        .frame(maxWidth: .infinity)
    }

    private func stat(icon: String, value: String, label: String) -> some View {
        VStack(spacing: 4) {
            Image(systemName: icon).foregroundColor(.accentColor)
            Text(value).font(.title3.bold())
            Text(label).font(.caption2).foregroundColor(.secondary)
        }
        .frame(maxWidth: .infinity)
    }

    // Function 1 + 2: vote and participation intent
    private var participationSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(String(localized: "PORTAL_YOUR_INPUT")).font(.headline)

            HStack(spacing: 12) {
                Button {
                    store.toggleVote(on: suggestion.id)
                    suggestion.didVote.toggle()
                    suggestion.voteCount += suggestion.didVote ? 1 : -1
                } label: {
                    Label(
                        suggestion.didVote ? "Voted" : "Vote for this",
                        systemImage: suggestion.didVote ? "hand.thumbsup.fill" : "hand.thumbsup"
                    )
                    .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .tint(suggestion.didVote ? .accentColor : nil)

                Button {
                    store.toggleParticipationIntent(on: suggestion.id)
                    suggestion.hasParticipationIntent.toggle()
                    suggestion.participationIntentCount += suggestion.hasParticipationIntent ? 1 : -1
                } label: {
                    Label(
                        suggestion.hasParticipationIntent ? "Would participate" : "Would participate?",
                        systemImage: suggestion.hasParticipationIntent ? "person.fill.checkmark" : "person.badge.plus"
                    )
                    .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .tint(suggestion.hasParticipationIntent ? .green : nil)
            }

            if suggestion.hasParticipationIntent {
                Text(String(localized: "PORTAL_INTENT_RECORDED"))
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }

    // Function 3: crowdfunding catalyst
    private var pledgeSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(String(localized: "PORTAL_SUPPORT_HEADING")).font(.headline)
            Text("Pledges are intent only — your card is not charged until a researcher"
                + " confirms feasibility and a formal campaign activates."
                + " If the target is not met, you are never charged.")
                .font(.caption)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            Picker("Pledge amount", selection: $selectedPledgeTier) {
                ForEach(PledgeTier.allCases, id: \.self) { tier in
                    Text(tier.label).tag(tier)
                }
            }
            .pickerStyle(.segmented)
            .onAppear {
                if let existing = suggestion.pledgeAmount {
                    selectedPledgeTier = PledgeTier(rawValue: existing) ?? .small
                }
            }

            if selectedPledgeTier != .none {
                Button("Pledge \(selectedPledgeTier.label)") {
                    store.pledge(amount: selectedPledgeTier.rawValue, on: suggestion.id)
                    suggestion.pledgeAmount = selectedPledgeTier.rawValue
                }
                .buttonStyle(.borderedProminent)
                .tint(.orange)
                .frame(maxWidth: .infinity)

                Text("Funders receive a plain-language results summary and are acknowledged"
                    + " in the published paper as 'NeuroPulse Patient Research Fund contributors.'")
                    .font(.caption2)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }
}

// MARK: - Submit sheet

struct SubmitSuggestionSheet: View {
    let store: ResearchSuggestionStore
    @Environment(\.dismiss) private var dismiss
    @State private var draft = ResearchSuggestionDraft()
    @State private var isSubmitting = false

    var body: some View {
        NavigationStack {
            Form {
                Section("Study idea") {
                    TextField("Title (one clear sentence)", text: $draft.title)
                        .fixedSize(horizontal: false, vertical: true)
                    TextEditor(text: $draft.body)
                        .frame(minHeight: 100)
                        .overlay(
                            Group {
                                if draft.body.isEmpty {
                                    Text(String(localized: "PORTAL_DESCRIBE_STUDY"))
                                        .foregroundColor(.secondary)
                                        .padding(4)
                                        .allowsHitTesting(false)
                                        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                                }
                            }
                        )
                }

                Section("Research areas") {
                    ForEach(ResearchCategory.allCases, id: \.self) { category in
                        Toggle(category.rawValue, isOn: Binding(
                            get: { draft.categories.contains(category) },
                            set: { if $0 { draft.categories.insert(category) } else { draft.categories.remove(category) } }
                        ))
                    }
                }

                Section {
                    Toggle("I would participate in this study if it ran", isOn: $draft.hasParticipationIntent)
                    Text("Saying yes signals to researchers that there's a motivated"
                        + " cohort ready to enrol — the hardest problem in trial recruitment.")
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                } header: {
                    Text(String(localized: "PORTAL_PARTICIPATION_INTENT"))
                }

                Section {
                    Picker("Pledge amount", selection: Binding(
                        get: { PledgeTier(rawValue: draft.pledgeAmount) ?? .none },
                        set: { draft.pledgeAmount = $0.rawValue }
                    )) {
                        ForEach(PledgeTier.allCases, id: \.self) { tier in
                            Text(tier.label).tag(tier)
                        }
                    }
                    .pickerStyle(.segmented)
                    if draft.pledgeAmount > 0 {
                        Text(String(localized: "PORTAL_PLEDGE_CAVEAT"))
                            .font(.caption)
                            .foregroundColor(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                } header: {
                    Text(String(localized: "PORTAL_SUPPORT_OPTIONAL"))
                }
            }
            .navigationTitle("Submit Research Idea")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(String(localized: "COMMON_CANCEL")) { dismiss() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    if isSubmitting {
                        ProgressView()
                    } else {
                        Button("Submit") {
                            Task {
                                isSubmitting = true
                                try? await store.submit(draft)
                                isSubmitting = false
                                dismiss()
                            }
                        }
                        .disabled(!draft.isValid)
                    }
                }
            }
        }
    }
}
