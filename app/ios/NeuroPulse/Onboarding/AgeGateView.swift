import SwiftUI

// Minimum age gate — the FIRST screen in the onboarding flow.
// Launch-blocking privacy compliance requirement (NP-PRIV-001 Rev B MEDIUM-03).
// Covers COPPA (under-13 bar), GDPR Art. 8 (under-16 bar in most EU states),
// and BIPA (minors cannot provide valid biometric written release).
//
// Satisfies: ISC-83, ISC-84, ISC-85, ISC-86, ISC-87, ISC-130.
//
// The checkbox (`Toggle`) is bound to `@State var ageConfirmed`, which is
// initialised to `false`. No code path in this file sets it to `true`.
// Continue is `.disabled(!ageConfirmed)` so it cannot be tapped until confirmed.

struct AgeGateView: View {

    /// UserDefaults key persisting age-gate completion (ISC-87).
    static let ageConfirmedDefaultsKey = "np.onboarding.age-confirmed"

    // MARK: - User-visible strings

    enum Strings {
        static var title: String         { String(localized: "AGE_GATE_TITLE") }
        static var subtitle: String      { String(localized: "AGE_GATE_SUBTITLE") }
        static var declaration: String   { String(localized: "AGE_GATE_DECLARATION") }
        static var continueLabel: String { String(localized: "AGE_GATE_CONTINUE") }
        static var under16Label: String  { String(localized: "AGE_GATE_UNDER_16") }
        static var whyTitle: String      { String(localized: "AGE_GATE_WHY_TITLE") }
        static var whyBody: String       { String(localized: "AGE_GATE_WHY_BODY") }
    }

    /// Age confirmation state. Initialised to `false` — never pre-set to `true`.
    @State private var ageConfirmed = false

    /// Navigation to the under-16 explanatory screen.
    @State private var showUnder16 = false

    /// Called when the user confirms their age and taps Continue.
    let onComplete: () -> Void

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                ScrollView {
                    VStack(alignment: .leading, spacing: 24) {
                        header
                        whyCard
                        declarationToggle
                    }
                    .padding()
                }

                Divider()
                actionButtons
                    .padding()
            }
            .navigationTitle(Strings.title)
            .navigationBarTitleDisplayMode(.inline)
            .navigationDestination(isPresented: $showUnder16) {
                Under16View()
            }
        }
    }

    // MARK: - Header

    private var header: some View {
        VStack(alignment: .leading, spacing: 12) {
            Image(systemName: "person.badge.shield.checkmark.fill")
                .font(.system(size: 48))
                .foregroundColor(.accentColor)
                .accessibilityHidden(true)

            Text(Strings.title)
                .font(.largeTitle.bold())

            Text(Strings.subtitle)
                .font(.body)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    // MARK: - Why card

    private var whyCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label(Strings.whyTitle, systemImage: "info.circle.fill")
                .font(.subheadline.bold())
                .foregroundColor(.accentColor)
            Text(Strings.whyBody)
                .font(.caption)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color.accentColor.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 10))
    }

    // MARK: - Declaration toggle (the age checkbox)

    private var declarationToggle: some View {
        Toggle(isOn: $ageConfirmed) {
            Text(Strings.declaration)
                .font(.headline)
                .fixedSize(horizontal: false, vertical: true)
        }
        .toggleStyle(.switch)
        .accessibilityLabel(Strings.declaration)
        .padding(.vertical, 8)
    }

    // MARK: - Action buttons

    private var actionButtons: some View {
        VStack(spacing: 12) {
            Button(Strings.continueLabel) {
                confirmAndContinue()
            }
            .buttonStyle(.borderedProminent)
            .frame(maxWidth: .infinity)
            .disabled(!ageConfirmed)
            .accessibilityLabel(Strings.continueLabel)

            Button(Strings.under16Label) {
                showUnder16 = true
            }
            .buttonStyle(.bordered)
            .frame(maxWidth: .infinity)
            .accessibilityLabel(Strings.under16Label)
        }
    }

    // MARK: - Actions

    private func confirmAndContinue() {
        // Guard: only proceed when confirmed. The button is already disabled
        // until `ageConfirmed`, but this keeps the side effect honest.
        guard ageConfirmed else { return }
        UserDefaults.standard.set(true, forKey: Self.ageConfirmedDefaultsKey)
        onComplete()
    }
}

#Preview {
    AgeGateView(onComplete: {})
}
