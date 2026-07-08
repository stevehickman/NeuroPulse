//
//  PhoneSessionManagerTests.swift
//  NeurOneTests
//
//  Satisfies:
//    ISC-122  PhoneSessionManager establishes a WCSession when isSupported (static analysis)
//    ISC-123  Session state is forwarded to Watch when reachable
//    ISC-124  Watch connectivity unavailable is handled gracefully (isReachable=false)
//    ISC-125  Anti: forwarded WC messages contain no UHDR-class fields
//             (no epoch, no impedancePassFlags, no raw EEG/HRV time-series)
//
//  Subject under test: PhoneSessionManager + GATTSessionPublishing + WatchMessageSending
//  (app/ios/NeurOne/WatchBridge/PhoneSessionManager.swift,
//   app/ios/NeurOne/WatchBridge/GATTSessionPublishing.swift)
//
//  Privacy note: these tests verify the UHDR/WatchConnectivity boundary.
//  The WC bridge sends only non-UHDR display fields — epoch and impedancePassFlags
//  are UHDR-class and must never appear in a WC message dictionary.
//  See CLAUDE.md §5.1, NP-PRIV-ANALYSIS-002 MEDIUM-08.

import XCTest
import Combine
import WatchConnectivity
@testable import NeurOne
import NeurOneShared

// MARK: - MockGATTSessionPublisher

/// CurrentValueSubject-backed publisher. Uses CurrentValueSubject so that
/// `.dropFirst()` in PhoneSessionManager.observeGATT() discards the initial
/// `.empty` emission at subscription time — matching production behavior
/// where @Published emits the current value immediately.
private final class MockGATTSessionPublisher: GATTSessionPublishing {

    private let subject: CurrentValueSubject<SessionState, Never>

    init(initial: SessionState = .empty) {
        self.subject = CurrentValueSubject(initial)
    }

    var sessionPublisher: AnyPublisher<SessionState, Never> {
        subject.eraseToAnyPublisher()
    }

    func send(_ state: SessionState) {
        subject.send(state)
    }
}

// MARK: - MockWatchMessageSender

/// Captures outbound sendMessage calls and exposes isReachable as a settable
/// property. Provides deterministic, synchronous test control over the WC bridge.
private final class MockWatchMessageSender: WatchMessageSending {

    var isReachable: Bool
    private(set) var capturedMessages: [[String: Any]] = []
    private(set) var sendCallCount: Int = 0

    init(reachable: Bool = true) {
        self.isReachable = reachable
    }

    func sendMessage(_ message: [String: Any],
                     replyHandler: (([String: Any]) -> Void)?,
                     errorHandler: ((Error) -> Void)?) {
        sendCallCount += 1
        capturedMessages.append(message)
    }
}

// MARK: - PhoneSessionManagerTests

@MainActor
final class PhoneSessionManagerTests: XCTestCase {

    // MARK: - ISC-122 Static source analysis: WCSession.isSupported() guard present

    // PhoneSessionManager must check WCSession.isSupported() before calling
    // WCSession.default.activate() to avoid crashing on devices without Watch support
    // (e.g. iPad). This is also why tests can inject a mock sender without activating
    // a real WCSession — the guard is false in iOS Simulator.

    func testWCSessionIsSupportedCheckInSource() throws {
        let source = try Self.phoneSessionManagerSource()

        XCTAssertTrue(
            source.contains("WCSession.isSupported()"),
            "PhoneSessionManager.swift must check WCSession.isSupported() before " +
            "activating a WCSession — required for iPad compatibility (ISC-122)."
        )
        XCTAssertTrue(
            source.contains("WCSession.default.activate()"),
            "PhoneSessionManager.swift must call WCSession.default.activate() " +
            "inside the isSupported() guard block (ISC-122)."
        )
    }

