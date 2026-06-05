import SwiftUI

// BIPA written-release disclosure screen.
// Launch-blocking privacy compliance requirement for any Illinois user
// (Illinois Biometric Information Privacy Act, 740 ILCS 14).
//
// EEG brainwave data is biometric information under BIPA. A written release is
// required before that data is collected or used. This screen presents the full
// disclosure verbatim from NP-APP-ROADMAP-001 Rev B §9.3 and records an explicit
// accept/decline choice — there is NO Skip or dismiss affordance.
//
// Satisfies: ISC-88, ISC-89 (presentation context handled by caller),
// ISC-91 (re-presentable from Settings), ISC-160 (no truncation).

struct BIPADisclosureView: View {

    // MARK: - User-visible strings (prepared for Localizable.strings; literals for now)

    enum Strings {
        static let title = "Brain Activity Data Consent (Illinois)"

        static let intro = """
        NeuroPulse collects your brainwave (EEG) data during sessions to \
        provide neurofeedback and to adapt stimulation settings in real time. \
        Under Illinois law (BIPA), this brainwave data is considered biometric \
        information.
        """

        static let bullets: [String] = [
            "Purpose: Session operation, neurofeedback display, closed-loop adaptation",
            "Retention: Until you delete your data or transfer/sell your device",
            "Destruction method: Secure hardware-level erasure (eMMC SANITIZE)",
            "NeuroPulse will not sell, lease, or profit from your brainwave data",
            "NeuroPulse will not share your brainwave data with third parties without your separate consent, except as required by law"
        ]

        static let question = """
        Do you consent to NeuroPulse collecting and using your brainwave data \
        as described above?
        """

        static let acceptLabel  = "Yes, I consent"
        static let declineLabel = "No, decline"
    }

    /// Called when the user consents.
    let onAccept: () -> Void

    /// Called when the user declines.
    let onDecline: () -> Void

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                ScrollView {
                    VStack(alignment: .leading, spacing: 20) {
                        header
                        Text(Strings.intro)
                            .font(.body)
                            .fixedSize(horizontal: false, vertical: true)

                        bulletList

                        Text(Strings.question)
                            .font(.headline)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                    .padding()
                }

                Divider()
                actionButtons
                    .padding()
            }
            .navigationTitle(Strings.title)
            .navigationBarTitleDisplayMode(.inline)
            // No toolbar dismiss / Skip — the user must make an explicit choice (ISC-88).
            .interactiveDismissDisabled(true)
        }
    }

    // MARK: - Header

    private var header: some View {
        VStack(alignment: .leading, spacing: 12) {
            Image(systemName: "brain.head.profile")
                .font(.system(size: 48))
                .foregroundColor(.accentColor)
                .accessibilityHidden(true)

            Text(Strings.title)
                .font(.largeTitle.bold())
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    // MARK: - Bullet list

    private var bulletList: some View {
        VStack(alignment: .leading, spacing: 12) {
            ForEach(Strings.bullets, id: \.self) { line in
                HStack(alignment: .top, spacing: 8) {
                    Text("•")
                        .font(.body)
                        .accessibilityHidden(true)
                    Text(line)
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

    // MARK: - Action buttons

    private var actionButtons: some View {
        VStack(spacing: 12) {
            Button(Strings.acceptLabel) {
                onAccept()
            }
            .buttonStyle(.borderedProminent)
            .frame(maxWidth: .infinity)
            .accessibilityLabel(Strings.acceptLabel)

            Button(Strings.declineLabel, role: .destructive) {
                onDecline()
            }
            .buttonStyle(.bordered)
            .frame(maxWidth: .infinity)
            .accessibilityLabel(Strings.declineLabel)
        }
    }
}

#Preview {
    BIPADisclosureView(onAccept: {}, onDecline: {})
}
