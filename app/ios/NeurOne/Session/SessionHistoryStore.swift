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
// completedAt is UHDR-class data (CLAUDE.md §5.1 — "session timestamps → UHDR")
// and is NOT stored here. Only the calendar day is persisted (sessionDay, "yyyy-MM-dd"
// in local timezone). Day granularity is sufficient for all display purposes and
// is safe to store in UserDefaults, which is included in iCloud/iTunes backups and
// MDM-accessible. Exact timestamps go to the encrypted UHDR partition only.
// (NP-PRIV-ANALYSIS-002 MEDIUM-07)
//
// The EDF+ file itself (full waveform archive) lives in Documents/UHDR and is
// referenced here only by its hub-assigned session ID (edfSessionID) — the
// store never decodes or stores its contents.

// One persisted history row. Codable for UserDefaults JSON storage.
struct SessionRecord: Identifiable, Codable, Equatable {
    var id: UUID = UUID()
    var protocolName: String
    // Calendar day the session completed — "yyyy-MM-dd" in device local timezone.
    // Day granularity only; exact timestamps are UHDR-class data and not stored here.
    var sessionDay: String
    // Monotonic insertion counter. Tiebreaks same-day records in newest-first order
    // after timestamp coarsening removes sub-day precision.
    var insertionIndex: Int
    var durationSeconds: Double
    var averageCoherenceScore: Float?   // 0.0–10.0; nil when protocol had no HRV
    var rmssdMilliseconds: UInt16?      // integer only — never a raw RR series
    var impedancePassCount: Int         // 0–8 EEG electrodes that passed
    // edfSessionID is held in-memory only — intentionally excluded from Codable
    // serialization. gatt.session.epoch is a Unix-ms timestamp (UHDR-class,
    // CLAUDE.md §5.1) and must not be persisted to the backed-up UserDefaults store.
    // The download button is available in the live post-session window; after app
    // restart the hub presents historical sessions via Mode 4 on reconnect.
    var edfSessionID: UInt32? = nil

    private enum CodingKeys: String, CodingKey {
        case id, protocolName, sessionDay, insertionIndex, durationSeconds
        case averageCoherenceScore, rmssdMilliseconds, impedancePassCount
        // edfSessionID deliberately omitted — see comment above
    }

    // Shared "yyyy-MM-dd" formatter in device local timezone.
    // Used on store write and display.
    static let dayFormatter: DateFormatter = {
        let f = DateFormatter()
        f.dateFormat = "yyyy-MM-dd"
        f.locale = Locale(identifier: "en_US_POSIX")
        return f
    }()
}

@MainActor
final class SessionHistoryStore: ObservableObject {

    // Newest-first ordering is maintained on every mutation.
    @Published private(set) var records: [SessionRecord] = []

    private let defaults: UserDefaults
    private let storageKey = "np.session.history"
    private static let maxRecords = 100
    // Monotonically increasing; tiebreaks same-day records newest-first.
    private var nextInsertionIndex = 0

    private static let logger = Logger(subsystem: "com.neurone.app", category: "SessionHistoryStore")

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        load()
    }

    // Append a completed session, persist, trim, and keep newest-first.
    func record(_ session: CompletedSessionSummary) {
        let new = SessionRecord(
            protocolName: session.protocolName,
            sessionDay: SessionRecord.dayFormatter.string(from: session.completedAt),
            insertionIndex: nextInsertionIndex,
            durationSeconds: Double(session.durationSeconds),
            averageCoherenceScore: session.averageCoherenceScore,
            rmssdMilliseconds: session.rmssdMilliseconds,
            impedancePassCount: session.impedancePassCount,
            edfSessionID: session.edfSessionID
        )
        nextInsertionIndex += 1
        records.insert(new, at: 0)
        records.sort(by: Self.newestFirst)
        if records.count > Self.maxRecords {
            records = Array(records.prefix(Self.maxRecords))
        }
        persist()
    }

    func clearHistory() {
        records = []
        nextInsertionIndex = 0
        persist()
    }

    // MARK: - Persistence

    private func load() {
        guard let data = defaults.data(forKey: storageKey) else { return }
        do {
            var decoded = try JSONDecoder().decode([SessionRecord].self, from: data)
            decoded.sort(by: Self.newestFirst)
            records = decoded
            nextInsertionIndex = (records.map(\.insertionIndex).max() ?? -1) + 1
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

    private static func newestFirst(_ a: SessionRecord, _ b: SessionRecord) -> Bool {
        if a.sessionDay != b.sessionDay { return a.sessionDay > b.sessionDay }
        return a.insertionIndex > b.insertionIndex
    }
}
