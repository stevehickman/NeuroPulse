import SwiftUI

// BIPA biometric data written-release — 4-step consent flow.
// Replaces the former single-screen disclosure with one screen per topic so users
// understand each dimension of what they are consenting to before they authorise.
//
// Each step covers one concept:
//   Step 1 — What EEG data is and why it is classified as biometric
//   Step 2 — Where it is stored and how it is encrypted
//   Step 3 — What it is used for and hard limits on use
//   Step 4 — The formal written release (checkbox + explicit action required)
//
// Shown to ALL users regardless of locale (OI-PA-03 resolved). Satisfies:
//   • BIPA 740 ILCS 14/15(b) — written release executed by the subject
//   • GDPR Art. 9 — explicit consent for special-category biometric data
//   • WA MHMD RCW 70.372 — authorization for consumer health data
//
// API: onAccept / onDecline. Both callers (NeuroPulseApp, SetupView) are unchanged.
//
// Satisfies: ISC-88 (explicit accept/decline, no skip), ISC-89 (presentation context
// handled by caller), ISC-91 (re-presentable from Settings), ISC-160 (no truncation).

struct BIPADisclosureView: View {

    let onAccept: () -> Void
    let onDecline: () -> Void

    private enum BIPAStep: Int, Hashable {
        case storage  // Step 2
        case purpose  // Step 3
        case release  // Step 4 — written release
    }

    @State private var path: [BIPAStep] = []

    var body: some View {
        NavigationStack(path: $path) {
            step1View
                .navigationDestination(for: BIPAStep.self) { step in
                    switch step {
                    case .storage: step2View
                    case .purpose: step3View
                    case .release: BIPAReleaseStep(onAccept: onAccept, onDecline: onDecline)
                    }
                }
        }
        // No interactive dismiss — user must accept or decline (ISC-88).
        .interactiveDismissDisabled(true)
    }

    // MARK: - Step 1 — What is biometric data?

    private var step1View: some View {
        BIPAStepShell(
            current: 1,
            symbol: "brain.head.profile",
            title: String(localized: "BIPA_STEP1_TITLE"),
            navTitle: String(localized: "BIPA_STEP1_NAV_TITLE"),
            hideBackButton: true
        ) {
            Text(String(localized: "BIPA_STEP1_BODY_1"))
                .font(.body)
                .fixedSize(horizontal: false, vertical: true)

            BIPAInfoCard {
                Text(String(localized: "BIPA_STEP1_CARD"))
                    .font(.body)
                    .fixedSize(horizontal: false, vertical: true)
            }

            Text(String(localized: "BIPA_STEP1_BODY_2"))
                .font(.body)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        } footer: {
            BIPAContinueButton { path.append(.storage) }
        }
    }

    // MARK: - Step 2 — Stored on your device only

    private var step2View: some View {
        BIPAStepShell(
            current: 2,
            symbol: "lock.iphone",
            title: String(localized: "BIPA_STEP2_TITLE"),
            navTitle: String(localized: "BIPA_STEP2_NAV_TITLE")
        ) {
            Text(String(localized: "BIPA_STEP2_BODY"))
                .font(.body)
                .fixedSize(horizontal: false, vertical: true)

            BIPACheckmarkCard(items: [
                String(localized: "BIPA_STEP2_BULLET_1"),
                String(localized: "BIPA_STEP2_BULLET_2"),
                String(localized: "BIPA_STEP2_BULLET_3"),
            ])
        } footer: {
            BIPAContinueButton { path.append(.purpose) }
        }
    }

    // MARK: - Step 3 — Purpose, retention and limits

    private var step3View: some View {
        BIPAStepShell(
            current: 3,
            symbol: "waveform.path.ecg",
            title: String(localized: "BIPA_STEP3_TITLE"),
            navTitle: String(localized: "BIPA_STEP3_NAV_TITLE")
        ) {
            Text(String(localized: "BIPA_STEP3_BODY"))
                .font(.body)
                .fixedSize(horizontal: false, vertical: true)

            BIPABulletSection(
                heading: String(localized: "BIPA_STEP3_PURPOSE_HEADING"),
                items: [
                    String(localized: "BIPA_STEP3_PURPOSE_1"),
                    String(localized: "BIPA_STEP3_PURPOSE_2"),
                    String(localized: "BIPA_STEP3_PURPOSE_3"),
                ]
            )

            BIPABulletSection(
                heading: String(localized: "BIPA_STEP3_LIMITS_HEADING"),
                items: [
                    String(localized: "BIPA_STEP3_LIMIT_1"),
                    String(localized: "BIPA_STEP3_LIMIT_2"),
                ]
            )
        } footer: {
            BIPAContinueButton { path.append(.release) }
        }
    }
}

// MARK: - Step 4 — Written release (separate struct — needs @State)

private struct BIPAReleaseStep: View {

    let onAccept: () -> Void
    let onDecline: () -> Void

    @State private var authorized = false

