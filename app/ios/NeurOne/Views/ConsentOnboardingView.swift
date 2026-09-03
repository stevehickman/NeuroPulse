import SwiftUI

// Two-screen research consent onboarding — CLAUDE.md §6.2.
//
// LAYERS ARE NOT SCREENS. The four consent layers are unchanged; they are presented
// across two screens (CLAUDE.md Rev 37):
//   S1 "What you get back" — L4 (results + portal) then L1 (contact consent)
//   S2 "What you share"    — L2 (categories) and L3 (blanket posture), two controls
//
// S1 comes first because L1 and L4 were always the same question: L1 asks whether we may
// contact you, L4 asks what about. A contact method is the shared precondition for study
// invitations, per-study engagement notifications, and results notifications alike (§6.2.1).
//
// S2 carries two controls rather than one because L2 and L3 are orthogonal axes — L2 is
// scope, L3 is posture. "Everything, but ask me" is a real position and survives only while
// both axes do (§6.2.2). Select-all sets the nine categories and deliberately does NOT touch
// the blanket toggle (§6.2.3).
//
// Displayed once at first launch; reachable any time from the Consent tab as Research
// Preferences. Because it is re-entrant, it is also the surface on which blanket consent is
// withdrawn — see commitAndDismiss.

struct ConsentOnboardingView: View {

    @EnvironmentObject private var consentStore: ConsentStore
    @Binding var isPresented: Bool
    @State private var screen: ConsentScreen = .s1Reciprocity
    @State private var draft = ResearchConsentState()

    /// Onboarding screens. Distinct from the four consent *layers* (L1–L4), which are what
    /// `ResearchConsentState`, the withdrawal surfaces and the document set key on.
    enum ConsentScreen: Int, CaseIterable {
        case s1Reciprocity = 0   // L4 + L1
        case s2Sharing     = 1   // L2 + L3
        case complete      = 2

