import SwiftUI

// Under-16 explanatory screen — reached from AgeGateView via the
// "I am under 16" path. Satisfies ISC-130: the age gate has no Skip, but
// offers a graceful path that explains which features become unavailable.
//
// Health-data-dependent features (EEG neurofeedback, HRV biofeedback,
// closed-loop adaptive stimulation) require the user to be 16+ to consent.
// PBM, audio entrainment, and visual stimulation remain available.

struct Under16View: View {

    @Environment(\.dismiss) private var dismiss

    // MARK: - User-visible strings (prepared for Localizable.strings; literals for now)

    enum Strings {
        static let title          = "Some Features Need Age Confirmation"
        static let intro          = "You can still use NeuroPulse, but a few features rely"
            + " on health-related data that, by law, requires you to be at least 16 years old to consent to."
        static let unavailableHdr = "Not available without age confirmation"
        static let availableHdr   = "Available to everyone"
        static let footer         = "If you are 16 or older, go back and confirm your age to unlock all features."
        static let backLabel      = "Back"
    }

    private struct FeatureLine: Identifiable {
        let id = UUID()
        let symbol: String
        let title: String
        let detail: String
    }

    private let unavailable: [FeatureLine] = [
        FeatureLine(symbol: "brain.head.profile",
                    title: "EEG Neurofeedback",
                    detail: "Brainwave recording and training."),
        FeatureLine(symbol: "heart.text.square",
                    title: "HRV Biofeedback",
                    detail: "Heart-rate-variability coherence training."),
        FeatureLine(symbol: "arrow.triangle.2.circlepath",
                    title: "Closed-Loop Adaptive Stimulation",
                    detail: "Sessions that adapt in real time to your brain activity.")
    ]

    private let available: [FeatureLine] = [
        FeatureLine(symbol: "light.max",
                    title: "Photobiomodulation (PBM)",
                    detail: "Light-based transcranial and intranasal sessions."),
        FeatureLine(symbol: "waveform",
                    title: "Audio Entrainment",
                    detail: "Binaural beats, isochronic tones, and noise."),
        FeatureLine(symbol: "eye",
                    title: "Visual Stimulation",
                    detail: "Photic and EMDR-style light sessions.")
    ]

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 24) {
                    header
                    section(title: Strings.unavailableHdr,
                            lines: unavailable,
                            tint: .secondary)
                    section(title: Strings.availableHdr,
                            lines: available,
                            tint: .green)
                    Text(Strings.footer)
                        .font(.callout)
                        .foregroundColor(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .padding()
            }

            Divider()
            Button(Strings.backLabel) {
                dismiss()
            }
            .buttonStyle(.borderedProminent)
            .frame(maxWidth: .infinity)
            .accessibilityLabel(Strings.backLabel)
            .padding()
        }
        .navigationTitle(Strings.title)
        .navigationBarTitleDisplayMode(.inline)
    }

    // MARK: - Header

    private var header: some View {
        VStack(alignment: .leading, spacing: 12) {
            Image(systemName: "info.circle.fill")
                .font(.system(size: 44))
                .foregroundColor(.accentColor)
                .accessibilityHidden(true)
            Text(Strings.intro)
                .font(.body)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    // MARK: - Section

    private func section(title: String, lines: [FeatureLine], tint: Color) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title)
                .font(.headline)
            ForEach(lines) { line in
                HStack(alignment: .top, spacing: 12) {
                    Image(systemName: line.symbol)
                        .font(.title3)
                        .foregroundColor(tint)
                        .frame(width: 28)
                        .accessibilityHidden(true)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(line.title)
                            .font(.subheadline.bold())
                        Text(line.detail)
                            .font(.caption)
                            .foregroundColor(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
                .accessibilityElement(children: .combine)
            }
        }
    }
}

#Preview {
    NavigationStack {
        Under16View()
    }
}
