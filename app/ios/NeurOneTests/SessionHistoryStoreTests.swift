//
//  SessionHistoryStoreTests.swift
//  NeurOneTests
//
//  Subject: SessionHistoryStore (app/ios/NeurOne/Session/SessionHistoryStore.swift)
//
//  PRIVACY invariant: store never persists raw EEG/HRV waveforms — only
//  aggregated display metrics. Each test that touches UserDefaults uses an
//  isolated suite so standard-suite data is never contaminated.

import XCTest
@testable import NeurOne

@MainActor
final class SessionHistoryStoreTests: XCTestCase {

    // MARK: - Helpers

    private func makeSuite(_ name: String) -> UserDefaults {
        let suite = UserDefaults(suiteName: name)!
        suite.removePersistentDomain(forName: name)
        return suite
    }

    private func makeSummary(
        protocolName: String = "Alpha Calm",
        durationSeconds: UInt32 = 600,
        completedAt: Date = Date(),
        coherence: Float? = 7.5,
        rmssd: UInt16? = 42,
        impedancePassed: Int = 8,
        edfID: UInt32? = 1001
    ) -> CompletedSessionSummary {
        CompletedSessionSummary(
            protocolName: protocolName,
            durationSeconds: durationSeconds,
            adaptationEvents: [],
            completedAt: completedAt,
            averageCoherenceScore: coherence,
            rmssdMilliseconds: rmssd,
            impedancePassCount: impedancePassed,
            edfSessionID: edfID
        )
    }

    // MARK: - insert