    // The WatchMessageSending protocol abstraction must be present so tests can
    // inject a mock sender without activating real WC infrastructure.
    func testWatchMessageSendingProtocolInSource() throws {
        let source = try Self.phoneSessionManagerSource()

        XCTAssertTrue(
            source.contains("protocol WatchMessageSending"),
            "PhoneSessionManager.swift must define the WatchMessageSending protocol " +
            "for test injection (ISC-122 / testability)."
        )
        XCTAssertTrue(
            source.contains("extension WCSession: WatchMessageSending"),
            "WCSession must conform to WatchMessageSending via extension in PhoneSessionManager.swift."
        )
    }

    // MARK: - ISC-123 Session state is forwarded to Watch when reachable

    func testSessionStateForwardedToWatchWhenReachable() {
        let mockGATT = MockGATTSessionPublisher()
        let mockSender = MockWatchMessageSender(reachable: true)
        let sut = PhoneSessionManager(gatt: mockGATT, watchSender: mockSender)

        // The initial .empty value is dropped by .dropFirst() on subscription.
        // Sending a non-empty state should trigger forward() → sendMessage.
        let runningState = SessionState(
            epoch: 12345,
            protocolID: 3,
            status: .running,
            hrv: HRVData(coherenceScore: 7.5, rmssdMilliseconds: 42),
            pacerPhase: .exhale,
            pacerElapsedPercent: 60,
            impedancePassFlags: 0xFF,
            consumableSessionCounts: [5, 10, 2, 0]
        )
        mockGATT.send(runningState)

        XCTAssertEqual(
            mockSender.sendCallCount, 1,
            "One session state update should produce exactly one sendMessage call (ISC-123)."
        )
        guard let msg = mockSender.capturedMessages.first else {
            XCTFail("Expected one captured WC message after sending session state.")
            return
        }

        // Verify correct values are forwarded.
        XCTAssertEqual(msg[WCKey.protocolID] as? Int, 3, "Protocol ID must match.")
        XCTAssertEqual(msg[WCKey.status] as? Int, Int(SessionStatus.running.rawValue), "Status must match.")
        XCTAssertEqual(msg[WCKey.pacerPhase] as? Int, Int(PacerPhase.exhale.rawValue), "Pacer phase must match.")
        XCTAssertEqual(msg[WCKey.pacerPercent] as? Int, 60, "Pacer percent must match.")
        XCTAssertEqual(msg[WCKey.coherenceX100] as? Int, 750, "Coherence × 100 must be 7.5 × 100 = 750.")
        XCTAssertEqual(msg[WCKey.rmssd] as? Int, 42, "RMSSD must match.")
        withExtendedLifetime(sut) {}
    }

    func testMultipleStateUpdatesSendMultipleMessages() {
        let mockGATT = MockGATTSessionPublisher()
        let mockSender = MockWatchMessageSender(reachable: true)
        let sut = PhoneSessionManager(gatt: mockGATT, watchSender: mockSender)

        // Send 3 state updates after the initial-value drop.
        for i in 0..<3 {
            mockGATT.send(SessionState(
                epoch: UInt32(i),
                protocolID: UInt8(i),
                status: .running,
                hrv: nil,
                pacerPhase: .inhale,
                pacerElapsedPercent: UInt8(i * 10),
                impedancePassFlags: 0,
                consumableSessionCounts: [0, 0, 0, 0]
            ))
        }
        XCTAssertEqual(mockSender.sendCallCount, 3,
                       "Three state updates must produce exactly three sendMessage calls.")
        withExtendedLifetime(sut) {}
    }

    // MARK: - ISC-124 Graceful handling when Watch is not reachable

    func testNoMessageSentWhenWatchNotReachable() {
        let mockGATT = MockGATTSessionPublisher()
        let mockSender = MockWatchMessageSender(reachable: false)
        let _ = PhoneSessionManager(gatt: mockGATT, watchSender: mockSender)

        mockGATT.send(SessionState(
            epoch: 1,
            protocolID: 1,
            status: .running,
            hrv: nil,
            pacerPhase: .inhale,
            pacerElapsedPercent: 0,
            impedancePassFlags: 0,
            consumableSessionCounts: [0, 0, 0, 0]
        ))

        XCTAssertEqual(
            mockSender.sendCallCount, 0,
            "PhoneSessionManager must not call sendMessage when isReachable is false (ISC-124)."
        )
        XCTAssertTrue(
            mockSender.capturedMessages.isEmpty,
            "No WC messages must be sent when the Watch is not reachable (ISC-124)."
        )
    }

