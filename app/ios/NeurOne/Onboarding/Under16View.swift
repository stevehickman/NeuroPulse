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
        static let intro          = "You can still use NeurOne, but a few features rely"
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
                    title: String(localized: "MODALITY_EEG_NEUROFEEDBACK_NAME"),
                    detail: String(localized: "UNDER16_BRAINWAVE_RECORDING_AND_TRAINING")),
        FeatureLine(symbol: "heart.text.square",
                    title: String(localized: "UNDER16_HRV_BIOFEEDBACK"),
                    detail: String(localized: "UNDER16_HEART_RATE_VARIABILITY_COHERENCE_TRAINING")),
        FeatureLine(symbol: "arrow.triangle.2.circlepath",
                    title: String(localized: "UNDER16_CLOSED_LOOP_ADAPTIVE_STIMULATION"),
                    detail: String(localized: "UNDER16_SESSIONS_THAT_ADAPT_IN_REAL_TIME_TO_YOUR_BRA"))
    ]

    private let available: [FeatureLine] = [
        FeatureLine(symbol: "light.max",
                    title: String(localized: "UNDER16_PHOTOBIOMODULATION_PBM"),
                    detail: String(localized: "UNDER16_LIGHT_BASED_TRANSCRANIAL_AND_INTRANASAL_SESS")),
        FeatureLine(symbol: "waveform",
                    title: String(localized: "MODALITY_AUDIO_ENTRAINMENT_NAME"),
                    detail: String(localized: "UNDER16_BINAURAL_BEATS_ISOCHRONIC_TONES_AND_NOISE")),
        FeatureLine(symbol: "eye",
                    title: String(localized: "MODALITY_VISUAL_STIMULATION_NAME"),
                    detail: String(localized: "UNDER16_PHOTIC_AND_EMDR_STYLE_LIGHT_SESSIONS"))
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
