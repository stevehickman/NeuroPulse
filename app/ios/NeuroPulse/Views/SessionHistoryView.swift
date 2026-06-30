import SwiftUI
import OSLog

// Per-session detail / post-session summary screen.
// Displays protocol name, duration, per-session metrics (RMSSD, average
// coherence, EEG impedance pass count), a last-30-sessions coherence trend
// sparkline (ISC-50), the Adaptive Adjustments card, and — when the session has
// an EDF+ archive — a Download EDF button (Mode 4, ISC-53/54/55).
//
// The AdaptiveAdjustmentsCard always renders — uses an empty state when there
// are no adaptive events (informs user the session ran without adjustment).

struct SessionHistoryView: View {

    let completedSession: CompletedSessionSummary

    // Oldest → newest average-coherence values for the trend sparkline (ISC-50).
    // Empty when opened as a fresh post-session summary with no history context.
    var coherenceHistory: [Float] = []

    @EnvironmentObject private var edfLoader: EDFDownloader

    @State private var edfState: EDFButtonState = .idle
    @State private var edfDownloadTask: Task<Void, Never>?

    private static let logger = Logger(subsystem: "com.neuropulse.app", category: "SessionHistoryView")

    private enum EDFButtonState: Equatable {
        case idle
        case downloading
        case downloaded(URL)
        case failed(String)
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 20) {
                    summaryHeader
                    metricsGrid
                    if coherenceHistory.count >= 2 {
                        coherenceTrendCard
                    }
                    AdaptiveAdjustmentsCard(events: completedSession.adaptationEvents)
                    if completedSession.edfSessionID != nil {
                        edfDownloadSection
                    }
                    privacyFooter
                }
                .padding()
            }
            .navigationTitle("Session Summary")
            .navigationBarTitleDisplayMode(.inline)
            .onDisappear { edfDownloadTask?.cancel() }
        }
    }

    // MARK: - Header

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
            Text(String(localized: "HISTORY_DURATION_LABEL"))
                .font(.caption2)
                .foregroundColor(.secondary)
        }
    }

    // MARK: - Metrics (ISC-50)

    private var metricsGrid: some View {
        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
            MetricCard(
                title: "Avg Coherence",
                value: completedSession.averageCoherenceScore.map { String(format: "%.1f / 10", $0) } ?? "—",
                icon: "waveform.path.ecg",
                color: coherenceColor(completedSession.averageCoherenceScore)
            )
            MetricCard(
                title: "RMSSD",
                value: completedSession.rmssdMilliseconds.map { "\($0) ms" } ?? "—",
                icon: "heart.fill",
                color: .pink
            )
            MetricCard(
                title: "EEG Contacts",
                value: "\(completedSession.impedancePassCount) / 8",
                icon: "brain",
                color: completedSession.impedancePassCount == 8 ? .green : .orange
            )
        }
    }

    private var coherenceTrendCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label("Coherence Trend", systemImage: "chart.line.uptrend.xyaxis")
                .font(.caption)
                .foregroundColor(.secondary)
            CoherenceSparkline(values: coherenceHistory)
                .frame(height: 44)
            Text(String(localized: "HISTORY_COHERENCE_CHART_TITLE").replacingOccurrences(of: "{0}", with: "\(coherenceHistory.count)"))
                .font(.caption2)
                .foregroundColor(.secondary)
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    // MARK: - EDF download (Mode 4, ISC-53/54/55)

    private var edfDownloadSection: some View {
        VStack(spacing: 8) {
            switch edfState {
            case .idle:
                Button {
                    startEDFDownload()
                } label: {
                    Label("Download EDF", systemImage: "arrow.down.doc")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)

            case .downloading:
                HStack(spacing: 10) {
                    ProgressView()
                    Text(String(localized: "HISTORY_DOWNLOADING"))
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)

            case .downloaded(let url):
                VStack(spacing: 4) {
                    Label(String(localized: "SESSION_DOWNLOADED_LABEL"), systemImage: "checkmark.circle.fill")
                        .font(.subheadline.bold())
                        .foregroundColor(.green)
                    Text(url.lastPathComponent)
                        .font(.caption2.monospaced())
                        .foregroundColor(.secondary)
                    Text(String(localized: "HISTORY_DOWNLOAD_HINT"))
                        .font(.caption2)
                        .foregroundColor(.secondary)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)

            case .failed(let message):
                VStack(spacing: 8) {
                    Label(message, systemImage: "exclamationmark.triangle")
                        .font(.caption)
                        .foregroundColor(.orange)
                        .multilineTextAlignment(.center)
                    Button(String(localized: "SESSION_DOWNLOAD_RETRY")) { startEDFDownload() }
                        .font(.caption.bold())
                }
                .frame(maxWidth: .infinity)
            }
        }
    }

    // ISC-55: the app is a passthrough — EDFDownloader writes the hub's EDF+ file
    // to Documents/UHDR unmodified. We never decode or rewrite the patient header.
    private func startEDFDownload() {
        guard let sessionID = completedSession.edfSessionID else { return }
        guard edfState != .downloading else { return }
        edfState = .downloading
        edfDownloadTask = Task {
            defer { if edfState == .downloading { edfState = .idle } }
            do {
                let session = try await edfLoader.requestDownload(sessionID: sessionID)
                if let url = session.localURL {
                    edfState = .downloaded(url)
                } else {
                    edfState = .failed("Download completed but no file location was returned.")
                }
            } catch is CancellationError {
                // Task cancelled (view dismissed mid-download). defer resets to .idle.
                Self.logger.info("EDF download cancelled")
            } catch let error as EDFDownloadError {
                let desc = error.errorDescription ?? "EDF download failed. Check hub connection."
                Self.logger.error("EDF download failed: \(desc, privacy: .public)")
                edfState = .failed(desc)
            } catch {
                Self.logger.error("EDF download failed (unexpected): \(String(describing: type(of: error)), privacy: .public)")
                edfState = .failed("Download failed. Please try again.")
            }
        }
    }

    // MARK: - Footer + helpers

    private var privacyFooter: some View {
        Text(String(localized: "HISTORY_ADAPT_PRIVACY_FOOTER"))
            .font(.caption2)
            .foregroundColor(.secondary)
            .multilineTextAlignment(.center)
            .padding(.horizontal)
    }

    private func coherenceColor(_ score: Float?) -> Color {
        guard let score else { return .secondary }
        return score >= 7 ? .green : score >= 4 ? .yellow : .orange
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
        f.timeStyle = .none
        return f.string(from: completedSession.completedAt)
    }
}

// MARK: - Sparkline

// Lightweight coherence trend line. Avoids the Charts framework dependency to
// keep the view available on the iOS 17 floor without extra import surface.
private struct CoherenceSparkline: View {

    let values: [Float]   // oldest → newest, 0.0–10.0

    var body: some View {
        GeometryReader { geo in
            let pts = points(in: geo.size)
            ZStack {
                Path { path in
                    guard let first = pts.first else { return }
                    path.move(to: first)
                    for p in pts.dropFirst() { path.addLine(to: p) }
                }
                .stroke(Color.blue, style: StrokeStyle(lineWidth: 2, lineJoin: .round))

                if let last = pts.last {
                    Circle()
                        .fill(Color.blue)
                        .frame(width: 6, height: 6)
                        .position(last)
                }
            }
        }
    }

    private func points(in size: CGSize) -> [CGPoint] {
        guard values.count >= 2 else { return [] }
        let maxScore: Float = 10
        let stepX = size.width / CGFloat(values.count - 1)
        return values.enumerated().map { idx, v in
            let clamped = min(max(v, 0), maxScore)
            let y = size.height * (1 - CGFloat(clamped / maxScore))
            return CGPoint(x: CGFloat(idx) * stepX, y: y)
        }
    }
}
