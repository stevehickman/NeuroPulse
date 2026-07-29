//
//  HardwareSetupManagerTests.swift
//  NeurOneTests
//
//  Satisfies:
//    ISC-114  wizard covers BLE pairing, impedance check, protocol selection, safety acknowledgement
//    ISC-115  8 electrode positions color-coded from impedancePassFlags (UI — structural code path)
//    ISC-116  wizard blocks progression to protocol selection until ≥ 6/8 EEG contacts pass
//    ISC-117  isFirstSetupComplete only set after wizard's final step
//    ISC-118  safety acknowledgement step includes T1 contraindications checkbox
//    ISC-119  contraindications checkbox not pre-ticked, cannot be bypassed
//    ISC-120  Setup tab badge disappears once isFirstSetupComplete == true (MainTabView binding)
//    ISC-121  SessionView inaccessible (tab guarded) until isFirstSetupComplete
//
//  Testing strategy
//  ─────────────────
//  HardwareSetupManager accepts a SetupGATTProviding protocol, so tests use MockSetupGATT
//  without a real BLE stack. UserDefaults is injected as a suite so tests are isolated.
//  Async confirmation methods (confirmImpedanceCheck, confirmADS1299Calibration) are
//  exercised via awaited XCTest async functions.

import XCTest
import Combine
@testable import NeurOne

// MARK: - Mock GATT provider

/// Deterministic SetupGATTProviding for unit tests. Drives state without BLE hardware.
private final class MockSetupGATT: SetupGATTProviding {

    var mockConnectionState = NeurOneGATTManager.ConnectionState.disconnected
    var mockCalibrationResult: Result<Void, GATTWriteError> = .success(())
    /// TEST-ONLY: synthetic values, no user data.
    /// Impedance flags injected via dedicated impedanceSubject — not actual hardware readings.
    var impedanceFlagsToInject: UInt16 = 0xFF   // all 8 pass by default

    private let sessionSubject   = CurrentValueSubject<SessionState, Never>(.empty)
    private let zonesSubject     = CurrentValueSubject<ZoneModuleConfiguration, Never>(
        ZoneModuleConfiguration())
    private let zoneEventSubject = PassthroughSubject<ZoneModuleStatus, Never>()
    private let impedanceSubject = PassthroughSubject<UInt16, Never>()

    var connectionState: NeurOneGATTManager.ConnectionState { mockConnectionState }

    var sessionPublisher: AnyPublisher<SessionState, Never> {
        sessionSubject.eraseToAnyPublisher()
    }

    var zoneModulesPublisher: AnyPublisher<ZoneModuleConfiguration, Never> {
        zonesSubject.eraseToAnyPublisher()
    }

    var zoneModuleEventPublisher: AnyPublisher<ZoneModuleStatus, Never> {
        zoneEventSubject.eraseToAnyPublisher()
    }

    var impedanceResultPublisher: AnyPublisher<UInt16, Never> {
        impedanceSubject.eraseToAnyPublisher()
    }

    func sendCalibration(_ opcode: CalibrationOpcode,
                         completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        if opcode == .impedanceCheck {
            // Mirror production behavior: update session state AND fire dedicated subject.
            var next = sessionSubject.value
            next.impedancePassFlags = impedanceFlagsToInject
            sessionSubject.send(next)
            impedanceSubject.send(impedanceFlagsToInject)
        }
        completion(mockCalibrationResult)
    }

    func pushZones(_ configuration: ZoneModuleConfiguration) {
        zonesSubject.send(configuration)
    }

    /// Publish a socket map AND the per-socket events that produced it, the way
    /// NeurOneGATTManager does on a real frame.
    func pushZoneEvent(_ status: ZoneModuleStatus) {
        var next = zonesSubject.value
        next.apply(ZoneModuleFrame(isSnapshot: false, isLastFragment: true,
                                   fragmentIndex: 0, records: [status]))
        zonesSubject.send(next)
        zoneEventSubject.send(status)
    }
}

// MARK: - Spy announcer

/// Captures what the app would speak, without an audio session.
@MainActor
private final class SpyAnnouncer: ZoneModuleAnnouncing {
    var spoken: [String] = []
    var cancelCount = 0

