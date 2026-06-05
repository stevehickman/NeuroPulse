// TODO(localisation): strings below should use NSLocalizedString — see en.lproj/Localizable.strings
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
                    Button("Skip") { commitAndDismiss() }
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
        case .l1Contact:  return "Research Contact"
        case .l2Category: return "Research Areas"
        case .l3Blanket:  return "Data Consent"
        case .l4Results:  return "Stay Informed"
        case .complete:   return "All Done"
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
            Text("Can we contact you about future research opportunities?")
                .font(.headline)
                .fixedSize(horizontal: false, vertical: true)

            Text("NeuroPulse works with academic research institutions to study the technologies in your device. Your participation is always voluntary. If you say yes, we may reach out when a study matches your interests.")
                .font(.body)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            Toggle("Yes, you can contact me", isOn: $draft.contactConsentGranted)
                .toggleStyle(.switch)

            if draft.contactConsentGranted {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Contact email or phone:").font(.caption).foregroundColor(.secondary)
                    TextField("your@email.com", text: $draft.contactMethod)
                        .textContentType(.emailAddress)
                        .keyboardType(.emailAddress)
                        .textFieldStyle(.roundedBorder)
                }

                Toggle("I am a Power of Attorney holder acting for a patient", isOn: $draft.isPOAHolder)
                    .font(.subheadline)

                if draft.isPOAHolder {
                    Text("You will be able to upload your executed healthcare POA documentation from the Privacy tab. Our team reviews documents within 3 business days.")
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
        Text("If you say no — or skip this entirely — all features work identically. We will never contact you about research.")
            .font(.caption)
            .foregroundColor(.secondary)
            .italic()
            .fixedSize(horizontal: false, vertical: true)
    }

    // L2 — Category consent
    private var l2CategoryLayer: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Which research areas interest you?")
                .font(.headline)
                .fixedSize(horizontal: false, vertical: true)

            Text("We will only contact you about studies in the areas you choose. Each project is a separate decision — saying yes here does not automatically enrol you in anything.")
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
            Text("Pre-approve all reviewed research?")
                .font(.headline)
                .fixedSize(horizontal: false, vertical: true)

            Text("If you say yes, your anonymised session data will be included in all NeuroPulse-reviewed research studies automatically. You will still receive per-study engagement notifications and can opt out of any individual study.")
                .font(.body)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            Toggle("Pre-approve all reviewed research", isOn: $draft.blanketConsentGranted)
                .toggleStyle(.switch)

            VStack(alignment: .leading, spacing: 8) {
                Label("Important — please read", systemImage: "info.circle.fill")
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
            Text("Stay connected to the science")
                .font(.headline)
                .fixedSize(horizontal: false, vertical: true)

            VStack(alignment: .leading, spacing: 12) {
                Toggle("Receive study results when they're published", isOn: $draft.resultsOptIn)
                    .toggleStyle(.switch)

                Text("Plain-language summaries, including null results. Never marketing.")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.leading, 44)

                Toggle("Join the research suggestion portal", isOn: $draft.suggestionPortalOptIn)
                    .toggleStyle(.switch)

                Text("Submit study ideas, vote on priorities, express interest in participating. Your suggestions are seen by the research community.")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(.leading, 44)
            }

            Text("Both options are entirely optional and can be changed at any time.")
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
            Text("Your preferences are saved.")
                .font(.title2.bold())
            Text("You can review and update your consent decisions at any time from the Privacy tab.")
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
                Button("Back") { withAnimation { goBack() } }
                    .buttonStyle(.bordered)
            }
            Spacer()
            if layer == .complete {
                Button("Done") { commitAndDismiss() }
                    .buttonStyle(.borderedProminent)
            } else {
                Button("Continue") { withAnimation { advance() } }
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

    private func commitAndDismiss() {
        consentStore.updateResearchConsent(draft)
        // Mark consent as actively completed — this is the stronger semantic
        // used by AnalyticsGate.isOpen. Keying on deliberate user action (Done,
        // Skip, or layer completion) rather than screen presentation ensures the
        // analytics SDK never opens without an explicit user decision (ISC-92).
        UserDefaults.standard.set(true, forKey: AnalyticsGate.consentAcceptedKey)
        isPresented = false
    }
}
