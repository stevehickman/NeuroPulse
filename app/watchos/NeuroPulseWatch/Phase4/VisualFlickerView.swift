import SwiftUI

// Phase 4 — 40Hz visual flicker + EMDR L/R indicator.
//
// BLOCKED on OI-WA-02: Apple Watch screen brightness must be characterized
// at ≥100 nits at 40Hz before this view is shown in production.
// Until characterization passes, showFlicker is always false and the view
// renders a placeholder (EMDR arrow + breathing ring only).
//
// When OI-WA-02 passes, set OI_WA_02_PASSED = true in build settings.
//
// Regulatory: "This feature uses flashing light. Do not enable if you have
// photosensitive epilepsy or have been advised to avoid flashing lights."
// The photoparoxysmal safety cutoff from the NeuroPulse EEG (Oz electrode,
// <200ms, CLAUDE.md §4.2) is mirrored here via WatchConnectivity within
// one message cycle — when the hub halts the goggle LEDs, the Watch flicker
// also stops.

// Build-time gate for OI-WA-02. Replace with true only after documented
// characterization measurement (NP-APP-ROADMAP-001 §4.2 Phase 4).
private let OI_WA_02_PASSED: Bool = false

struct VisualFlickerView: View {

    @ObservedObject private var mgr = WatchSessionManager.shared
    @State private var flickerOn = false
    @State private var flickerTimer: Timer? = nil
    @State private var showFlickerWarningAck = false
    @State private var userAcknowledgedWarning = false

    // EMDR state — L/R alternation driven by session clock.
    @State private var emdrArrowLeft = true
    @State private var emdrTimer: Timer? = nil

    // Flicker parameters.
    private let flickerHz: Double = 40.0                 // 40 Hz ± 0.5 Hz (OI-WA-02 confirmation required)
    private let emdrCadenceHz: Double = 1.0              // 1 alternation per second (adjustable)

    private var session: SessionState { mgr.session }

    var body: some View {
        ZStack {
            // Flicker background layer (only when enabled and characterization passed).
            if OI_WA_02_PASSED && userAcknowledgedWarning && session.status == .running {
                flickerLayer
            } else {
                Color.black
            }

            VStack(spacing: 10) {
                // EMDR arrow — available regardless of flicker gate.
                emdrArrow

                if !OI_WA_02_PASSED {
                    characterisationPending
                } else if !userAcknowledgedWarning {
                    warningPrompt
                } else {
                    activeStatus
                }
            }
            .padding(.horizontal, 12)
        }
        .ignoresSafeArea()
        .onChange(of: session.status) { _, status in handleStatusChange(status) }
        // Hub photoparoxysmal cutoff mirror: when hub halts goggles, Watch flicker stops too.
        // The WatchSessionManager receives a status update (paused/idle) from the phone
        // within one WatchConnectivity message cycle when the EEG safety interlock fires.
        .onChange(of: session.status) { _, status in
            if status == .idle || status == .paused { stopFlicker() }
        }
    }

    // MARK: - Sub-views

    private var flickerLayer: some View {
        Rectangle()
            .fill(flickerOn ? Color.white : Color.black)
            // No SwiftUI animation — raw timer-driven toggle achieves 40Hz precision.
            .ignoresSafeArea()
    }

    private var emdrArrow: some View {
        HStack(spacing: 0) {
            Image(systemName: "chevron.left")
                .font(.system(size: 28, weight: .bold))
                .foregroundColor(emdrArrowLeft ? .white : .white.opacity(0.15))
                .frame(maxWidth: .infinity)
            Image(systemName: "chevron.right")
                .font(.system(size: 28, weight: .bold))
                .foregroundColor(emdrArrowLeft ? .white.opacity(0.15) : .white)
                .frame(maxWidth: .infinity)
        }
        .frame(height: 44)
    }

    private var characterisationPending: some View {
        VStack(spacing: 6) {
            Image(systemName: "exclamationmark.triangle")
                .font(.system(size: 20))
                .foregroundColor(.orange)
            Text("40Hz flicker unavailable")
                .font(.system(size: 12, weight: .semibold))
                .foregroundColor(.white)
            Text("Screen brightness characterization pending (OI-WA-02).")
                .font(.system(size: 10))
                .foregroundColor(.white.opacity(0.5))
                .multilineTextAlignment(.center)
            Text("EMDR arrow active.")
                .font(.system(size: 10))
                .foregroundColor(.white.opacity(0.5))
        }
    }

    private var warningPrompt: some View {
        VStack(spacing: 8) {
            Image(systemName: "eye.trianglebadge.exclamationmark")
                .font(.system(size: 24))
                .foregroundColor(.yellow)
            Text("Flashing light warning")
                .font(.system(size: 12, weight: .semibold))
                .foregroundColor(.white)
            Text("This feature uses flashing light at 40Hz. Do not enable if you have photosensitive epilepsy or have been advised to avoid flashing lights.")
                .font(.system(size: 10))
                .foregroundColor(.white.opacity(0.7))
                .multilineTextAlignment(.center)
            Button("I understand — enable") {
                userAcknowledgedWarning = true
                if session.status == .running { startFlicker() }
            }
            .font(.system(size: 11, weight: .semibold))
            .foregroundColor(.black)
            .padding(.horizontal, 12)
            .padding(.vertical, 6)
            .background(Color.yellow)
            .clipShape(Capsule())
        }
    }

    private var activeStatus: some View {
        VStack(spacing: 4) {
            Circle()
                .fill(flickerOn ? Color.white : Color.white.opacity(0.3))
                .frame(width: 8, height: 8)
            Text("40Hz active")
                .font(.system(size: 10))
                .foregroundColor(.white.opacity(0.6))
        }
    }

    // MARK: - Timer control

    private func handleStatusChange(_ status: SessionStatus) {
        switch status {
        case .running:
            if OI_WA_02_PASSED && userAcknowledgedWarning { startFlicker() }
            startEMDR()
        case .idle, .completed:
            stopFlicker()
            stopEMDR()
        case .paused:
            stopFlicker()
            stopEMDR()
        }
    }

    private func startFlicker() {
        flickerTimer?.invalidate()
        // 40Hz = 25ms period. Timer fires every 12.5ms to toggle the bit,
        // achieving a full 40Hz on/off cycle.
        let halfPeriod = 1.0 / (flickerHz * 2.0)
        flickerTimer = Timer.scheduledTimer(withTimeInterval: halfPeriod, repeats: true) { _ in
            flickerOn.toggle()
        }
    }

    private func stopFlicker() {
        flickerTimer?.invalidate()
        flickerTimer = nil
        flickerOn = false
    }

    private func startEMDR() {
        emdrTimer?.invalidate()
        let period = 1.0 / emdrCadenceHz
        emdrTimer = Timer.scheduledTimer(withTimeInterval: period, repeats: true) { _ in
            emdrArrowLeft.toggle()
        }
    }

    private func stopEMDR() {
        emdrTimer?.invalidate()
        emdrTimer = nil
    }
}

#Preview {
    VisualFlickerView()
        .background(Color.black)
}
