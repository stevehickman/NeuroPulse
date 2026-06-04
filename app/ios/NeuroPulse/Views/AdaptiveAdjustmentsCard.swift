import SwiftUI

// Adaptive Adjustments card — NP-APP-ROADMAP-001 Rev B §9.4, STEP-33.
// Displayed in Session History. Shows plain-language descriptions of any
// closed-loop parameter changes that occurred during the session.
// Satisfies GDPR Art. 13(2)(f) disclosure requirement.

struct AdaptiveAdjustmentsCard: View {

    let events: [AdaptationEvent]

    var body: some View {
        if events.isEmpty {
            EmptyView()
        } else {
            VStack(alignment: .leading, spacing: 12) {
                Label("Adaptive Adjustments", systemImage: "arrow.triangle.2.circlepath")
                    .font(.headline)

                Text("The session made \(events.count) automatic adjustment\(events.count == 1 ? "" : "s") in response to your body's signals.")
                    .font(.subheadline)
                    .foregroundColor(.secondary)

                ForEach(events) { event in
                    eventRow(event)
                }
            }
            .padding()
            .background(Color(.systemGray6))
            .clipShape(RoundedRectangle(cornerRadius: 12))
        }
    }

    private func eventRow(_ event: AdaptationEvent) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(event.modality)
                    .font(.subheadline.bold())
                Spacer()
                Text(event.timestamp, style: .time)
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            Text(event.trigger.rawValue)
                .font(.caption)
                .foregroundColor(.secondary)
            Text("\(event.parameterChanged): \(event.previousValue, specifier: "%.1f") → \(event.newValue, specifier: "%.1f") \(event.unit)")
                .font(.caption2)
                .foregroundColor(.secondary)
        }
        .padding(.vertical, 4)
    }
}

#Preview {
    AdaptiveAdjustmentsCard(events: [
        AdaptationEvent(modality: "BES", trigger: .eegBandShift,
                        parameterChanged: "Frequency", previousValue: 10.0,
                        newValue: 12.0, unit: "Hz")
    ])
    .padding()
}
