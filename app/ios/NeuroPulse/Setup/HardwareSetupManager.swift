import Foundation
import Combine
import UIKit

// First-session hardware setup coordinator.
// Guides the user through: BLE pairing → EEG impedance check → ADS1299 calibration →
// zone module identification → Boa dial / electrode pod adjustment → safety acknowledgement →
// first protocol selection.
// All hardware-gated steps require hub confirmation via GATT before the wizard advances.

// MARK: - GATT abstraction for testability

@MainActor
protocol SetupGATTProviding {
    var connectionState: NeuroPulseGATTManager.ConnectionState { get }
    var sessionPublisher: AnyPublisher<SessionState, Never> { get }
    var zoneModulesPublisher: AnyPublisher<[UInt8], Never> { get }
    /// Fires exactly once per impedance check, carrying the raw pass-flags bitmask.
    /// Backed by a dedicated PassthroughSubject in production — distinct from sessionPublisher
    /// so waitForImpedanceResult cannot false-trigger on unrelated session updates.
    var impedanceResultPublisher: AnyPublisher<UInt16, Never> { get }
    func sendCalibration(_ opcode: CalibrationOpcode,
                         completion: @escaping (Result<Void, GATTWriteError>) -> Void)
}

// MARK: - Setup step definitions

enum SetupStep: Int, CaseIterable, Identifiable {
    case welcome             = 0
    case bleConfirmation     = 1   // Hub BLE pairing confirmation (ISC-114)
    case boaDial             = 2   // Fit adjustment — 52–62cm head range
    case electrodePods       = 3   // Spring-decoupled pods, 80–120g contact force
    case zoneModules         = 4   // Zone module insertion + bone conduction confirmation
    case impedanceCheck      = 5   // ADS1299 electrode impedance < 5 kΩ target
    case ads1299Calibration  = 6   // ADS1299 internal reference self-calibration
    case hydrationCaps       = 7   // Moisture-barrier hydration cap removal before use
    case safetyAcknowledgement = 8 // T1 contraindications checkbox (ISC-118/119)
    case protocolSelection   = 9   // First protocol selection (ISC-114)
    case complete            = 10

    var id: Int { rawValue }

    var title: String {
        switch self {
        case .welcome:               return String(localized: "SETUP_TITLE_WELCOME")
        case .bleConfirmation:       return String(localized: "SETUP_TITLE_BLE")
        case .boaDial:               return String(localized: "SETUP_TITLE_BOA_DIAL")
        case .electrodePods:         return String(localized: "SETUP_TITLE_ELECTRODE_PODS")
        case .zoneModules:           return String(localized: "SETUP_TITLE_ZONE_MODULES")
        case .impedanceCheck:        return String(localized: "SETUP_TITLE_IMPEDANCE")
        case .ads1299Calibration:    return String(localized: "SETUP_TITLE_CALIBRATION")
        case .hydrationCaps:         return String(localized: "SETUP_TITLE_HYDRATION")
        case .safetyAcknowledgement: return String(localized: "SETUP_TITLE_SAFETY")
        case .protocolSelection:     return String(localized: "SETUP_TITLE_PROTOCOL")
        case .complete:              return String(localized: "SETUP_TITLE_COMPLETE")
        }
    }

    var instruction: String {
        switch self {
        case .welcome:               return String(localized: "SETUP_INSTR_WELCOME")
        case .bleConfirmation:       return String(localized: "SETUP_INSTR_BLE")
        case .boaDial:               return String(localized: "SETUP_INSTR_BOA_DIAL")
        case .electrodePods:         return String(localized: "SETUP_INSTR_ELECTRODE_PODS")
        case .zoneModules:           return String(localized: "SETUP_INSTR_ZONE_MODULES")
        case .impedanceCheck:        return String(localized: "SETUP_INSTR_IMPEDANCE")
        case .ads1299Calibration:    return String(localized: "SETUP_INSTR_CALIBRATION")
        case .hydrationCaps:         return String(localized: "SETUP_INSTR_HYDRATION")
        case .safetyAcknowledgement: return String(localized: "SETUP_INSTR_SAFETY")
        case .protocolSelection:     return String(localized: "SETUP_INSTR_PROTOCOL")
        case .complete:              return String(localized: "SETUP_INSTR_COMPLETE")
        }
    }

