import SwiftUI

// Phase 1 — Root Watch view: session status, HRV breathing ring, elapsed timer,
// impedance alerts, consumable reminders, and basic protocol preset selector.

struct SessionStatusView: View {

    @ObservedObject private var mgr = WatchSessionManager.shared
    @State private var elapsedSeconds: Int = 0
    @State private var elapsedTimer: Timer? = nil
    @State private var showImpedanceAlert = false
    @State private var showConsumableAlert = false
    @State private var showPresetPicker = false

    private var session: SessionState { mgr.session }

    var body: some View {
        NavigationStack {
            ZStack {
                Color.black.ignoresSafeArea()

                VStack(spacing: 8) {
                    // Connection / status chip.
                    statusChip

                    if session.status == .running || session.status == .paused {
                        // Breathing ring — only shown during active HRV protocols.
                        HRVBreathingRingView(
                            phase: session.pacerPhase,
                            elapsedPercent: session.pacerElapsedPercent,
                            coherenceScore: session.hrv?.coherenceScore
                        )

                        HStack(spacing: 16) {
                            rmssdView
                            elapsedView
                        }
                    } else {
                        idleView
                    }
                }
                .padding(.horizontal, 8)
            }
            .navigationTitle("NeuroPulse")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button { showPresetPicker = true } label: {
                        Image(systemName: "slider.horizontal.3")
                    }
                }
            }
        }
        .sheet(isPresented: $showPresetPicker) { presetPickerView }
        .alert("Impedance Check", isPresented: $showImpedanceAlert) {
            Button("OK", role: .cancel) {}
        } message: {
            impedanceAlertMessage
        }
        .alert("Consumable Low", isPresented: $showConsumableAlert) {
            Button("OK", role: .cancel) {}
        } message: {
            consumableAlertMessage
        }
        .onChange(of: session.status) { _, newStatus in
            handleStatusChange(newStatus)
        }
        .onChange(of: mgr.latestImpedancePassFlags) { _, _ in
            showImpedanceAlert = true
        }
        .onChange(of: mgr.latestConsumableLowIndex) { _, idx in
            if idx != nil { showConsumableAlert = true }
        }
    }

    // MARK: - Sub-views

    private var statusChip: some View {
        HStack(spacing: 4) {
            Circle()
                .fill(statusColor)
                .frame(width: 7, height: 7)
            Text(statusLabel)
                .font(.system(size: 11, weight: .medium))
                .foregroundColor(.white.opacity(0.7))
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 4)
        .background(Color.white.opacity(0.08))
        .clipShape(Capsule())
    }

    private var statusColor: Color {
        switch session.status {
        case .idle:      return mgr.isPhoneReachable ? .yellow : .gray
        case .running:   return .green
        case .paused:    return .orange
        case .completed: return .blue
        }
    }

    private var statusLabel: String {
        if !mgr.isPhoneReachable { return "Not connected" }
        switch session.status {
        case .idle:      return "Idle"
        case .running:   return "Session active"
        case .paused:    return "Paused"
        case .completed: return "Complete"
        }
    }

    private var rmssdView: some View {
        VStack(spacing: 2) {
            if let rmssd = session.hrv?.rmssdMilliseconds {
                Text("\(rmssd)")
                    .font(.system(size: 18, weight: .semibold, design: .rounded))
                    .foregroundColor(.white)
                Text("RMSSD ms")
                    .font(.system(size: 9))
                    .foregroundColor(.white.opacity(0.5))
            } else {
                Text("–")
                    .font(.system(size: 18, weight: .semibold))
                    .foregroundColor(.gray)
                Text("RMSSD ms")
                    .font(.system(size: 9))
                    .foregroundColor(.white.opacity(0.5))
            }
        }
        .frame(maxWidth: .infinity)
    }

    private var elapsedView: some View {
        VStack(spacing: 2) {
            Text(elapsedFormatted)
                .font(.system(size: 18, weight: .semibold, design: .monospaced))
                .foregroundColor(.white)
            Text("elapsed")
                .font(.system(size: 9))
                .foregroundColor(.white.opacity(0.5))
        }
        .frame(maxWidth: .infinity)
    }

    private var elapsedFormatted: String {
        let m = elapsedSeconds / 60
        let s = elapsedSeconds % 60
        return String(format: "%d:%02d", m, s)
    }

    private var idleView: some View {
        VStack(spacing: 12) {
            Image(systemName: "brain.head.profile")
                .font(.system(size: 36))
                .foregroundColor(.white.opacity(0.3))
            Text("No active session")
                .font(.system(size: 13))
                .foregroundColor(.white.opacity(0.4))
            if !mgr.isPhoneReachable {
                Text("Open NeuroPulse on iPhone")
                    .font(.system(size: 11))
                    .foregroundColor(.white.opacity(0.3))
                    .multilineTextAlignment(.center)
            }
        }
        .padding(.top, 16)
    }

    // Protocol preset picker — sends selection back to phone (which routes to hub).
    private var presetPickerView: some View {
        List {
            ForEach(PresetProtocol.allCases) { preset in
                Button {
                    mgr.selectPreset(preset.rawValue)
                    showPresetPicker = false
                } label: {
                    VStack(alignment: .leading, spacing: 2) {
                        Text(preset.displayName)
                            .font(.system(size: 13, weight: .medium))
                        Text(preset.duration)
                            .font(.system(size: 11))
                            .foregroundColor(.secondary)
                    }
                }
            }
        }
        .navigationTitle("Presets")
    }

    // MARK: - Alert messages

    @ViewBuilder
    private var impedanceAlertMessage: some View {
        if let flags = mgr.latestImpedancePassFlags {
            let passed = (0..<8).filter { flags & (1 << $0) != 0 }.count
            Text("\(passed)/8 electrodes passed. Check headset fit if any failed.")
        }
    }

    @ViewBuilder
    private var consumableAlertMessage: some View {
        if let idx = mgr.latestConsumableLowIndex {
            Text("\(ConsumableName.name(for: idx)) is running low. Order via NeuroPulse app.")
        }
    }

    // MARK: - Timer management

    private func handleStatusChange(_ status: SessionStatus) {
        switch status {
        case .running:
            elapsedSeconds = 0
            elapsedTimer?.invalidate()
            elapsedTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { _ in
                elapsedSeconds += 1
            }
            WKInterfaceDevice.current().play(.start)
        case .completed:
            elapsedTimer?.invalidate()
            WKInterfaceDevice.current().play(.success)
        case .paused:
            elapsedTimer?.invalidate()
        case .idle:
            elapsedTimer?.invalidate()
            elapsedSeconds = 0
        }
    }
}

// MARK: - Supporting types

enum PresetProtocol: Int, CaseIterable, Identifiable {
    case gamma = 0
    case alpha = 1
    case sleep = 2

    var id: Int { rawValue }

    var displayName: String {
        switch self {
        case .gamma: return "Gamma Clarity"
        case .alpha: return "Alpha Calm"
        case .sleep: return "Sleep Deep"
        }
    }

    var duration: String {
        switch self {
        case .gamma: return "20 min · 40 Hz"
        case .alpha: return "20 min · 10 Hz"
        case .sleep: return "30 min · 2 Hz"
        }
    }
}

enum ConsumableName {
    static func name(for index: Int) -> String {
        switch index {
        case 0: return "Intranasal sleeves"
        case 1: return "Electrode hydrogel tips"
        case 2: return "VNS clip pads"
        case 3: return "Audio cup foam"
        default: return "Consumable"
        }
    }
}
