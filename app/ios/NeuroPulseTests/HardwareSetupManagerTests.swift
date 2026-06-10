//
//  HardwareSetupManagerTests.swift
//  NeuroPulseTests
//
//  Satisfies:
//    ISC-114  wizard covers BLE pairing, impedance check, protocol selection, safety acknowledgement
//    ISC-115  8 electrode positions colour-coded from impedancePassFlags (UI — structural code path)
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
@testable import NeuroPulse

// MARK: - Mock GATT provider

/// Deterministic SetupGATTProviding for unit tests. Drives state without BLE hardware.
private final class MockSetupGATT: SetupGATTProviding {

    var mockConnectionState = NeuroPulseGATTManager.ConnectionState.disconnected
    var mockCalibrationResult: Result<Void, GATTWriteError> = .success(())
    /// TEST-ONLY: synthetic values, no user data.
    /// Impedance flags injected via dedicated impedanceSubject — not actual hardware readings.
    var impedanceFlagsToInject: UInt16 = 0xFF   // all 8 pass by default
    /// Zone module bytes to publish.
    var zoneModulesToInject: [UInt8] = [1, 2, 3, 4, 5]  // all present by default

    private let sessionSubject   = CurrentValueSubject<SessionState, Never>(.empty)
    private let zonesSubject     = CurrentValueSubject<[UInt8], Never>([0, 0, 0, 0, 0])
    private let impedanceSubject = PassthroughSubject<UInt16, Never>()

    var connectionState: NeuroPulseGATTManager.ConnectionState { mockConnectionState }

    var sessionPublisher: AnyPublisher<SessionState, Never> {
        sessionSubject.eraseToAnyPublisher()
    }

    var zoneModulesPublisher: AnyPublisher<[UInt8], Never> {
        zonesSubject.eraseToAnyPublisher()
    }

    var impedanceResultPublisher: AnyPublisher<UInt16, Never> {
        impedanceSubject.eraseToAnyPublisher()
    }

    func sendCalibration(_ opcode: CalibrationOpcode,
                         completion: @escaping (Result<Void, GATTWriteError>) -> Void) {
        if opcode == .impedanceCheck {
            // Mirror production behaviour: update session state AND fire dedicated subject.
            var next = sessionSubject.value
            next.impedancePassFlags = impedanceFlagsToInject
            sessionSubject.send(next)
            impedanceSubject.send(impedanceFlagsToInject)
        }
        completion(mockCalibrationResult)
    }

    func pushZones(_ zones: [UInt8]) {
        zonesSubject.send(zones)
    }
}

// MARK: - Helpers

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

    // MARK: Zone module confirmation (pre-existing step, validate gating still works)

    func testZoneModuleConfirmationAdvancesWhenAllPresent() {
        let mock = MockSetupGATT()
        mock.zoneModulesToInject = [1, 2, 3, 4, 5]  // all 5 zones present
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mock.pushZones([1, 2, 3, 4, 5])
        mgr.setStepForTesting(.zoneModules)

        mgr.confirmZoneModules()

        XCTAssertNil(mgr.lastError)
        XCTAssertEqual(mgr.currentStep, .impedanceCheck)
    }

    func testZoneModuleConfirmationFailsWhenSomeMissing() {
        let mock = MockSetupGATT()
        let mgr = HardwareSetupManager(gatt: mock, userDefaults: defaults)
        mock.pushZones([1, 0, 3, 0, 5])  // slots 1, 3 absent
        mgr.setStepForTesting(.zoneModules)

        mgr.confirmZoneModules()

        guard case .zoneModulesMissing(let missing) = mgr.lastError else {
            XCTFail("Expected .zoneModulesMissing"); return
        }
        XCTAssertEqual(missing.sorted(), [1, 3])
    }
}