    var requiresHardwareConfirmation: Bool {
        switch self {
        case .bleConfirmation, .zoneModules, .impedanceCheck, .ads1299Calibration: return true
        default: return false
        }
    }

    var requiresSafetyAcknowledgement: Bool {
        self == .safetyAcknowledgement
    }
}

// MARK: - Setup errors

enum SetupError: LocalizedError {
    case notConnected
    case calibrationFailed
    case impedanceFailed(failedElectrodes: [Int])
    case zoneModulesMissing([Int])
    case timeout
    case safetyAcknowledgementRequired

    var errorDescription: String? {
        switch self {
        case .notConnected:
            return String(localized: "SETUP_ERROR_NOT_CONNECTED")
        case .calibrationFailed:
            return String(localized: "SETUP_ERROR_CALIBRATION_FAILED")
        case .impedanceFailed(let els):
            let names = els.map { String(format: String(localized: "SETUP_ELECTRODE_LABEL"), $0 + 1) }.joined(separator: ", ")
            return String(format: String(localized: "SETUP_ERROR_IMPEDANCE_FAILED"), names)
        case .zoneModulesMissing(let slots):
            let names = slots.map { String(format: String(localized: "SETUP_SLOT_LABEL"), $0 + 1) }.joined(separator: ", ")
            return String(format: String(localized: "SETUP_ERROR_ZONE_MISSING"), names)
        case .timeout:
            return String(localized: "SETUP_ERROR_TIMEOUT")
        case .safetyAcknowledgementRequired:
            return String(localized: "SETUP_ERROR_SAFETY_ACK_REQUIRED")
        }
    }
}

// MARK: - Hardware Setup Manager

@MainActor
final class HardwareSetupManager: ObservableObject {

    @Published private(set) var currentStep: SetupStep = .welcome
    @Published private(set) var isProcessing = false
    @Published private(set) var lastError: SetupError?
    @Published private(set) var impedanceFlags: UInt16 = 0     // bitmask from GATT
    @Published private(set) var zoneConfiguration = ZoneModuleConfiguration()
    @Published private(set) var isFirstSetupComplete = false

    // Separate signal for wait logic — set whenever ANY impedance update arrives from the hub,
    // including flags = 0x00 (all fail). removeDuplicates() on impedanceFlags would suppress 0→0
    // transitions, so this flag is the reliable "hub responded" indicator.
    private var impedanceResultReceived = false

    // ISC-118/119: safety acknowledgement checkbox — not pre-ticked, cannot be bypassed.
    // PRIVACY: must never be persisted — constitutes inferred health status (GDPR Art. 9, BIPA).
    @Published private(set) var safetyAcknowledged = false

    /// Minimum number of electrodes (out of 8) that must pass impedance check (ISC-116).
    /// Hard-coded to 6 — this is a safety threshold and must not be configurable at runtime.
    let minimumImpedancePassCount = 6

    private let gatt: SetupGATTProviding
    private let userDefaults: UserDefaults
    private var cancellables = Set<AnyCancellable>()

    private let firstSetupKey = "np.setup.first-complete"
    private let expectedElectrodeCount = 8  // 8-ch semi-dry EEG

    init(gatt: SetupGATTProviding, userDefaults: UserDefaults = .standard) {
        self.gatt = gatt
        self.userDefaults = userDefaults
        isFirstSetupComplete = userDefaults.bool(forKey: "np.setup.first-complete")
        observeGATT()
    }

    // MARK: - Step navigation

    func advance() {
        if currentStep == .safetyAcknowledgement && !safetyAcknowledged {
            lastError = .safetyAcknowledgementRequired
            return
        }
        guard let next = SetupStep(rawValue: currentStep.rawValue + 1) else { return }
        // Zero out impedance flags when leaving the check step — UHDR-class data should not
        // linger in memory beyond the step that requires it.
        if currentStep == .impedanceCheck { impedanceFlags = 0 }
        currentStep = next
        lastError = nil
        if currentStep == .complete { markFirstSetupComplete() }
    }

    func retry() {
        lastError = nil
        isProcessing = false
    }

