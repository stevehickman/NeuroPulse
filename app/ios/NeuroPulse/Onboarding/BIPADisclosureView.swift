import SwiftUI

// Biometric data consent disclosure screen.
// Shown to ALL users before their first EEG session — not gated by locale.
//
// EEG brainwave data is biometric information under Illinois BIPA (740 ILCS 14),
// GDPR Art. 9 (special category data), and WA MHMD (consumer health data). A
// written release is required before that data is collected or used. This screen
// records an explicit accept/decline choice — there is NO Skip or dismiss affordance.
//
// Satisfies: ISC-88, ISC-89 (presentation context handled by caller),
// ISC-91 (re-presentable from Settings), ISC-160 (no truncation).

struct BIPADisclosureView: View {

    // MARK: - User-visible strings

    enum Strings {
        static var title: String        { String(localized: "BIPA_BRAIN_TITLE") }
        static var intro: String        { String(localized: "BIPA_BRAIN_INTRO") }
        static var bullets: [String]    {
            [
                String(localized: "BIPA_BRAIN_BULLET_1"),
                String(localized: "BIPA_BRAIN_BULLET_2"),
                String(localized: "BIPA_BRAIN_BULLET_3"),
                String(localized: "BIPA_BRAIN_BULLET_4"),
                String(localized: "BIPA_BRAIN_BULLET_5"),
            ]
        }
        static var question: String     { String(localized: "BIPA_BRAIN_QUESTION") }
        static var acceptLabel: String  { String(localized: "BIPA_BRAIN_ACCEPT") }
        static var declineLabel: String { String(localized: "BIPA_BRAIN_DECLINE") }
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
