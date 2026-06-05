import SwiftUI

// Past-sessions list — accessible from the Session tab toolbar (ISC-48, ISC-49).
//
// Distinct from SessionHistoryView, which is the post-session / per-session
// DETAIL summary. This view is the scrollable index of all persisted
// SessionRecords; each row navigates to the detail view.

struct SessionHistoryListView: View {

    @EnvironmentObject private var historyStore: SessionHistoryStore
    @EnvironmentObject private var edfLoader:    EDFDownloader

    @Environment(\.dismiss) private var dismiss

    @State private var showClearConfirmation = false

    var body: some View {
        NavigationStack {
            Group {
                if historyStore.records.isEmpty {
                    emptyState
                } else {
                    sessionList
                }
            }
            .navigationTitle("Session History")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button("Done") { dismiss() }
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(role: .destructive) {
                        showClearConfirmation = true
                    } label: {
                        Label("Clear History", systemImage: "trash")
                    }
                    .disabled(historyStore.records.isEmpty)
                }
            }
            .alert("Clear Session History?", isPresented: $showClearConfirmation) {
                Button("Clear History", role: .destructive) {
                    historyStore.clearHistory()
                }
                Button("Cancel", role: .cancel) { }
            } message: {
                Text("This removes the on-device list of your past sessions. Your encrypted session data and any downloaded EDF files are not affected.")
            }
        }
    }

    // MARK: - Subviews

    private var sessionList: some View {
        List {
            ForEach(historyStore.records) { record in
                NavigationLink {
                    SessionHistoryView(
                        completedSession: CompletedSessionSummary(record: record),
                        coherenceHistory: coherenceTrend
                    )
                    .environmentObject(edfLoader)
                } label: {
                    SessionRecordRow(record: record)
                }
            }
        }
        .listStyle(.plain)
    }

    // Last-30 average-coherence series (oldest → newest) for the detail sparkline (ISC-50).
    private var coherenceTrend: [Float] {
        historyStore.records
            .prefix(30)
            .reversed()
            .compactMap { $0.averageCoherenceScore }
    }

    private var emptyState: some View {
        VStack(spacing: 12) {
            Image(systemName: "clock.arrow.circlepath")
                .font(.system(size: 44))
                .foregroundColor(.secondary)
            Text("No sessions yet. Complete your first session to see it here.")
                .font(.subheadline)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 40)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

// MARK: - Row

private struct SessionRecordRow: View {

    let record: SessionRecord

    var body: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Text(record.protocolName)
                    .font(.subheadline.bold())
                    .foregroundColor(.primary)
                Text(relativeDate)
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            Spacer()
            Text(durationFormatted)
                .font(.callout.monospacedDigit())
                .foregroundColor(.secondary)
            coherenceChip
        }
        .padding(.vertical, 4)
    }

    private var coherenceChip: some View {
        Group {
            if let score = record.averageCoherenceScore {
                Text(String(format: "%.1f", score))
                    .font(.caption.bold())
                    .foregroundColor(.white)
                    .padding(.horizontal, 8).padding(.vertical, 4)
                    .background(coherenceColor(score))
                    .clipShape(Capsule())
            } else {
                Text("—")
                    .font(.caption.bold())
                    .foregroundColor(.secondary)
                    .padding(.horizontal, 8).padding(.vertical, 4)
                    .background(Color(.systemGray5))
                    .clipShape(Capsule())
            }
        }
        .frame(minWidth: 36)
    }

    private func coherenceColor(_ score: Float) -> Color {
        score >= 7 ? .green : score >= 4 ? .yellow : .orange
    }

    private var relativeDate: String {
        let cal = Calendar.current
        let timeFmt = DateFormatter()
        timeFmt.dateFormat = "HH:mm"
        let time = timeFmt.string(from: record.completedAt)

        if cal.isDateInToday(record.completedAt) {
            return "Today \(time)"
        }
        if cal.isDateInYesterday(record.completedAt) {
            return "Yesterday \(time)"
        }
        let dateFmt = DateFormatter()
        dateFmt.dateStyle = .medium
        dateFmt.timeStyle = .short
        return dateFmt.string(from: record.completedAt)
    }

    private var durationFormatted: String {
        let total = Int(record.durationSeconds)
        let h = total / 3600
        let m = (total % 3600) / 60
        let s = total % 60
        if h > 0 { return String(format: "%d:%02d:%02d", h, m, s) }
        return String(format: "%d:%02d", m, s)
    }
}