    func testConsumedLowNotificationNotSentWhenWatchNotReachable() {
        let mockGATT = MockGATTSessionPublisher()
        let mockSender = MockWatchMessageSender(reachable: false)
        let manager = PhoneSessionManager(gatt: mockGATT, watchSender: mockSender)

        manager.sendConsumableLowNotification(consumableIndex: 0, sessionsRemaining: 3)

        XCTAssertEqual(
            mockSender.sendCallCount, 0,
            "sendConsumableLowNotification must not call sendMessage when not reachable (ISC-124)."
        )
    }

    func testConsumableLowNotificationSentWhenReachable() {
        let mockGATT = MockGATTSessionPublisher()
        let mockSender = MockWatchMessageSender(reachable: true)
        let manager = PhoneSessionManager(gatt: mockGATT, watchSender: mockSender)

        manager.sendConsumableLowNotification(consumableIndex: 2, sessionsRemaining: 7)

        XCTAssertEqual(mockSender.sendCallCount, 1,
                       "Consumable low notification should be sent when Watch is reachable.")
        if let msg = mockSender.capturedMessages.first {
            XCTAssertEqual(msg["alert"] as? String, "consumable_low",
                           "Consumable low message must contain 'alert': 'consumable_low'.")
            XCTAssertEqual(msg["index"] as? Int, 2,
                           "Consumable low message must carry the correct consumableIndex.")
            XCTAssertEqual(msg["remaining"] as? Int, 7,
                           "Consumable low message must carry the correct sessionsRemaining.")
        }
    }

    // MARK: - ISC-125 Anti: no UHDR-class fields in forwarded WC messages

    // epoch and impedancePassFlags are UHDR-class (session timestamp / per-electrode
    // impedance bitmask). They must never appear in a WC message dictionary.
    // See CLAUDE.md §5.1, NP-PRIV-ANALYSIS-002 MEDIUM-08.

    func testForwardedMessageContainsNoEpoch() {
        let mockGATT = MockGATTSessionPublisher()
        let mockSender = MockWatchMessageSender(reachable: true)
        let sut = PhoneSessionManager(gatt: mockGATT, watchSender: mockSender)

        mockGATT.send(SessionState(
            epoch: 99999,          // non-zero epoch — must NOT appear in WC message
            protocolID: 1,
            status: .running,
            hrv: nil,
            pacerPhase: .inhale,
            pacerElapsedPercent: 0,
            impedancePassFlags: 0xFFFF,
            consumableSessionCounts: [0, 0, 0, 0]
        ))

        guard let msg = mockSender.capturedMessages.first else {
            XCTFail("Expected one captured WC message.")
            return
        }

        // epoch must not appear under any key.
        let uhdrForbiddenKeys = ["epoch", "ts", "timestamp", "session_ts", "start"]
        for key in uhdrForbiddenKeys {
            XCTAssertNil(
                msg[key],
                "WC message must not contain UHDR-class key '\(key)' — " +
                "epoch is a session timestamp (CLAUDE.md §5.1 / NP-PRIV-ANALYSIS-002 MEDIUM-08)."
            )
        }
        withExtendedLifetime(sut) {}
    }

