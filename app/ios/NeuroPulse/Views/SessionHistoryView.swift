import SwiftUI

// Post-session summary screen shown after a session completes.
// Displays protocol name, duration, and the Adaptive Adjustments card.
// The AdaptiveAdjustmentsCard always renders — uses an empty state when there
// are no adaptive events (informs user the session ran without adjustment).

struct SessionHistoryView: View {

    let completedSession: CompletedSessionSummary

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 20) {
                    summaryHeader
                    AdaptiveAdjustmentsCard(events: completedSession.adaptationEvents)
                    privacyFooter
                }
                .padding()
            }
            .navigationTitle("Session Summary")
            .navigationBarTitleDisplayMode(.inline)
        }
    }

    // MARK: - Subviews

    private var summaryHeader: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text(completedSession.protocolName)
                        .font(.title3.bold())
                    Text(completedAt)
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                Spacer()
                durationBadge
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    private var durationBadge: some View {
        VStack(spacing: 2) {
            Text(durationFormatted)
                .font(.title2.bold().monospacedDigit())
            Text("duration")
                .font(.caption2)
                .foregroundColor(.secondary)
        }
    }

    private var durationFormatted: String {
        let total = Int(completedSession.durationSeconds)
        let h = total / 3600
        let m = (total % 3600) / 60
        let s = total % 60
        if h > 0 { return String(format: "%d:%02d:%02d", h, m, s) }
        return String(format: "%d:%02d", m, s)
    }

    private var completedAt: String {
        let f = DateFormatter()
        f.dateStyle = .medium
        f.timeStyle = .short
        return f.string(from: completedSession.completedAt)
    }

    private var privacyFooter: some View {
        Text("Adaptive adjustments are made automatically during closed-loop sessions based on your real-time brainwave and heart-rate activity. No raw biological values are stored outside your device. Learn more in the Privacy & Data section of Settings.")
            .font(.caption2)
            .foregroundColor(.secondary)
            .multilineTextAlignment(.center)
            .padding(.horizontal)
    }
}