    func announce(_ message: String) { spoken.append(message) }
    func cancel() { cancelCount += 1 }
}

// MARK: - Helpers

/// Build a socket status the way a decoded frame record would.
private func socketStatus(_ id: UInt8,
                          type: ZoneModuleType = .eeg,
                          lobe: ZoneLobe = .frontal,
                          side: ZoneSide = .left,
                          present: Bool = true,
                          fault: Bool = false) -> ZoneModuleStatus {
    ZoneModuleStatus(socketID: id, moduleType: type, lobe: lobe, side: side,
                     isPresent: present, hasFault: fault)
}

private func configuration(_ statuses: [ZoneModuleStatus]) -> ZoneModuleConfiguration {
    var c = ZoneModuleConfiguration()
    c.apply(ZoneModuleFrame(isSnapshot: true, isLastFragment: true,
                            fragmentIndex: 0, records: statuses))
    return c
}

/// Counts bits set in a UInt16 bitmask.
private func popCount(_ v: UInt16) -> Int {
    (0..<16).filter { v & (1 << $0) != 0 }.count
}

// MARK: - Tests

@MainActor
final class HardwareSetupManagerTests: XCTestCase {

    private var defaults: UserDefaults!

    override func setUp() {
        super.setUp()
        defaults = UserDefaults(suiteName: "HardwareSetupManagerTests-\(UUID().uuidString)")!
    }

    override func tearDown() {
        defaults.removeSuite(named: defaults.description)
        super.tearDown()
    }

    // MARK: ISC-114 — wizard step coverage

    func testBLEConfirmationStepExists() {
        XCTAssertTrue(SetupStep.allCases.contains(.bleConfirmation),
                      "Wizard must include a BLE pairing confirmation step (ISC-114 item 1)")
    }

    func testSafetyAcknowledgementStepExists() {
        XCTAssertTrue(SetupStep.allCases.contains(.safetyAcknowledgement),
                      "Wizard must include a safety acknowledgement step (ISC-114 item 4)")
    }

    func testProtocolSelectionStepExists() {
        XCTAssertTrue(SetupStep.allCases.contains(.protocolSelection),
                      "Wizard must include a first protocol selection step (ISC-114 item 3)")
    }

    func testImpedanceCheckStepExists() {
        XCTAssertTrue(SetupStep.allCases.contains(.impedanceCheck),
                      "Wizard must include an EEG impedance check step (ISC-114 item 2)")
    }

    // MARK: ISC-116 — impedance threshold

