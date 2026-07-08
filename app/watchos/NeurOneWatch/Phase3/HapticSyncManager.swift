import CoreHaptics
import Combine

// Phase 3 — 40Hz continuous Core Haptics sync.
//
// Drives Apple Watch Taptic Engine at 40Hz to add a wrist somatosensory
// channel alongside the NeurOne mastoid vibrotactile pad.
//
// Sync strategy: epoch is excluded from the WC bridge (UHDR boundary,
// NP-PRIV-ANALYSIS-002 MEDIUM-08) so sessionEpochMs is always 0 on Watch.
// Haptic starts immediately with no meaningful sync delay.
// Sub-50ms sync requires a future non-UHDR clock mechanism.
//
// Thermal prerequisite: OI-WA-01 (20-min continuous Core Haptics thermal
// characterization on Apple Watch Series / Ultra hardware) must pass before
// this manager is enabled in production builds.  See NP-APP-ROADMAP-001 §4.2.

final class HapticSyncManager: ObservableObject {

    @Published private(set) var isPlaying = false
    @Published private(set) var isAvailable = false
    @Published private(set) var thermalState: ThermalState = .nominal

    enum ThermalState { case nominal, elevated, serious, critical }

    // MARK: - Private

    private var engine: CHHapticEngine?
    private var player: CHHapticPatternPlayer?
    private var thermalTimer: Timer?

    private let targetFrequencyHz: Double = 40.0
    private let sessionDurationSeconds: Double = 20 * 60     // 20-minute target session

    // Ramp durations per roadmap spec: 2s up, 2s down.
    private let rampDurationSeconds: Double = 2.0

    // MARK: - Lifecycle

    init() {
        isAvailable = CHHapticEngine.capabilitiesForHardware().supportsHaptics
        guard isAvailable else { return }
        setupEngine()
        observeThermalState()
    }

    // Call when hub session starts. sessionEpochMs aligns haptic start to hub clock.
    func start(sessionEpochMs: UInt32) {
        guard isAvailable, thermalState == .nominal || thermalState == .elevated else { return }
        let offsetSec = Double(UInt32(Date().timeIntervalSince1970 * 1000) &- sessionEpochMs) / 1000.0
        let delayBeforeStart = max(0, 0.2 - offsetSec)   // 200ms pre-buffer, matching audio
        do {
            try engine?.start()
            let player = try buildPlayer()
            self.player = player
            // Schedule haptic to start synchronized with hub epoch + pre-buffer.
            try player.start(atTime: CHHapticTimeImmediate + delayBeforeStart)
            isPlaying = true
        } catch {
            isPlaying = false
        }
    }

    func stop() {
        try? player?.stop(atTime: CHHapticTimeImmediate)
        player = nil
        isPlaying = false
    }

    // MARK: - Engine setup

    private func setupEngine() {
        do {
            let eng = try CHHapticEngine()
            eng.stoppedHandler = { [weak self] reason in
                DispatchQueue.main.async {
                    self?.isPlaying = false
                    if reason != .engineDestroyed { self?.setupEngine() }
                }
            }
            eng.resetHandler = { [weak self] in
                try? self?.engine?.start()
            }
            engine = eng
        } catch {
            isAvailable = false
        }
    }

    // MARK: - Pattern construction
    //
    // Produces a continuous transient burst at 40Hz (25ms period) for the
    // full session duration, with 2-second amplitude ramps at start and end.

    private func buildPlayer() throws -> CHHapticPatternPlayer {
        let periodSec = 1.0 / targetFrequencyHz     // 0.025 s
        let totalEvents = Int(sessionDurationSeconds / periodSec)

        var events: [CHHapticEvent] = []
        events.reserveCapacity(totalEvents)

        for i in 0..<totalEvents {
            let t = Double(i) * periodSec

            // Amplitude envelope: ramp up for first 2s, full for middle, ramp down last 2s.
            let amplitude: Float
            if t < rampDurationSeconds {
                amplitude = Float(t / rampDurationSeconds)
            } else if t > sessionDurationSeconds - rampDurationSeconds {
                amplitude = Float((sessionDurationSeconds - t) / rampDurationSeconds)
            } else {
                amplitude = 1.0
            }

            let event = CHHapticEvent(
                eventType: .hapticTransient,
                parameters: [
                    CHHapticEventParameter(parameterID: .hapticIntensity, value: amplitude * 0.7),
                    CHHapticEventParameter(parameterID: .hapticSharpness, value: 0.8)
                ],
                relativeTime: t,
                duration: periodSec * 0.5     // 50% duty cycle within each 25ms window
            )
            events.append(event)
        }

        let pattern = try CHHapticPattern(events: events, parameters: [])
        return try engine!.makePlayer(with: pattern)
    }

    // MARK: - Thermal monitoring (OI-WA-01)

    private func observeThermalState() {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(thermalStateChanged),
            name: ProcessInfo.thermalStateDidChangeNotification,
            object: nil
        )
        updateThermalState()
    }

    @objc private func thermalStateChanged() { updateThermalState() }

    private func updateThermalState() {
        let mapped: ThermalState
        switch ProcessInfo.processInfo.thermalState {
        case .nominal:  mapped = .nominal
        case .fair:     mapped = .elevated
        case .serious:  mapped = .serious
        case .critical: mapped = .critical
        @unknown default: mapped = .nominal
        }
        DispatchQueue.main.async {
            self.thermalState = mapped
            // Auto-stop if Watch reaches serious thermal level to protect hardware.
            if mapped == .serious || mapped == .critical {
                self.stop()
            }
        }
    }
}

// MARK: - Availability guard view modifier

import SwiftUI

extension View {
    /// Wraps a view with a thermal/availability overlay for Phase 3.
    func hapticUnavailableOverlay(manager: HapticSyncManager) -> some View {
        self.overlay {
            if !manager.isAvailable {
                unavailableBanner("Haptic unavailable on this Apple Watch model.")
            } else if manager.thermalState == .serious || manager.thermalState == .critical {
                unavailableBanner("Haptic paused — Watch temperature elevated.")
            }
        }
    }

    private func unavailableBanner(_ text: String) -> some View {
        VStack {
            Spacer()
            Text(text)
                .font(.system(size: 11))
                .foregroundColor(.white)
                .multilineTextAlignment(.center)
                .padding(8)
                .background(Color.orange.opacity(0.85))
                .clipShape(RoundedRectangle(cornerRadius: 8))
                .padding(.bottom, 8)
        }
        .padding(.horizontal, 8)
    }
}
