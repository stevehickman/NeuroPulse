import Foundation
import Combine
import UIKit

// First-session hardware setup coordinator.
// Guides the user through: EEG impedance check → ADS1299 self-calibration →
// zone module identification → Boa dial and electrode pod adjustment.
// All steps are measurement-triggered: the app does not advance until the hub
// confirms the hardware condition is within spec.

enum SetupStep: Int, CaseIterable, Identifiable {
    case welcome            = 0
    case boaDial            = 1   // Fit adjustment — 52–62cm head range
    case electrodePods      = 2   // Spring-decoupled pods, 80–120g contact force
    case zoneModules        = 3   // Zone module insertion + bone conduction confirmation
    case impedanceCheck     = 4   // ADS1299 electrode impedance < 5kΩ target
    case ads1299Calibration = 5   // ADS1299 internal reference self-calibration
    case hydrationCaps      = 6   // Moisture-barrier hydration cap removal before use
    case complete           = 7

    var id: Int { rawValue }

    var title: String {
        switch self {
        case .welcome:            return "Welcome to NeuroPulse"
        case .boaDial:            return "Fit Adjustment"
        case .electrodePods:      return "Electrode Contact"
        case .zoneModules:        return "Zone Modules"
        case .impedanceCheck:     return "Signal Quality Check"
        case .ads1299Calibration: return "Amplifier Calibration"
        case .hydrationCaps:      return "Electrode Hydration"
        case .complete:           return "Ready"
        }
    }

    var instruction: String {
        switch self {
        case .welcome:
            return "This short setup confirms your NeuroPulse is ready for its first session. It takes about 3 minutes."
        case .boaDial:
            return "Turn the Boa dial clockwise until the headset sits firmly on your head. The bridge should rest comfortably above your ears. All five electrode pods should touch your scalp."
        case .electrodePods:
            return "Press each electrode pod lightly against your scalp. Each pod travels ±12mm independently, so you don't need to dial tighter to reach every contact point. Remove any hydration caps from the electrode tips now."
        case .zoneModules:
            return "Insert your zone modules into the colour-coded slots. Each module will announce its position via bone conduction when seated correctly. Check that all five slots are confirmed below."
        case .impedanceCheck:
            return "Stay still while the app checks signal quality. Good contact = green. Moistening your hair slightly under the electrodes can help if you see red."
        case .ads1299Calibration:
            return "The EEG amplifier is calibrating its internal reference. This takes about 2 seconds and eliminates drift across all 8 channels. Stay still."
        case .hydrationCaps:
            return "Confirm you have removed the moisture-barrier hydration caps from all electrode tips. Store the caps in the provided case for future storage sessions."
        case .complete:
            return "Your NeuroPulse is set up and calibrated. You're ready for your first session."
        }
    }

    var requiresHardwareConfirmation: Bool {
        switch self {
        case .zoneModules, .impedanceCheck, .ads1299Calibration: return true
        default: return false
        }
    }
}

enum SetupError: LocalizedError {
    case notConnected
    case calibrationFailed
    case impedanceFailed(failedElectrodes: [Int])
    case zoneModulesMissing([Int])
    case timeout

    var errorDescription: String? {
        switch self {
        case .notConnected:
            return "Hub not connected. Connect via USB-C or Bluetooth."
        case .calibrationFailed:
            return "ADS1299 calibration failed. Try powering the hub off and on."
        case .impedanceFailed(let els):
            let names = els.map { "Electrode \($0 + 1)" }.joined(separator: ", ")
            return "High impedance on \(names). Moisten the scalp under those electrodes and retry."
        case .zoneModulesMissing(let slots):
            let names = slots.map { "Slot \($0 + 1)" }.joined(separator: ", ")
            return "Zone modules not detected in \(names). Re-seat and listen for the confirmation tone."
        case .timeout:
            return "Hardware confirmation timed out. Ensure the hub is connected and powered."
        }
    }
}

@MainActor
final class HardwareSetupManager: ObservableObject {

    @Published private(set) var currentStep: SetupStep = .welcome
    @Published private(set) var isProcessing = false
    @Published private(set) var lastError: SetupError?
    @Published private(set) var impedanceFlags: UInt16 = 0     // bitmask from GATT
    @Published private(set) var zoneConfiguration = ZoneModuleConfiguration()
    @Published private(set) var isFirstSetupComplete = false

    private let gatt: NeuroPulseGATTManager
    private var cancellables = Set<AnyCancellable>()

    private let firstSetupKey = "np.setup.first-complete"
    private let expectedElectrodeCount = 8  // 8-ch semi-dry EEG

    init(gatt: NeuroPulseGATTManager) {
        self.gatt = gatt
        isFirstSetupComplete = UserDefaults.standard.bool(forKey: firstSetupKey)
        observeGATT()
    }

    // MARK: - Step navigation

    func advance() {
        guard let next = SetupStep(rawValue: currentStep.rawValue + 1) else { return }
        currentStep = next
        lastError = nil
        if currentStep == .complete { markFirstSetupComplete() }
    }

    func retry() {
        lastError = nil
        isProcessing = false
    }

    // MARK: - Hardware confirmation steps

    func confirmImpedanceCheck() async {
        guard gatt.connectionState == .connected else {
            lastError = .notConnected; return
        }
        isProcessing = true
        lastError = nil
        defer { isProcessing = false }

        do {
            try await triggerCalibration(.impedanceCheck)
            // Wait for GATT impedance notification (up to 10 seconds)
            try await waitForImpedanceResult(timeout: 10)

            let failedElectrodes = (0..<expectedElectrodeCount).filter { bit in
                impedanceFlags & (1 << bit) == 0
            }
            if !failedElectrodes.isEmpty {
                lastError = .impedanceFailed(failedElectrodes: failedElectrodes)
            } else {
                advance()
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
        gatt.$session
            .map(\.impedancePassFlags)
            .removeDuplicates()
            .assign(to: \.impedanceFlags, on: self)
            .store(in: &cancellables)

        gatt.$zoneModules
            .sink { [weak self] rawSlots in
                self?.zoneConfiguration.update(rawSlotData: rawSlots)
            }
            .store(in: &cancellables)
    }

    private func triggerCalibration(_ opcode: CalibrationOpcode) async throws {
        try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Void, Error>) in
            gatt.sendCalibration(opcode) { result in
                switch result {
                case .success:      cont.resume()
                case .failure:      cont.resume(throwing: SetupError.timeout)
                }
            }
        }
    }

    private func waitForImpedanceResult(timeout: TimeInterval) async throws {
        let deadline = Date().addingTimeInterval(timeout)
        let initialFlags = impedanceFlags
        while impedanceFlags == initialFlags {
            guard Date() < deadline else { throw SetupError.timeout }
            try await Task.sleep(nanoseconds: 200_000_000)
        }
    }

    private func markFirstSetupComplete() {
        isFirstSetupComplete = true
        UserDefaults.standard.set(true, forKey: firstSetupKey)
    }
}

private typealias AnyCancellable = Combine.AnyCancellable
