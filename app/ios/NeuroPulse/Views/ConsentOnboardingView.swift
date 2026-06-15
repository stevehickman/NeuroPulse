import SwiftUI

// 4-screen research consent onboarding — CLAUDE.md §6.2.
// L1: Contact consent · L2: Category consent · L3: Blanket consent + irreversibility notice · L4: Results + community.
// Displayed once at first launch; accessible any time from Consent tab.

struct ConsentOnboardingView: View {

    @EnvironmentObject private var consentStore: ConsentStore
    @Binding var isPresented: Bool
    @State private var layer: ConsentLayer = .l1Contact
    @State private var draft = ResearchConsentState()

    enum ConsentLayer: Int, CaseIterable {
        case l1Contact   = 0
        case l2Category  = 1
        case l3Blanket   = 2
        case l4Results   = 3
        case complete    = 4
    }

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                layerProgressDots
                    .padding(.top, 16)
                Divider().padding(.top, 12)
                layerContent
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                Divider()
                navigationButtons
                    .padding()
            }
            .navigationTitle(layerTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    // Skip defers all data decisions — analytics gate stays closed.
                    Button(String(localized: "CONSENT_SKIP_BUTTON")) { commitAndDismiss(grantAnalyticsConsent: false) }
                        .foregroundColor(.secondary)
                }
            }
        }
    }

    // MARK: - Progress dots

    private var layerProgressDots: some View {
        HStack(spacing: 8) {
            ForEach(0..<4, id: \.self) { idx in
                Circle()
                    .fill(idx <= layer.rawValue ? Color.accentColor : Color(.systemGray4))
                    .frame(width: 8, height: 8)
                    .animation(.easeInOut, value: layer)
            }
        }
    }

    private var layerTitle: String {
        switch layer {
        case .l1Contact:  return String(localized: "CONSENT_L1_TITLE")
        case .l2Category: return String(localized: "CONSENT_L2_TITLE")
        case .l3Blanket:  return String(localized: "CONSENT_L3_TITLE")
        case .l4Results:  return String(localized: "CONSENT_L4_TITLE")
        case .complete:   return String(localized: "CONSENT_COMPLETE_TITLE")
        }
    }

    // MARK: - Layer content

    @ViewBuilder
    private var layerContent: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                switch layer {
                case .l1Contact:  l1ContactLayer
                case .l2Category: l2CategoryLayer
                case .l3Blanket:  l3BlanketLayer
                case .l4Results:  l4ResultsLayer
                case .complete:   completeLayer
                }
            }
            .padding()
        }
    }

    // L1 — Contact consent
    private var l1ContactLayer: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("CONSENT_L1_HEADING")
                .font(.headline)
                .fixedSize(horizontal: false, vertical: true)

            Text("CONSENT_L1_BODY")
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
                    Picker("Contact frequency", selection: $draft.contactFrequency) {
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

            noContactNote
        }
    }

    private var noContactNote: some View {
        Text("CONSENT_L1_NO_CONTACT_NOTE")
            .font(.caption)
            .foregroundColor(.secondary)
            .italic()
            .fixedSize(horizontal: false, vertical: true)
    }

    // L2 — Category consent
    private var l2CategoryLayer: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("CONSENT_L2_HEADING")
                .font(.headline)
                .fixedSize(horizontal: false, vertical: true)

            Text("CONSENT_L2_BODY")
                .font(.body)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            ForEach(ResearchCategory.allCases, id: \.self) { category in
                Toggle(category.rawValue, isOn: Binding(
                    get: { draft.categoryConsents[category] ?? false },
                    set: { draft.categoryConsents[category] = $0 }
                ))
                .toggleStyle(.switch)
            }
        }
    }

    // L3 — Blanket consent + irreversibility notice
    private var l3BlanketLayer: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("CONSENT_L3_HEADING")
                .font(.headline)
                .fixedSize(horizontal: false, vertical: true)

            Text("CONSENT_L3_BODY")
                .font(.body)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            Toggle("CONSENT_L3_TOGGLE", isOn: $draft.blanketConsentGranted)
                .toggleStyle(.switch)

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

    // L4 — Results + community
    private var l4ResultsLayer: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("CONSENT_L4_HEADING")
                .font(.headline)
                .fixedSize(horizontal: false, vertical: true)

            VStack(alignment: .leading, spacing: 12) {
                Toggle("CONSENT_L4_RESULTS_TOGGLE", isOn: $draft.resultsOptIn)
                    .toggleStyle(.switch)

                Text("CONSENT_L4_RESULTS_CAPTION")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.leading, 44)

                Toggle("CONSENT_L4_PORTAL_TOGGLE", isOn: $draft.suggestionPortalOptIn)
                    .toggleStyle(.switch)

                Text("CONSENT_L4_PORTAL_CAPTION")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.leading, 44)
            }

            Text("CONSENT_L4_OPTIONAL_NOTE")
                .font(.caption)
                .foregroundColor(.secondary)
                .italic()
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private var completeLayer: some View {
        VStack(spacing: 20) {
            Image(systemName: "checkmark.seal.fill")
                .font(.system(size: 60))
                .foregroundColor(.green)
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
            if layer != .l1Contact && layer != .complete {
                Button("CONSENT_BACK_BUTTON") { withAnimation { goBack() } }
                    .buttonStyle(.bordered)
            }
            Spacer()
            if layer == .complete {
                Button("CONSENT_DONE_BUTTON") { commitAndDismiss() }
                    .buttonStyle(.borderedProminent)
            } else {
                Button("CONSENT_CONTINUE_BUTTON") { withAnimation { advance() } }
                    .buttonStyle(.borderedProminent)
            }
        }
    }

    private func advance() {
        if let next = ConsentLayer(rawValue: layer.rawValue + 1) {
            layer = next
            if layer == .complete { commitAndDismiss() }
        }
    }

    private func goBack() {
        if let prev = ConsentLayer(rawValue: layer.rawValue - 1) { layer = prev }
    }

    /// Persist the draft research-consent state and dismiss the sheet.
    ///
    /// - Parameter grantAnalyticsConsent: Pass `true` when the user actively
    ///   completed the flow (Done). Pass `false` when the user pressed Skip —
    ///   skipping defers all data decisions; the analytics gate stays closed.
    ///
    /// When `grantAnalyticsConsent` is true this function also calls
    /// `AnalyticsGate.configure()` directly so that re-consent from
    /// `ConsentDashboardView` restarts the SDK without requiring the app to
    /// re-run `presentNextOnboardingStep()`.
    private func commitAndDismiss(grantAnalyticsConsent: Bool = true) {
        consentStore.updateResearchConsent(draft)
        if grantAnalyticsConsent {
            UserDefaults.standard.set(true, forKey: AnalyticsGate.analyticsConsentKey)
            AnalyticsGate.configure()
        }
        isPresented = false
    }
}
