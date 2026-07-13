import Combine
import SwiftUI
#if canImport(WatchKit)
import WatchKit
#endif

// Phase 3 — watchOS haptic session cue.
//
// PLATFORM REALITY (decided 2026-07-13, NP-APP-ROADMAP-001 §4.2):
// Core Haptics (CHHapticEngine / CHHapticPattern) does NOT exist on watchOS.
// The original Phase 3 spec assumed a continuous 40Hz CHHapticEngine waveform;
// that is not achievable on any Apple Watch. The only watchOS haptic API is
// `WKInterfaceDevice.play(_:)`, which fires a fixed set of NAMED haptics on
// demand and cannot render an arbitrary continuous 40Hz waveform.
//
// Consequently this manager delivers a low-rate RHYTHMIC SESSION CUE — a gentle
// repeating tap that gives the wearer a felt "session-active" wrist presence.
// It is explicitly NOT 40Hz and NOT a therapeutic vibrotactile channel.
//
// This aligns with the marketing position (CLAUDE.md §3b/§15): the Watch haptic
// is a supplement, not a therapeutic. True 40Hz vibrotactile delivery is the
// NeurOne mastoid LRA pad (hardware, DRV2605L @ 40Hz ±0.5Hz). The iPhone app can
// render 40Hz via Core Haptics where the phone is the contact surface; the Watch
// cannot, so it provides a coarse cue only.
//
// Fidelity limitation (documented, not a bug): watchOS coalesces / rate-limits
// rapid `play()` calls, so the cue runs at `cuePeriodSeconds` (a felt pulse well
// below 40Hz), never a continuous buzz. Do not label this channel "40Hz".
//
// Sync strategy: epoch is excluded from the WC bridge (UHDR boundary,
// NP-PRIV-ANALYSIS-002 MEDIUM-08) so sessionEpochMs is always 0 on Watch.
// The cue starts immediately with no meaningful sync delay.
//
// Superseded open items: OI-WA-01 (20-min continuous Core Haptics thermal
// characterisation) and OI-WA-04 (CHHapticEngine 40Hz verification) no longer
// apply on watchOS — there is no continuous Core Haptics path to characterise.
// A lightweight thermal guard is retained below because a long-running repeating
// haptic timer still carries some (much smaller) thermal/battery cost.

final class HapticSyncManager: ObservableObject {

    @Published private(set) var isPlaying = false
    @Published private(set) var isAvailable = false
    @Published private(set) var thermalState: ThermalState = .nominal

    enum ThermalState { case nominal, elevated, serious, critical }

    // MARK: - Private

    private var cueTimer: Timer?
    private var sessionEndTimer: Timer?
    private var thermalObserver: NSObjectProtocol?

    // Cue cadence: a gentle felt pulse, NOT an entrainment frequency. Firing a
    // named haptic every 500ms (2Hz) is a reliable, perceptible "session-active"
    // presence that watchOS will not silently drop. This is deliberately far
    // below the 40Hz therapeutic target, which only the mastoid pad delivers.
    private let cuePeriodSeconds: Double = 0.5

    // Safety cap: stop the cue after the target session length even if a stop()
    // from the hub session is missed. The app also calls stop() on idle/completed.
    private let sessionDurationSeconds: Double = 20 * 60

    // MARK: - Lifecycle

    init() {
        #if canImport(WatchKit)
        // Every Apple Watch has a Taptic Engine; WKInterfaceDevice haptics are
        // always available. There is no Core-Haptics-style capability query.
        isAvailable = true
        observeThermalState()
        #else
        // Non-watchOS build (e.g. host tooling): no-op stub.
        isAvailable = false
        #endif
    }

    // Call when the hub session starts. sessionEpochMs is retained for API
    // parity with AudioSyncManager but is always 0 (UHDR boundary), so the cue
    // starts immediately.
    func start(sessionEpochMs: UInt32) {
        guard isAvailable, !isPlaying else { return }
        // Do not start the cue if the Watch is already thermally stressed.
        guard thermalState == .nominal || thermalState == .elevated else { return }

        #if canImport(WatchKit)
        playCue()   // immediate first tap so the cue is felt at session start
        let timer = Timer(timeInterval: cuePeriodSeconds, repeats: true) { [weak self] _ in
            self?.playCue()
        }
        RunLoop.main.add(timer, forMode: .common)
        cueTimer = timer

        let endTimer = Timer.scheduledTimer(withTimeInterval: sessionDurationSeconds, repeats: false) { [weak self] _ in
            self?.stop()
        }
        sessionEndTimer = endTimer

        isPlaying = true
        #endif
    }

    func stop() {
        cueTimer?.invalidate()
        cueTimer = nil
        sessionEndTimer?.invalidate()
        sessionEndTimer = nil
        isPlaying = false
    }

    // MARK: - Cue playback

    #if canImport(WatchKit)
    private func playCue() {
        // `.click` is a single light tap — the closest named haptic to a neutral
        // rhythmic pulse. Must be invoked on the main thread.
        WKInterfaceDevice.current().play(.click)
    }
    #endif

    deinit {
        if let thermalObserver {
            NotificationCenter.default.removeObserver(thermalObserver)
        }
    }

    // MARK: - Thermal monitoring (lightweight guard)

    // Block-based observer: HapticSyncManager is a pure Swift ObservableObject
    // (not an NSObject subclass), so the selector-based addObserver API is not
    // usable here. The closure keeps a weak reference to avoid a retain cycle.
    private func observeThermalState() {
        thermalObserver = NotificationCenter.default.addObserver(
            forName: ProcessInfo.thermalStateDidChangeNotification,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            self?.updateThermalState()
        }
        updateThermalState()
    }

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
            // Stop the cue if the Watch reaches a serious thermal level.
            if mapped == .serious || mapped == .critical {
                self.stop()
            }
        }
    }
}

// MARK: - Availability guard view modifier

extension View {
    /// Wraps a view with a thermal/availability overlay for the Phase 3 cue.
    func hapticUnavailableOverlay(manager: HapticSyncManager) -> some View {
        self.overlay {
            if !manager.isAvailable {
                unavailableBanner("Haptic cue unavailable on this device.")
            } else if manager.thermalState == .serious || manager.thermalState == .critical {
                unavailableBanner("Haptic cue paused — Watch temperature elevated.")
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
