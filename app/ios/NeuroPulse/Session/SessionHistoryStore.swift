import Foundation
import OSLog

// Local, display-only record of completed sessions (ISC-48, ISC-49).
//
// PRIVACY (UHDR boundary): this store NEVER persists raw EEG/HRV waveform data.
// It holds only display-aggregated metrics already shown post-session:
// average coherence score, RMSSD as an integer, and the EEG impedance pass count.
// These are the same values surfaced live in SessionView. Raw biological time
// series remains exclusively in the on-device encrypted UHDR partition and is
// never copied into UserDefaults.
//
// The EDF+ file itself (full waveform archive) lives in Documents/UHDR and is
// referenced here only by its hub-assigned session ID (`edfSessionID`) — the
// store never decodes or stores its contents.

// One persisted history row. Codable for UserDefaults JSON storage.
struct SessionRecord: Identifiable, Codable, Equatable {
    var id: UUID = UUID()
    var protocolName: String
    var completedAt: Date
    var durationSeconds: Double
    var averageCoherenceScore: Float?   // 0.0–10.0; nil when protocol had no HRV
    var rmssdMilliseconds: UInt16?      // integer only — never a raw RR series
    var impedancePassCount: Int         // 0–8 EEG electrodes that passed
    var edfSessionID: UInt32?           // hub session ID for Mode 4 EDF download; nil if unavailable
}

@MainActor
final class SessionHistoryStore: ObservableObject {

    // Newest-first ordering is maintained on every mutation.
    @Published private(set) var records: [SessionRecord] = []

    private let defaults: UserDefaults
    private let storageKey = "np.session.history"
    private static let maxRecords = 100

    private static let logger = Logger(subsystem: "com.neuropulse.app", category: "SessionHistoryStore")

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        load()
    }

    // Append a completed session, persist, trim, and keep newest-first.
    func record(_ session: CompletedSessionSummary) {
        let new = SessionRecord(
            protocolName: session.protocolName,
            completedAt: session.completedAt,
            durationSeconds: Double(session.durationSeconds),
            averageCoherenceScore: session.averageCoherenceScore,
            rmssdMilliseconds: session.rmssdMilliseconds,
            impedancePassCount: session.impedancePassCount,
            edfSessionID: session.edfSessionID
        )
        records.insert(new, at: 0)
        records.sort { $0.completedAt > $1.completedAt }
        if records.count > Self.maxRecords {
            records = Array(records.prefix(Self.maxRecords))
        }
        persist()
    }

    func clearHistory() {
        records = []
        persist()
    }

    // MARK: - Persistence

    private func load() {
        guard let data = defaults.data(forKey: storageKey) else { return }
        do {
            let decoded = try JSONDecoder().decode([SessionRecord].self, from: data)
            records = decoded.sorted { $0.completedAt > $1.completedAt }
        } catch {
            // Corrupt blob is non-fatal — start clean rather than crash on launch.
            Self.logger.error("Failed to decode session history: \(String(describing: error))")
            records = []
        }
    }

    private func persist() {
        do {
            let data = try JSONEncoder().encode(records)
            defaults.set(data, forKey: storageKey)
        } catch {
            Self.logger.error("Failed to encode session history: \(String(describing: error))")
        }
    }
}