    func testRecordAppendsToList() {
        let store = SessionHistoryStore(defaults: makeSuite(#function))
        XCTAssertTrue(store.records.isEmpty)

        store.record(makeSummary(protocolName: "Focus Prime"))

        XCTAssertEqual(store.records.count, 1)
        XCTAssertEqual(store.records.first?.protocolName, "Focus Prime")
    }

    func testRecordMapsAllFields() {
        let suite = makeSuite(#function)
        let store = SessionHistoryStore(defaults: suite)
        let at = Date(timeIntervalSince1970: 1_700_000_000)

        store.record(makeSummary(
            protocolName: "Gamma 40Hz",
            durationSeconds: 1200,
            completedAt: at,
            coherence: 8.3,
            rmssd: 55,
            impedancePassed: 6,
            edfID: 9999
        ))

        let r = store.records[0]
        XCTAssertEqual(r.protocolName, "Gamma 40Hz")
        XCTAssertEqual(r.durationSeconds, 1200, accuracy: 0.01)
        // sessionDay stores the calendar date at day granularity (UHDR boundary — exact
        // timestamps are not persisted to UserDefaults; NP-PRIV-ANALYSIS-002 MEDIUM-07).
        XCTAssertEqual(r.sessionDay, SessionRecord.dayFormatter.string(from: at))
        XCTAssertEqual(r.averageCoherenceScore ?? 0, 8.3, accuracy: 0.01)
        XCTAssertEqual(r.rmssdMilliseconds, 55)
        XCTAssertEqual(r.impedancePassCount, 6)
        // edfSessionID is in-memory only (UHDR boundary — not serialized). The in-memory
        // record carries it; after a reload from UserDefaults it will be nil.
        XCTAssertEqual(r.edfSessionID, 9999)
    }

    func testRecordWithNilOptionalFields() {
        let store = SessionHistoryStore(defaults: makeSuite(#function))
        store.record(makeSummary(coherence: nil, rmssd: nil, edfID: nil))

        let r = store.records[0]
        XCTAssertNil(r.averageCoherenceScore, "No-HRV session must store nil coherence")
        XCTAssertNil(r.rmssdMilliseconds)
        XCTAssertNil(r.edfSessionID)
    }

    // MARK: - ordering

    func testRecordsAreNewestFirst() {
        let suite = makeSuite(#function)
        let store = SessionHistoryStore(defaults: suite)
        let base = Date(timeIntervalSince1970: 1_700_000_000)

        store.record(makeSummary(protocolName: "A", completedAt: base))
        store.record(makeSummary(protocolName: "B", completedAt: base + 60))
        store.record(makeSummary(protocolName: "C", completedAt: base + 120))

        XCTAssertEqual(store.records.map(\.protocolName), ["C", "B", "A"])
    }

    // MARK: - trim

    func testTrimsToMaxOneHundredRecords() {
        let suite = makeSuite(#function)
        let store = SessionHistoryStore(defaults: suite)

        for i in 0..<110 {
            store.record(makeSummary(
                protocolName: "Session \(i)",
                completedAt: Date(timeIntervalSince1970: Double(i))
            ))
        }

        XCTAssertEqual(store.records.count, 100, "Store must not exceed 100 records")
    }

    func testTrimmingKeepsNewestRecords() {
        let suite = makeSuite(#function)
        let store = SessionHistoryStore(defaults: suite)
        let base = Date(timeIntervalSince1970: 1_000_000)

        for i in 0..<105 {
            store.record(makeSummary(
                protocolName: "S\(i)",
                completedAt: base + Double(i)
            ))
        }

        // Newest 100 are base+5 through base+104 (i=5..104).
        XCTAssertEqual(store.records.first?.protocolName, "S104", "Newest record must survive trim")
        XCTAssertEqual(store.records.last?.protocolName, "S5", "Oldest surviving record is S5")
        XCTAssertFalse(store.records.map(\.protocolName).contains("S0"), "Oldest 5 must be trimmed")
    }

    // MARK: - clearHistory

    func testClearHistoryEmptiesList() {
        let suite = makeSuite(#function)
        let store = SessionHistoryStore(defaults: suite)
        store.record(makeSummary())
        store.record(makeSummary())

        store.clearHistory()

        XCTAssertTrue(store.records.isEmpty)
    }

    func testClearHistoryPersistsClear() {
        let suite = makeSuite(#function)
        let store = SessionHistoryStore(defaults: suite)
        store.record(makeSummary())
        store.clearHistory()

        let reloaded = SessionHistoryStore(defaults: suite)
        XCTAssertTrue(reloaded.records.isEmpty, "Clear must survive a reload")
    }

    // MARK: - persistence

    func testRecordsSurviveReload() {
        let suite = makeSuite(#function)
        let store = SessionHistoryStore(defaults: suite)
        store.record(makeSummary(protocolName: "Sleep Deep", durationSeconds: 900))

        let reloaded = SessionHistoryStore(defaults: suite)
        XCTAssertEqual(reloaded.records.count, 1)
        XCTAssertEqual(reloaded.records.first?.protocolName, "Sleep Deep")
        XCTAssertEqual(reloaded.records.first?.durationSeconds ?? 0, 900, accuracy: 0.01)
    }

    func testNewestFirstOrderingSurvivesReload() {
        let suite = makeSuite(#function)
        let store = SessionHistoryStore(defaults: suite)
        let base = Date(timeIntervalSince1970: 1_700_000_000)
        store.record(makeSummary(protocolName: "A", completedAt: base))
        store.record(makeSummary(protocolName: "B", completedAt: base + 60))

        let reloaded = SessionHistoryStore(defaults: suite)
        XCTAssertEqual(reloaded.records.map(\.protocolName), ["B", "A"])
    }

    // MARK: - corrupt blob recovery

    func testCorruptBlobStartsCleanRatherThanCrashing() {
        let suite = makeSuite(#function)
        suite.set("this is not JSON".data(using: .utf8)!, forKey: "np.session.history")

        let store = SessionHistoryStore(defaults: suite)

        XCTAssertTrue(store.records.isEmpty, "Corrupt stored data must yield empty records, not a crash")
    }

    // MARK: - Privacy invariant

    func testPersistedBlobContainsNoCompletedAtKey() throws {
        let suite = makeSuite(#function)
        let store = SessionHistoryStore(defaults: suite)
        store.record(makeSummary())

        let data = try XCTUnwrap(suite.data(forKey: "np.session.history"))
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [[String: Any]])
        let row = try XCTUnwrap(json.first)

        XCTAssertNil(row["completedAt"], "Exact timestamp must not be persisted (UHDR boundary)")
        XCTAssertNotNil(row["sessionDay"], "sessionDay must be present in persisted blob")
        XCTAssertNotNil(row["insertionIndex"], "insertionIndex must be present in persisted blob")
    }

    func testPersistedBlobContainsNoEdfSessionID() throws {
        let suite = makeSuite(#function)
        let store = SessionHistoryStore(defaults: suite)
        store.record(makeSummary(edfID: 9999))

        let data = try XCTUnwrap(suite.data(forKey: "np.session.history"))
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [[String: Any]])
        let row  = try XCTUnwrap(json.first)

        // edfSessionID is a Unix-ms timestamp (UHDR-class) and must not be persisted
        // to the backed-up UserDefaults store. (PRIV-ISC30-02)
        XCTAssertNil(row["edfSessionID"], "edfSessionID (UHDR epoch) must not be serialized to UserDefaults")
        // In-memory record should still carry it for the live post-session window.
        XCTAssertEqual(store.records.first?.edfSessionID, 9999)
    }

    func testNewRecordsWritableAfterCorruptBlobRecovery() {
        let suite = makeSuite(#function)
        suite.set("bad data".data(using: .utf8)!, forKey: "np.session.history")

        let store = SessionHistoryStore(defaults: suite)
        store.record(makeSummary(protocolName: "Post-Corrupt"))

        XCTAssertEqual(store.records.count, 1)
        XCTAssertEqual(store.records.first?.protocolName, "Post-Corrupt")
    }
}