    var body: some View {
        VStack(spacing: 0) {
            BIPAProgressPills(current: 4)
                .padding(.vertical, 12)
            Divider()

            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    // Header
                    VStack(alignment: .leading, spacing: 12) {
                        Image(systemName: "checkmark.seal")
                            .font(.system(size: 48))
                            .foregroundColor(.accentColor)
                            .accessibilityHidden(true)
                        Text(String(localized: "BIPA_STEP4_TITLE"))
                            .font(.largeTitle.bold())
                            .fixedSize(horizontal: false, vertical: true)
                    }

                    Text(String(localized: "BIPA_STEP4_INTRO"))
                        .font(.body)
                        .fixedSize(horizontal: false, vertical: true)

                    // Formal release text — must render in full (ISC-160).
                    Text(String(localized: "BIPA_STEP4_RELEASE_TEXT"))
                        .font(.callout)
                        .fixedSize(horizontal: false, vertical: true)
                        .padding(12)
                        .background(Color(.systemGray6))
                        .clipShape(RoundedRectangle(cornerRadius: 10))

                    // Authorization checkbox — gates the I Authorize button.
                    Toggle(isOn: $authorized) {
                        Text(String(localized: "BIPA_STEP4_CHECKBOX_LABEL"))
                            .font(.headline)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    .toggleStyle(.switch)
                    .padding(.vertical, 4)
                    .accessibilityLabel(String(localized: "BIPA_STEP4_CHECKBOX_LABEL"))
                }
                .padding()
            }

            Divider()

            VStack(spacing: 12) {
                Button(String(localized: "BIPA_STEP4_AUTHORIZE_BUTTON")) {
                    onAccept()
                }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
                .disabled(!authorized)
                .accessibilityLabel(String(localized: "BIPA_STEP4_AUTHORIZE_BUTTON"))

                Button(String(localized: "BIPA_STEP4_DECLINE_BUTTON"), role: .destructive) {
                    onDecline()
                }
                .buttonStyle(.bordered)
                .frame(maxWidth: .infinity)
                .accessibilityLabel(String(localized: "BIPA_STEP4_DECLINE_BUTTON"))
            }
            .padding()
        }
        .navigationTitle(String(localized: "BIPA_STEP4_NAV_TITLE"))
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Shared sub-components (private, file-scope)

/// Pill-style step progress indicator — fills and widens the current step.
private struct BIPAProgressPills: View {
    let current: Int
    private let total = 4

    var body: some View {
        HStack(spacing: 6) {
            ForEach(1...total, id: \.self) { i in
                Capsule()
                    .fill(i <= current ? Color.accentColor : Color(.systemGray4))
                    .frame(width: i == current ? 24 : 8, height: 8)
                    .animation(.easeInOut(duration: 0.2), value: current)
            }
        }
        .accessibilityHidden(true)
    }
}

/// Container view for steps 1–3 — progress pills, scrollable content, and a footer.
private struct BIPAStepShell<Content: View, Footer: View>: View {
    let current: Int
    let symbol: String
    let title: String
    let navTitle: String
    var hideBackButton: Bool = false
    @ViewBuilder let content: () -> Content
    @ViewBuilder let footer: () -> Footer

    var body: some View {
        VStack(spacing: 0) {
            BIPAProgressPills(current: current)
                .padding(.vertical, 12)
            Divider()

            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    // Step header
                    VStack(alignment: .leading, spacing: 12) {
                        Image(systemName: symbol)
                            .font(.system(size: 48))
                            .foregroundColor(.accentColor)
                            .accessibilityHidden(true)
                        Text(title)
                            .font(.largeTitle.bold())
                            .fixedSize(horizontal: false, vertical: true)
                    }

                    content()
                }
                .padding()
            }

            Divider()
            footer().padding()
        }
        .navigationTitle(navTitle)
        .navigationBarTitleDisplayMode(.inline)
        .navigationBarBackButtonHidden(hideBackButton)
    }
}

/// Tinted card for highlighting key information.
private struct BIPAInfoCard<Content: View>: View {
    @ViewBuilder let content: () -> Content

    var body: some View {
        content()
            .padding(12)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color.accentColor.opacity(0.08))
            .clipShape(RoundedRectangle(cornerRadius: 10))
    }
}

/// Tinted card with accent-color checkmark bullets.
private struct BIPACheckmarkCard: View {
    let items: [String]

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            ForEach(items, id: \.self) { item in
                HStack(alignment: .top, spacing: 8) {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundColor(.accentColor)
                        .font(.callout)
                        .accessibilityHidden(true)
                    Text(item)
                        .font(.body)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color.accentColor.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 10))
    }
}

/// Inline heading + plain bullet list used in Step 3.
private struct BIPABulletSection: View {
    let heading: String
    let items: [String]

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(heading)
                .font(.subheadline.bold())
            ForEach(items, id: \.self) { item in
                HStack(alignment: .top, spacing: 8) {
                    Text("•")
                        .font(.body)
                        .accessibilityHidden(true)
                    Text(item)
                        .font(.body)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
        }
    }
}

/// Primary Continue button used by Steps 1–3.
private struct BIPAContinueButton: View {
    let action: () -> Void

    var body: some View {
        Button(String(localized: "BIPA_CONTINUE_BUTTON"), action: action)
            .buttonStyle(.borderedProminent)
            .frame(maxWidth: .infinity)
            .accessibilityLabel(String(localized: "BIPA_CONTINUE_BUTTON"))
    }
}

#Preview {
    BIPADisclosureView(onAccept: {}, onDecline: {})
}