    func testMinimumImpedancePassCountDefaultIs6() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        XCTAssertEqual(mgr.minimumImpedancePassCount, 6)
    }

    func testSafetyAcknowledgedIsNeverPersistedToUserDefaults() {
        // PRIVACY: safetyAcknowledged must not be written to UserDefaults.
        // Persisting "accepted T1 contraindications" constitutes inferred health status
        // under GDPR Art. 9 and Illinois BIPA.
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        mgr.acknowledgeSafety()
        let all = defaults.dictionaryRepresentation()
        XCTAssertFalse(
            all.keys.contains { $0.contains("safety") || $0.contains("ack") },
            "safetyAcknowledged must never be persisted (GDPR Art. 9 / BIPA)"
        )
    }

    func testImpedanceCheckAdvancesWhen6of8Pass() async {
        let mock = MockSetupGATT()
        mock.mockConnectionState = .connected
        mock.impedanceFlagsToInject = 0x3F  // bits 0–5 set = 6 passes
        XCTAssertEqual(popCount(0x3F), 6, "precondition: 6 bits set")
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mgr.setStepForTesting(.impedanceCheck)

        await mgr.confirmImpedanceCheck()

        XCTAssertNil(mgr.lastError, "6/8 passes should clear the impedance step")
        XCTAssertEqual(mgr.currentStep, .ads1299Calibration, "should advance past impedanceCheck")
    }

    func testImpedanceCheckAdvancesWhenAll8Pass() async {
        let mock = MockSetupGATT()
        mock.mockConnectionState = .connected
        mock.impedanceFlagsToInject = 0xFF  // all 8 bits
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mgr.setStepForTesting(.impedanceCheck)

        await mgr.confirmImpedanceCheck()

        XCTAssertNil(mgr.lastError)
        XCTAssertEqual(mgr.currentStep, .ads1299Calibration)
    }

    func testImpedanceCheckFailsWhen5of8Pass() async {
        let mock = MockSetupGATT()
        mock.mockConnectionState = .connected
        mock.impedanceFlagsToInject = 0x1F  // bits 0–4 = 5 passes
        XCTAssertEqual(popCount(0x1F), 5, "precondition: 5 bits set")
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mgr.setStepForTesting(.impedanceCheck)

        await mgr.confirmImpedanceCheck()

        XCTAssertNotNil(mgr.lastError, "5/8 passes (below threshold of 6) should set an error")
        guard case .impedanceFailed(let els) = mgr.lastError else {
            XCTFail("Expected .impedanceFailed, got \(String(describing: mgr.lastError))"); return
        }
        XCTAssertEqual(els.count, 3, "3 electrodes should be reported as failed")
        XCTAssertEqual(mgr.currentStep, .impedanceCheck, "should not advance")
    }

    func testImpedanceCheckFailsWhenZeroPass() async {
        let mock = MockSetupGATT()
        mock.mockConnectionState = .connected
        mock.impedanceFlagsToInject = 0x00  // none pass
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mgr.setStepForTesting(.impedanceCheck)

        await mgr.confirmImpedanceCheck()

        guard case .impedanceFailed(let els) = mgr.lastError else {
            XCTFail("Expected .impedanceFailed"); return
        }
        XCTAssertEqual(els.count, 8)
        XCTAssertEqual(mgr.currentStep, .impedanceCheck)
    }

    func testImpedanceCheckFailsWhenNotConnected() async {
        let mock = MockSetupGATT()
        mock.mockConnectionState = .disconnected
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mgr.setStepForTesting(.impedanceCheck)

        await mgr.confirmImpedanceCheck()

        guard case .notConnected = mgr.lastError else {
            XCTFail("Expected .notConnected"); return
        }
    }

    // MARK: ISC-118 / ISC-119 — safety acknowledgement

    func testSafetyAcknowledgedDefaultsFalse() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        XCTAssertFalse(mgr.safetyAcknowledged,
                       "Safety checkbox must not be pre-ticked (ISC-119)")
    }

    func testAdvanceBlockedAtSafetyStepWithoutAcknowledgement() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        mgr.setStepForTesting(.safetyAcknowledgement)

        mgr.advance()

        XCTAssertEqual(mgr.currentStep, .safetyAcknowledgement,
                       "Advance must be blocked when safety not yet acknowledged (ISC-119)")
        guard case .safetyAcknowledgementRequired = mgr.lastError else {
            XCTFail("Expected .safetyAcknowledgementRequired"); return
        }
    }

    func testAdvanceAllowedAfterAcknowledgeSafety() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        mgr.setStepForTesting(.safetyAcknowledgement)

        mgr.acknowledgeSafety()
        mgr.advance()

        XCTAssertEqual(mgr.currentStep, .protocolSelection,
                       "After acknowledging safety, wizard should advance to protocolSelection")
    }

    func testAcknowledgeSafetySetsFlag() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        XCTAssertFalse(mgr.safetyAcknowledged)

        mgr.acknowledgeSafety()

        XCTAssertTrue(mgr.safetyAcknowledged)
    }

    func testSafetyAcknowledgedResetOnRetry() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        mgr.acknowledgeSafety()
        XCTAssertTrue(mgr.safetyAcknowledged)

        mgr.setStepForTesting(.safetyAcknowledgement)  // re-entering step resets ack

        XCTAssertFalse(mgr.safetyAcknowledged,
                       "Re-entering safety step must reset acknowledgement flag")
    }

    // MARK: ISC-114 — safety step property

    func testSafetyAcknowledgementStepRequiresSafetyAcknowledgement() {
        XCTAssertTrue(SetupStep.safetyAcknowledgement.requiresSafetyAcknowledgement)
    }

    func testOtherStepsDoNotRequireSafetyAcknowledgement() {
        let others = SetupStep.allCases.filter { $0 != .safetyAcknowledgement }
        XCTAssertTrue(others.allSatisfy { !$0.requiresSafetyAcknowledgement })
    }

    // MARK: BLE confirmation step

    func testBLEConfirmationRequiresHardwareConfirmation() {
        XCTAssertTrue(SetupStep.bleConfirmation.requiresHardwareConfirmation,
                      "bleConfirmation must use the hardware confirm button path")
    }

    func testBLEConfirmationAdvancesWhenConnected() {
        let mock = MockSetupGATT()
        mock.mockConnectionState = .connected
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mgr.setStepForTesting(.bleConfirmation)

        mgr.confirmBLEPairing()

        XCTAssertNil(mgr.lastError)
        XCTAssertEqual(mgr.currentStep, .boaDial,
                       "Confirmed BLE pairing should advance to boaDial")
    }

    func testBLEConfirmationFailsWhenNotConnected() {
        let mock = MockSetupGATT()
        mock.mockConnectionState = .disconnected
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mgr.setStepForTesting(.bleConfirmation)

        mgr.confirmBLEPairing()

        guard case .notConnected = mgr.lastError else {
            XCTFail("Expected .notConnected when hub not paired"); return
        }
        XCTAssertEqual(mgr.currentStep, .bleConfirmation, "Step must not advance without BLE")
    }

    // MARK: ISC-117 — isFirstSetupComplete gate

    func testIsFirstSetupCompleteIsFalseInitially() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        XCTAssertFalse(mgr.isFirstSetupComplete)
    }

    func testIsFirstSetupCompleteSetOnlyWhenReachingCompleteStep() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        mgr.acknowledgeSafety()
        mgr.setStepForTesting(.protocolSelection)
        XCTAssertFalse(mgr.isFirstSetupComplete, "Not complete until final advance")

        mgr.advance()

        XCTAssertTrue(mgr.isFirstSetupComplete,
                      "Advancing from protocolSelection to complete must set the flag (ISC-117)")
    }

    func testIsFirstSetupCompletePersistsAcrossInstances() {
        let mgr1 = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        mgr1.acknowledgeSafety()
        mgr1.setStepForTesting(.protocolSelection)
        mgr1.advance()
        XCTAssertTrue(defaults.bool(forKey: "np.setup.first-complete"))

        let mgr2 = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        XCTAssertTrue(mgr2.isFirstSetupComplete,
                      "Completion must persist across manager instantiations")
    }

    func testIsFirstSetupCompleteNotSetBeforeFinalStep() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        // Advance through every step before complete
        for step in SetupStep.allCases where step != .complete {
            XCTAssertFalse(mgr.isFirstSetupComplete,
                           "Must not be complete at step \(step)")
        }
    }

    // MARK: ISC-120 — Setup badge (structural — MainTabView binds to isFirstSetupComplete)

    func testSetupBadgeKeyIsDrivenByIsFirstSetupComplete() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        // Badge count is `setup.isFirstSetupComplete ? 0 : 1`
        XCTAssertFalse(mgr.isFirstSetupComplete)  // badge = 1 before complete

        mgr.acknowledgeSafety()
        mgr.setStepForTesting(.protocolSelection)
        mgr.advance()

        XCTAssertTrue(mgr.isFirstSetupComplete)   // badge = 0 after complete
    }

    // MARK: ISC-121 — Session tab guard (logic)

    func testSessionTabShouldBeGuardedWhenSetupIncomplete() {
        // This test confirms the manager exposes the flag MainTabView uses for the guard.
        // MainTabView's onChange(of: selectedTab) uses `setup.isFirstSetupComplete`.
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        XCTAssertFalse(mgr.isFirstSetupComplete,
                       "Session tab guard must fire: isFirstSetupComplete == false on new install")
    }

    func testSessionTabUnblockedAfterSetupComplete() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        mgr.acknowledgeSafety()
        mgr.setStepForTesting(.protocolSelection)
        mgr.advance()
        XCTAssertTrue(mgr.isFirstSetupComplete,
                      "Session tab guard must not fire after setup complete")
    }

    // MARK: Step step ordering — ISC-116 dependency

    func testProtocolSelectionComesAfterImpedanceCheckInStepOrder() {
        // ISC-116: "blocks progression to protocol selection until ≥ 6/8 contacts pass"
        // means impedanceCheck must precede protocolSelection in the step sequence.
        let steps = SetupStep.allCases
        let impIdx  = steps.firstIndex(of: .impedanceCheck)!
        let protoIdx = steps.firstIndex(of: .protocolSelection)!
        XCTAssertLessThan(impIdx, protoIdx,
                          "impedanceCheck must come before protocolSelection (ISC-116)")
    }

    func testBLEConfirmationComesBeforeImpedanceCheck() {
        let steps = SetupStep.allCases
        let bleIdx  = steps.firstIndex(of: .bleConfirmation)!
        let impIdx  = steps.firstIndex(of: .impedanceCheck)!
        XCTAssertLessThan(bleIdx, impIdx, "BLE confirmation should precede impedance check")
    }

    // MARK: Retry clears error

    func testRetryClearsLastError() {
        let mgr = HardwareSetupManager(gatt: MockSetupGATT(), userDefaults: defaults)
        mgr.setStepForTesting(.safetyAcknowledgement)
        mgr.advance()  // sets safetyAcknowledgementRequired error
        XCTAssertNotNil(mgr.lastError)

        mgr.retry()

        XCTAssertNil(mgr.lastError, "retry() must clear lastError")
    }

    // MARK: Zone module confirmation (socket-keyed)

    func testZoneModuleConfirmationAdvancesWithAnyPopulatedSockets() {
        // Tiles are type-agnostic and a build is a configuration choice, so the
        // app cannot know how many modules "should" be installed. Any populated,
        // fault-free set advances; per-protocol requirements are the firmware
        // placement check's job (NP-HEX-ZM-001 SW-1).
        let mock = MockSetupGATT()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mock.pushZones(configuration([
            socketStatus(12), socketStatus(47, lobe: .parietal, side: .midline),
            socketStatus(80, present: false),
        ]))
        mgr.setStepForTesting(.zoneModules)

        mgr.confirmZoneModules()

        XCTAssertNil(mgr.lastError)
        XCTAssertEqual(mgr.currentStep, .impedanceCheck)
    }

    func testZoneModuleConfirmationAdvancesWithMoreThanFiveSockets() {
        // The retired model could not represent a sixth socket at all.
        let mock = MockSetupGATT()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mock.pushZones(configuration((1...78).map { socketStatus(UInt8($0)) }))
        mgr.setStepForTesting(.zoneModules)

        mgr.confirmZoneModules()

        XCTAssertNil(mgr.lastError)
        XCTAssertEqual(mgr.zoneConfiguration.presentSockets.count, 78)
        XCTAssertEqual(mgr.currentStep, .impedanceCheck)
    }

    func testZoneModuleConfirmationFailsOnFaultedSocket() {
        let mock = MockSetupGATT()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mock.pushZones(configuration([
            socketStatus(12),
            socketStatus(31, present: false, fault: true),
        ]))
        mgr.setStepForTesting(.zoneModules)

        mgr.confirmZoneModules()

        guard case .zoneModulesMissing(let faulted) = mgr.lastError else {
            XCTFail("Expected .zoneModulesMissing"); return
        }
        XCTAssertEqual(faulted, [31])
        XCTAssertEqual(mgr.currentStep, .zoneModules, "a faulted socket blocks the step")
    }

    func testZoneModuleConfirmationFailsWhenNothingReported() {
        let mock = MockSetupGATT()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mgr.setStepForTesting(.zoneModules)

        mgr.confirmZoneModules()

        guard case .noZoneModulesDetected = mgr.lastError else {
            XCTFail("Expected .noZoneModulesDetected"); return
        }
    }

    // MARK: Spoken insertion confirmation
    //
    // The confirmation the headset's bone-conduction transducer could never
    // deliver: seating a module requires the helmet off the head, so the cue is
    // spoken by this device instead (see ZoneModuleAnnouncer).

    func testInsertionIsSpokenDuringZoneModuleStep() {
        let mock = MockSetupGATT()
        let spy = SpyAnnouncer()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults, announcer: spy)
        mgr.setStepForTesting(.zoneModules)

        mock.pushZoneEvent(socketStatus(12, type: .eeg, lobe: .frontal, side: .left))

        XCTAssertEqual(spy.spoken.count, 1, "an insertion during the step is spoken")
        let said = spy.spoken[0]
        XCTAssertTrue(said.contains("12"), "the spoken string names the socket: \(said)")
        XCTAssertTrue(said.localizedCaseInsensitiveContains("frontal"),
                      "the spoken string names the anatomy: \(said)")
    }

    func testInsertionIsNotSpokenOutsideZoneModuleStep() {
        // The same events arrive during a session, where narrating socket changes
        // would be noise rather than confirmation.
        let mock = MockSetupGATT()
        let spy = SpyAnnouncer()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults, announcer: spy)
        mgr.setStepForTesting(.impedanceCheck)

        mock.pushZoneEvent(socketStatus(12))

        XCTAssertTrue(spy.spoken.isEmpty)
    }

    func testRemovalIsNotSpoken() {
        // The user pulled it out; they know. The list already reflects it.
        let mock = MockSetupGATT()
        let spy = SpyAnnouncer()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults, announcer: spy)
        mgr.setStepForTesting(.zoneModules)

        mock.pushZoneEvent(socketStatus(12, present: false))

        XCTAssertTrue(spy.spoken.isEmpty)
    }

    func testEachInsertionSpokenOnceInSocketOrder() {
        let mock = MockSetupGATT()
        let spy = SpyAnnouncer()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults, announcer: spy)
        mgr.setStepForTesting(.zoneModules)

        mock.pushZoneEvent(socketStatus(12))
        mock.pushZoneEvent(socketStatus(47, lobe: .parietal, side: .right))
        mock.pushZoneEvent(socketStatus(80, lobe: .occipital, side: .midline))

        XCTAssertEqual(spy.spoken.count, 3, "one confirmation per insertion")
        XCTAssertTrue(spy.spoken[1].contains("47"))
        XCTAssertTrue(spy.spoken[2].contains("80"))
    }

    func testLeavingZoneStepCancelsSpeech() {
        // A confirmation must not trail into the next screen.
        let mock = MockSetupGATT()
        let spy = SpyAnnouncer()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults, announcer: spy)
        mock.pushZones(configuration([socketStatus(12)]))
        mgr.setStepForTesting(.zoneModules)

        mgr.confirmZoneModules()

        XCTAssertEqual(spy.cancelCount, 1)
    }

    func testUnidentifiedModuleIsSpokenWithoutClaimingAType() {
        // The retired ZONE_ID ladder reports position but cannot distinguish a
        // base tile from an electrode tile. The app must not invent one.
        let mock = MockSetupGATT()
        let spy = SpyAnnouncer()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults, announcer: spy)
        mgr.setStepForTesting(.zoneModules)

        mock.pushZoneEvent(socketStatus(12, type: .unknown))

        XCTAssertEqual(spy.spoken.count, 1)
        XCTAssertFalse(spy.spoken[0].localizedCaseInsensitiveContains("EEG"),
                       "no type may be claimed when the hub could not identify one")
    }

    // MARK: - ISC-115: impedanceFlags zeroed after advancing past impedanceCheck

    // SetupView colors 8 electrode position indicators from impedanceFlags.
    // Once the user advances past the impedanceCheck step, those flags must be
    // cleared so a stale "all-green" pattern from this session does not persist
    // into the next setup session. HardwareSetupManager zeros impedanceFlags in
    // the advanceStep() path when leaving .impedanceCheck.

    func testImpedanceFlagsClearedAfterAdvancingPastImpedanceCheck() async {
        let mock = MockSetupGATT()
        mock.mockConnectionState = .connected
        mock.impedanceFlagsToInject = 0xFF   // all 8 electrodes pass
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mgr.setStepForTesting(.impedanceCheck)

        await mgr.confirmImpedanceCheck()

        XCTAssertEqual(mgr.currentStep, .ads1299Calibration,
                       "precondition: must have advanced past impedanceCheck")
        XCTAssertEqual(mgr.impedanceFlags, 0,
                       "impedanceFlags must be zeroed after leaving impedanceCheck (ISC-115)")
    }
}