        /// Screens the progress indicator counts — `complete` is a confirmation, not a step.
        static var stepCount: Int { 2 }
    }

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                screenProgressDots
                    .padding(.top, 16)
                Divider().padding(.top, 12)
                screenContent
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                Divider()
                navigationButtons
                    .padding()
            }
            .navigationTitle(screenTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    // Skip defers all data decisions — research analytics gate stays closed.
                    Button(String(localized: "CONSENT_SKIP_BUTTON")) { commitAndDismiss(grantResearchAnalytics: false) }
                        .foregroundColor(.secondary)
                }
            }
            // Seed the draft from committed state. The view is re-entrant as Research
            // Preferences, so starting from a blank state would silently clear every prior
            // consent decision the moment the user pressed Done.
            .onAppear { draft = consentStore.researchConsent }
        }
    }

    // MARK: - Progress dots

    private var screenProgressDots: some View {
        HStack(spacing: 8) {
            ForEach(0..<ConsentScreen.stepCount, id: \.self) { idx in
                Circle()
                    .fill(idx <= screen.rawValue ? Color.accentColor : Color(.systemGray4))
                    .frame(width: 8, height: 8)
                    .animation(.easeInOut, value: screen)
            }
        }
        .accessibilityHidden(true)
    }

    private var screenTitle: String {
        switch screen {
        case .s1Reciprocity: return String(localized: "CONSENT_S1_TITLE")
        case .s2Sharing:     return String(localized: "CONSENT_S2_TITLE")
        case .complete:      return String(localized: "CONSENT_COMPLETE_TITLE")
        }
    }

    // MARK: - Screen content

    @ViewBuilder
    private var screenContent: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                switch screen {
                case .s1Reciprocity: s1ReciprocityScreen
                case .s2Sharing:     s2SharingScreen
                case .complete:      completeScreen
                }
            }
            .padding()
        }
    }

    // MARK: - S1 — What you get back (L4 then L1)

    private var s1ReciprocityScreen: some View {
        VStack(alignment: .leading, spacing: 24) {
            // L4 — results + community. Presented first, under the §6.2.4 copy rules:
            // conditional framing ("if your data ever contributes"), the exchange stated as
            // symmetric, and non-coercion stated on the screen.
            VStack(alignment: .leading, spacing: 16) {
                Text("CONSENT_S1_HEADING")
                    .font(.headline)
                    .fixedSize(horizontal: false, vertical: true)

                Text("CONSENT_S1_BODY")
                    .font(.body)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)

                Toggle("CONSENT_L4_RESULTS_TOGGLE", isOn: $draft.resultsOptIn)
                    .toggleStyle(.switch)

                Text("CONSENT_L4_RESULTS_CAPTION")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)

                Toggle("CONSENT_L4_PORTAL_TOGGLE", isOn: $draft.suggestionPortalOptIn)
                    .toggleStyle(.switch)

                Text("CONSENT_L4_PORTAL_CAPTION")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)

                Text("CONSENT_S1_NONCOERCION")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .italic()
                    .fixedSize(horizontal: false, vertical: true)
            }

            Divider()

            // L1 — contact consent. Same screen because it is the same question: L4 asks what
            // you want to hear about, L1 asks how we reach you (§6.2.1).
            VStack(alignment: .leading, spacing: 16) {
                Text("CONSENT_S1_CONTACT_HEADING")
                    .font(.headline)
                    .fixedSize(horizontal: false, vertical: true)

                Text("CONSENT_S1_CONTACT_BODY")
                    .font(.body)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)

                Toggle("CONSENT_L1_TOGGLE", isOn: $draft.contactConsentGranted)
                    .toggleStyle(.switch)

                if draft.contactConsentGranted {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("CONSENT_L1_CONTACT_LABEL").font(.caption).foregroundColor(.secondary)
                        TextField("CONSENT_L1_CONTACT_PLACEHOLDER", text: $draft.contactMethod)
                            .textContentType(.emailAddress)
                            .keyboardType(.emailAddress)
                            .textFieldStyle(.roundedBorder)
                        Picker("CONSENT_CONTACT_FREQUENCY", selection: $draft.contactFrequency) {
                            ForEach(ContactFrequency.allCases, id: \.self) { freq in
                                Text(freq.rawValue).tag(freq)
                            }
                        }
                        .pickerStyle(.segmented)
                    }

                    Toggle("CONSENT_L1_POA_TOGGLE", isOn: $draft.isPOAHolder)
                        .font(.subheadline)

                    if draft.isPOAHolder {
                        Text("CONSENT_L1_POA_NOTE")
                            .font(.caption)
                            .foregroundColor(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                            .padding(10)
                            .background(Color(.systemGray6))
                            .clipShape(RoundedRectangle(cornerRadius: 8))
                    }
                }

                Text("CONSENT_L1_NO_CONTACT_NOTE")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .italic()
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }

    // MARK: - S2 — What you share (L2 + L3, two controls)

    private var s2SharingScreen: some View {
        VStack(alignment: .leading, spacing: 24) {
            // L2 — category scope.
            VStack(alignment: .leading, spacing: 16) {
                Text("CONSENT_L2_HEADING")
                    .font(.headline)
                    .fixedSize(horizontal: false, vertical: true)

                Text("CONSENT_L2_BODY")
                    .font(.body)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)

                // Select-all sets all nine categories. It deliberately does NOT enable the
                // blanket toggle below: ticking nine boxes expresses breadth of interest, not a
                // wish to stop being consulted (§6.2.3). The usability gap that creates is
                // closed with copy — the note below — rather than by coupling the state.
                Toggle("CONSENT_S2_SELECT_ALL_TOGGLE", isOn: Binding(
                    get: { draft.allCategoriesSelected },
                    set: { draft.setAllCategories($0) }
                ))
                .toggleStyle(.switch)
                .font(.subheadline.bold())

                if draft.allCategoriesSelected && !draft.blanketConsentGranted {
                    Text("CONSENT_S2_SELECT_ALL_NOTE")
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                        .padding(10)
                        .background(Color(.systemGray6))
                        .clipShape(RoundedRectangle(cornerRadius: 8))
                }

                ForEach(ResearchCategory.allCases, id: \.self) { category in
                    Toggle(category.rawValue, isOn: Binding(
                        get: { draft.categoryConsents[category] ?? false },
                        set: { draft.categoryConsents[category] = $0 }
                    ))
                    .toggleStyle(.switch)
                }
            }

            Divider()

            // L3 — posture. A separate control, separately labelled in plain words rather than
            // by naming a tier (§6.2.3).
            VStack(alignment: .leading, spacing: 16) {
                Text("CONSENT_S2_BLANKET_HEADING")
                    .font(.headline)
                    .fixedSize(horizontal: false, vertical: true)

                Text("CONSENT_S2_BLANKET_BODY")
                    .font(.body)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)

                Toggle("CONSENT_S2_BLANKET_TOGGLE", isOn: $draft.blanketConsentGranted)
                    .toggleStyle(.switch)

                Text("CONSENT_S2_BLANKET_CAPTION")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)

                // Irreversibility notice — shown whenever the blanket control is on, on whichever
                // screen renders it (CLAUDE.md §6.2, L3 row).
                if draft.blanketConsentGranted {
                    VStack(alignment: .leading, spacing: 8) {
                        Label("CONSENT_IMPORTANT_LABEL", systemImage: "info.circle.fill")
                            .font(.subheadline.bold())
                            .foregroundColor(.orange)
                            .fixedSize(horizontal: false, vertical: true)
                        Text(ConsentEngine.irreversibilityNotice)
                            .font(.caption)
                            .foregroundColor(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    .padding(12)
                    .background(Color.orange.opacity(0.08))
                    .clipShape(RoundedRectangle(cornerRadius: 10))
                }
            }
        }
    }

    private var completeScreen: some View {
        VStack(spacing: 20) {
            Image(systemName: "checkmark.seal.fill")
                .font(.system(size: 60))
                .foregroundColor(.green)
                .accessibilityHidden(true)
            Text("CONSENT_COMPLETE_HEADING")
                .font(.title2.bold())
            Text("CONSENT_COMPLETE_BODY")
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
    }

    // MARK: - Navigation

    private var navigationButtons: some View {
        HStack(spacing: 12) {
            if screen != .s1Reciprocity && screen != .complete {
                Button("CONSENT_BACK_BUTTON") { withAnimation { goBack() } }
                    .buttonStyle(.bordered)
            }
            Spacer()
            if screen == .complete {
                Button("CONSENT_DONE_BUTTON") { commitAndDismiss() }
                    .buttonStyle(.borderedProminent)
            } else {
                Button("CONSENT_CONTINUE_BUTTON") { withAnimation { advance() } }
                    .buttonStyle(.borderedProminent)
            }
        }
    }

    private func advance() {
        if let next = ConsentScreen(rawValue: screen.rawValue + 1) {
            screen = next
        }
    }

    private func goBack() {
        if let prev = ConsentScreen(rawValue: screen.rawValue - 1) { screen = prev }
    }

    /// Persist the draft research-consent state and dismiss the sheet.
    ///
    /// - Parameter grantResearchAnalytics: Pass `true` when the user actively
    ///   completed the flow (Done). Pass `false` when the user pressed Skip —
    ///   skipping defers all data decisions; the research analytics gate stays closed.
    ///
    /// This view is re-entrant (Consent tab → Research Preferences), so it is also the
    /// surface on which a user turns blanket consent back off. `updateResearchConsent`
    /// detects that true→false transition and performs the analytics teardown (§6.2.5) —
    /// the view deliberately does not special-case it, because a view-level check would be
    /// one more place for the coupling to be forgotten. That is exactly how it was lost here
    /// before Rev 37.
    ///
    /// When `grantResearchAnalytics` is true this function also calls
    /// `ResearchAnalyticsGate.configure()` directly so that re-consent from
    /// `ConsentDashboardView` restarts the SDK without requiring the app to
    /// re-run `presentNextOnboardingStep()`.
    private func commitAndDismiss(grantResearchAnalytics: Bool = true) {
        // Read the transition BEFORE committing — afterwards the store's previous value is gone.
        let isBlanketWithdrawal =
            consentStore.researchConsent.blanketConsentGranted && !draft.blanketConsentGranted

        consentStore.updateResearchConsent(draft)

        // Re-opening the gate straight after a blanket withdrawal would undo the teardown the
        // commit just performed, so Done does not re-grant in that case.
        if grantResearchAnalytics && !isBlanketWithdrawal {
            UserDefaults.standard.set(true, forKey: ResearchAnalyticsGate.researchAnalyticsKey)
            ResearchAnalyticsGate.configure()
        }
        isPresented = false
    }
}