    func testForwardedMessageContainsNoImpedancePassFlags() {
        let mockGATT = MockGATTSessionPublisher()
        let mockSender = MockWatchMessageSender(reachable: true)
        let sut = PhoneSessionManager(gatt: mockGATT, watchSender: mockSender)

        mockGATT.send(SessionState(
            epoch: 0,
            protocolID: 1,
            status: .running,
            hrv: nil,
            pacerPhase: .inhale,
            pacerElapsedPercent: 0,
            impedancePassFlags: 0xFF,  // non-zero flags — must NOT appear in WC message
            consumableSessionCounts: [0, 0, 0, 0]
        ))

        guard let msg = mockSender.capturedMessages.first else {
            XCTFail("Expected one captured WC message.")
            return
        }

        // Per-electrode impedance bitmask is UHDR-class.
        let uhdrForbiddenKeys = ["imp", "impedance", "impedance_flags", "pass_flags", "elec"]
        for key in uhdrForbiddenKeys {
            XCTAssertNil(
                msg[key],
                "WC message must not contain UHDR-class key '\(key)' — " +
                "impedancePassFlags is a per-electrode health bitmask (CLAUDE.md §5.1)."
            )
        }
        withExtendedLifetime(sut) {}
    }

    func testForwardedMessageOnlyContainsExpectedWCKeys() {
        let mockGATT = MockGATTSessionPublisher()
        let mockSender = MockWatchMessageSender(reachable: true)
        let sut = PhoneSessionManager(gatt: mockGATT, watchSender: mockSender)

        let stateWithHRV = SessionState(
            epoch: 1000,
            protocolID: 5,
            status: .running,
            hrv: HRVData(coherenceScore: 6.0, rmssdMilliseconds: 38),
            pacerPhase: .inhale,
            pacerElapsedPercent: 25,
            impedancePassFlags: 0xABCD,
            consumableSessionCounts: [1, 2, 3, 4]
        )
        mockGATT.send(stateWithHRV)

        guard let msg = mockSender.capturedMessages.first else {
            XCTFail("Expected one captured WC message.")
            return
        }

        // The complete allowed key set for WC messages (SessionState.toWCMessage()).
        let allowedKeys: Set<String> = [
            WCKey.protocolID,
            WCKey.status,
            WCKey.pacerPhase,
            WCKey.pacerPercent,
            WCKey.consumableCounts,
            WCKey.coherenceX100,
            WCKey.rmssd,
        ]
        for key in msg.keys {
            XCTAssertTrue(
                allowedKeys.contains(key),
                "WC message contains unexpected key '\(key)' — only declared WCKey values " +
                "are allowed in the bridge payload (ISC-125)."
            )
        }
        withExtendedLifetime(sut) {}
    }

    // MARK: - ISC-125 Static source analysis: no UHDR field names in forward path

    func testNoEpochReferenceInForwardPathSource() throws {
        let source = try Self.phoneSessionManagerSource()

        // The forward() method must not reference "epoch" directly.
        // toWCMessage() in NeurOneShared handles the exclusion; we verify
        // the bridge layer doesn't re-introduce it.
        let forwardSection = source
            .components(separatedBy: "private func forward(")
            .dropFirst().first ?? ""

        XCTAssertFalse(
            forwardSection.contains(".epoch"),
            "forward() in PhoneSessionManager.swift must not access .epoch — " +
            "epoch is UHDR-class (NP-PRIV-ANALYSIS-002 MEDIUM-08)."
        )
        XCTAssertFalse(
            forwardSection.contains(".impedancePassFlags"),
            "forward() in PhoneSessionManager.swift must not access .impedancePassFlags — " +
            "impedance bitmask is UHDR-class (CLAUDE.md §5.1)."
        )
    }

    // MARK: - Source file helper

    private static func phoneSessionManagerSource(file: StaticString = #filePath) throws -> String {
        let testFileURL = URL(fileURLWithPath: "\(file)")
        let iosDir = testFileURL
            .deletingLastPathComponent()   // NeurOneTests/
            .deletingLastPathComponent()   // ios/
        let sourceURL = iosDir
            .appendingPathComponent("NeurOne")
            .appendingPathComponent("WatchBridge")
            .appendingPathComponent("PhoneSessionManager.swift")
        return try String(contentsOf: sourceURL, encoding: .utf8)
    }
}
