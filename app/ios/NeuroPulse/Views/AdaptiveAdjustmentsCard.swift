import SwiftUI

// "Adaptive Adjustments" card shown in Session History.
// Displays up to 5 closed-loop adaptation events; collapses longer lists
// behind a "View all" sheet.  Renders plain-language copy only — no raw values.
// Requires Privacy Lead sign-off (OI-PA-04) before shipping (NP-PRIV-REM-001 STEP-33).

struct AdaptiveAdjustmentsCard: View {

    let events: [AdaptationEvent]

    private static let maxInlineEvents = 5

    @State private var showAllEvents = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            cardHeader
            if events.isEmpty {
                emptyState
            } else {
                eventList
                if events.count > Self.maxInlineEvents {
                    overflowFooter
                }
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
        .sheet(isPresented: $showAllEvents) {
            AllAdaptationEventsSheet(events: events)
        }
    }

    // MARK: - Subviews

    private var cardHeader: some View {
        HStack {
            Label("Adaptive Adjustments", systemImage: "waveform.path")
                .font(.headline)
            Spacer()
            Text("\(events.count)")
                .font(.caption.bold())
                .foregroundColor(.secondary)
                .padding(.horizontal, 8).padding(.vertical, 2)
                .background(Color(.systemGray5))
                .clipShape(Capsule())
        }
    }

    private var emptyState: some View {
        Text("No adaptive adjustments this session.")
            .font(.subheadline)
            .foregroundColor(.secondary)
    }

    private var eventList: some View {
        VStack(alignment: .leading, spacing: 8) {
            ForEach(events.prefix(Self.maxInlineEvents)) { event in
                AdaptationEventRow(event: event)
            }
        }
    }

    private var overflowFooter: some View {
        let remaining = events.count - Self.maxInlineEvents
        return HStack {
            Text("and \(remaining) more")
                .font(.caption)
                .foregroundColor(.secondary)
            Spacer()
            Button("View all") { showAllEvents = true }
                .font(.caption.bold())
        }
    }
}

// MARK: - Row

private struct AdaptationEventRow: View {

    let event: AdaptationEvent

    var body: some View {
        HStack(alignment: .top, spacing: 10) {
            Image(systemName: "arrow.triangle.2.circlepath")
                .font(.caption)
                .foregroundColor(.blue)
                .frame(width: 16, height: 16)
                .padding(.top, 2)
            VStack(alignment: .leading, spacing: 2) {
                Text(event.trigger.plainLanguageDescription)
                    .font(.subheadline)
                    .fixedSize(horizontal: false, vertical: true)
                Text(offsetLabel(event.sessionOffset))
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
        }
    }

    private func offsetLabel(_ offset: TimeInterval) -> String {
        let total = Int(offset)
        let h = total / 3600
        let m = (total % 3600) / 60
        let s = total % 60
        if h > 0 {
            return String(format: "%d:%02d:%02d into session", h, m, s)
        }
        return String(format: "%d:%02d into session", m, s)
    }
}

// MARK: - All Events Sheet

private struct AllAdaptationEventsSheet: View {

    let events: [AdaptationEvent]
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List(events) { event in
                AdaptationEventRow(event: event)
                    .listRowBackground(Color.clear)
                    .listRowSeparator(.hidden)
                    .padding(.vertical, 4)
            }
            .listStyle(.plain)
            .navigationTitle("All Adjustments")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
    }
}