    /// Direct step override for testing. Resets safetyAcknowledged when entering .safetyAcknowledgement.
    /// Wrapped in #if DEBUG so the production binary cannot bypass wizard safety gates.
    #if DEBUG
    func setStepForTesting(_ step: SetupStep) {
        currentStep = step
        if step == .safetyAcknowledgement {
            safetyAcknowledged = false
        }
    }
    #endif

    // MARK: - Safety acknowledgement (ISC-118/119)

    func acknowledgeSafety() {
        safetyAcknowledged = true
    }

    // MARK: - Hardware confirmation steps

    func confirmBLEPairing() {
        guard gatt.connectionState == .connected else {
            lastError = .notConnected; return
        }
        lastError = nil
        advance()
    }

    func confirmImpedanceCheck() async {
        guard gatt.connectionState == .connected else {
            lastError = .notConnected; return
        }
        isProcessing = true
        lastError = nil
        // Reset flags + received sentinel before triggering so wait detects the hub's fresh response.
        impedanceFlags = 0
        impedanceResultReceived = false
        defer { isProcessing = false }

        do {
            try await triggerCalibration(.impedanceCheck)
            try await waitForImpedanceResult(timeout: 10)

            let passCount = (0..<expectedElectrodeCount)
                .filter { bit in impedanceFlags & (1 << bit) != 0 }
                .count

            if passCount >= minimumImpedancePassCount {
                advance()
            } else {
                let failedElectrodes = (0..<expectedElectrodeCount)
                    .filter { bit in impedanceFlags & (1 << bit) == 0 }
                lastError = .impedanceFailed(failedElectrodes: failedElectrodes)
            }
        } catch {
            lastError = error as? SetupError ?? .timeout
        }
    }

    func confirmADS1299Calibration() async {
        guard gatt.connectionState == .connected else {
            lastError = .notConnected; return
        }
        isProcessing = true
        lastError = nil
        defer { isProcessing = false }

        do {
            try await triggerCalibration(.ads1299SelfCal)
            try await Task.sleep(nanoseconds: 3_000_000_000)  // 3s for calibration to complete
            advance()
        } catch {
            lastError = .calibrationFailed
        }
    }

    func confirmZoneModules() {
        let missingSlots = zoneConfiguration.slots.enumerated()
            .filter { !$0.element.isPresent }
            .map(\.offset)
        if missingSlots.isEmpty {
            advance()
        } else {
            lastError = .zoneModulesMissing(missingSlots)
        }
    }

    // MARK: - GATT observation

    private func observeGATT() {
        // UI update chain — removeDuplicates avoids spurious redraws when flags haven't changed.
        gatt.sessionPublisher
            .map(\.impedancePassFlags)
            .removeDuplicates()
            .assign(to: \.impedanceFlags, on: self)
            .store(in: &cancellables)

        // Wait-logic chain — uses the dedicated impedanceResultPublisher that fires ONLY on
        // a real NPUUID.impedanceResult GATT notification. Avoids false-triggers from other
        // session state updates (HRV coherence, pacer phase, etc.) that also flow via sessionPublisher.
        gatt.impedanceResultPublisher
            .sink { [weak self] _ in self?.impedanceResultReceived = true }
            .store(in: &cancellables)

        gatt.zoneModulesPublisher
            .sink { [weak self] rawSlots in
                self?.zoneConfiguration.update(rawSlotData: rawSlots)
            }
            .store(in: &cancellables)
    }

    private func triggerCalibration(_ opcode: CalibrationOpcode) async throws {
        try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Void, Error>) in
            gatt.sendCalibration(opcode) { result in
                switch result {
                case .success:  cont.resume()
                case .failure:  cont.resume(throwing: SetupError.timeout)
                }
            }
        }
    }

    /// Wait until the hub sends an impedance result (even flags = 0x00 meaning all fail).
    /// Uses `impedanceResultReceived` rather than `impedanceFlags != 0` because removeDuplicates
    /// on the UI chain suppresses 0→0 transitions when all electrodes fail.
    private func waitForImpedanceResult(timeout: TimeInterval) async throws {
        let deadline = Date().addingTimeInterval(timeout)
        while !impedanceResultReceived {
            guard Date() < deadline else { throw SetupError.timeout }
            try await Task.sleep(nanoseconds: 200_000_000)
        }
    }

    private func markFirstSetupComplete() {
        isFirstSetupComplete = true
        userDefaults.set(true, forKey: firstSetupKey)
    }
}

