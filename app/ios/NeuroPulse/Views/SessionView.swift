import SwiftUI
import OSLog

// TODO(localisation): strings below should use NSLocalizedString — see en.lproj/Localizable.strings

// Real-time session display — Mode 1 Connected (<1ms USB-C).
// Shows: connection status · session status · EEG/HRV live metrics ·
// HRV breathing ring · zone module status · session controls.

struct SessionView: View {

    @EnvironmentObject private var gatt:        NeuroPulseGATTManager
    @EnvironmentObject private var uploader:    SessionProtocolUploader
    @EnvironmentObject private var consumable:  ConsumableTracker
    @EnvironmentObject private var setup:       HardwareSetupManager
    @EnvironmentObject private var library:     NPProtocolLibrary
    @EnvironmentObject private var healthKit:   HealthKitSessionReader

    @State private var showProtocolPicker    = false
    @State private var showStopConfirmation  = false
    @State private var stopErrorMessage:     String?

    // True only while an HRV biofeedback session is live. The hub streams the HRV
    // coherence characteristic only for protocols that include HRV biofeedback, so
    // a non-nil session.hrv during a running session is the signal that HRV
    // biofeedback is the active modality. HealthKit is gated on this (ISC-94).
    private var hrvBiofeedbackActive: Bool {
        gatt.session.status == .running && gatt.session.hrv != nil
    }

    private static let logger = Logger(subsystem: "com.neuropulse.app", category: "SessionView")

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 20) {
                    connectionBanner
                    if consumable.sessionIsBlocked {
                        blockingConsumableAlert
                    } else {
                        sessionStatusCard
                        if gatt.session.status == .running {
                            hrvBreathingRing
                            liveMetricsGrid
                        }
                        zoneModuleRow
                        sessionControls
                    }
                    regulatoryFooter
                }
                .padding()
            }
            .navigationTitle("Session")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    connectionIndicator
                }
            }
            .sheet(isPresented: $showProtocolPicker) {
                ProtocolMenuView()
                    .environmentObject(library)
                    .environmentObject(uploader)
            }
            .confirmationDialog("End this session?",
                                isPresented: $showStopConfirmation,
                                titleVisibility: .visible) {
                Button("End Session", role: .destructive) { sendSessionStop() }
                Button("Cancel", role: .cancel) { }
            } message: {
                Text("The hub will stop the active session.")
            }
            .alert("Couldn't End Session",
                   isPresented: Binding(
                       get: { stopErrorMessage != nil },
                       set: { if !$0 { stopErrorMessage = nil } }
                   )) {
                Button("OK", role: .cancel) { stopErrorMessage = nil }
            } message: {
                Text(stopErrorMessage ?? "")
            }
            .onChange(of: hrvBiofeedbackActive) { _, isActive in
                if isActive {
                    Task { await healthKit.requestAuthorizationAndStart() }
                } else {
                    healthKit.stopAndClear()
                }
            }
            .onDisappear { healthKit.stopAndClear() }
        }
    }

    // Requests the hub to stop the session. Does NOT update session.status locally —
    // the UI reflects new state only after the hub reports it via GATT (ISC-34).
    private func sendSessionStop() {
        gatt.sendSessionStop { result in
            if case .failure(let error) = result {
                Self.logger.error("Session stop failed: \(String(describing: error))")
                stopErrorMessage = "Could not reach the hub. Please check the connection and try again."
            }
        }
    }

    // MARK: - Subviews

    private var connectionBanner: some View {
        HStack {
            Circle()
                .fill(connectionColor)
                .frame(width: 8, height: 8)
            Text(connectionLabel)
                .font(.footnote)
                .foregroundColor(.secondary)
            Spacer()
            if gatt.connectionState == .connected {
                Text(connectionModeLabel)
                    .font(.caption2)
                    .padding(.horizontal, 8).padding(.vertical, 2)
                    .background(Color.green.opacity(0.15))
                    .clipShape(Capsule())
            }
        }
        .padding(.horizontal, 4)
    }

    private var connectionColor: Color {
        switch gatt.connectionState {
        case .connected:    return .green
        case .connecting:   return .yellow
        case .scanning:     return .orange
        case .disconnected: return .red
        }
    }

    private var connectionLabel: String {
        switch gatt.connectionState {
        case .disconnected: return "Searching for hub…"
        case .scanning:     return "Scanning…"
        case .connecting:   return "Connecting…"
        case .connected:    return "Hub connected"
        }
    }

    // Distinguishes the wired USB-C transport (sub-millisecond, "Live") from the
    // BLE transport ("Wireless"). The BLE path deliberately makes no latency claim
    // because the <1ms guarantee only holds over USB-C (CLAUDE.md §4.1).
    //
    // USB-C detection: iOS exposes no public IOKit accessory API for this, so the
    // device charging state is used as a reasonable proxy — the hub powers the
    // phone over USB-C when wired. Battery monitoring is enabled app-wide in
    // NeuroPulseApp.swift (UIDevice.current.isBatteryMonitoringEnabled = true).
    private var connectionModeLabel: String {
        let isWired = UIDevice.current.batteryState == .charging
            || UIDevice.current.batteryState == .full
        return isWired ? "Mode 1 — Live (<1ms)" : "Mode 1 — Wireless"
    }

    private var connectionIndicator: some View {
        Image(systemName: gatt.connectionState == .connected ? "wifi" : "wifi.slash")
            .foregroundColor(connectionColor)
    }

    private var blockingConsumableAlert: some View {
        VStack(alignment: .leading, spacing: 12) {
            Label("Session Blocked", systemImage: "exclamationmark.triangle.fill")
                .foregroundColor(.red)
                .font(.headline)
            if let reason = consumable.sessionBlockReason {
                Text(reason)
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
            Text("Go to the Supplies tab to resolve this before starting a session.")
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .padding()
        .background(Color.red.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    private var sessionStatusCard: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                statusIndicator
                Spacer()
                if gatt.session.status == .running {
                    Text("Protocol \(gatt.session.protocolID)")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }

            if gatt.session.status == .idle {
                Text("No active session. Choose a protocol below to begin.")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    private var statusIndicator: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(sessionStatusColor)
                .frame(width: 10, height: 10)
                .overlay {
                    if gatt.session.status == .running {
                        Circle().fill(sessionStatusColor).opacity(0.4)
                            .scaleEffect(1.5)
                            .animation(.easeInOut(duration: 1).repeatForever(), value: gatt.session.status)
                    }
                }
            Text(sessionStatusLabel)
                .font(.headline)
        }
    }

    private var sessionStatusColor: Color {
        switch gatt.session.status {
        case .running:   return .green
        case .paused:    return .yellow
        case .completed: return .blue
        case .idle:      return .gray
        }
    }

    private var sessionStatusLabel: String {
        switch gatt.session.status {
        case .idle:      return "Ready"
        case .running:   return "Session Active"
        case .paused:    return "Paused"
        case .completed: return "Session Complete"
        }
    }

    // Breathing ring — expands on inhale, contracts on exhale.
    private var hrvBreathingRing: some View {
        VStack(spacing: 8) {
            ZStack {
                Circle()
                    .stroke(Color.blue.opacity(0.2), lineWidth: 6)
                    .frame(width: 120, height: 120)
                Circle()
                    .stroke(Color.blue, lineWidth: 6)
                    .frame(
                        width: gatt.session.pacerPhase == .inhale ? 120 : 80,
                        height: gatt.session.pacerPhase == .inhale ? 120 : 80
                    )
                    .animation(.easeInOut(duration: 2.5), value: gatt.session.pacerPhase)

                VStack(spacing: 2) {
                    Text(gatt.session.pacerPhase == .inhale ? "Inhale" : "Exhale")
                        .font(.caption2).foregroundColor(.blue)
                    if let hrv = gatt.session.hrv {
                        Text(String(format: "%.1f", hrv.coherenceScore))
                            .font(.title3.bold())
                        Text("coherence").font(.caption2).foregroundColor(.secondary)
                    }
                }
            }

            if let hrv = gatt.session.hrv {
                Text("RMSSD \(hrv.rmssdMilliseconds) ms")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
        }
    }

    private var liveMetricsGrid: some View {
        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
            if let hrv = gatt.session.hrv {
                MetricCard(title: "Coherence", value: String(format: "%.1f / 10", hrv.coherenceScore),
                           icon: "waveform.path.ecg", color: coherenceColor(hrv.coherenceScore))
                MetricCard(title: "RMSSD", value: "\(hrv.rmssdMilliseconds) ms",
                           icon: "heart.fill", color: .pink)
            }
            MetricCard(title: "EEG Contacts",
                       value: "\(impedancePassCount) / 8",
                       icon: "brain", color: impedancePassCount == 8 ? .green : .orange)
            if let healthKitHRV = healthKit.latestHRVSDNN {
                MetricCard(title: "HealthKit HRV",
                           value: String(format: "%.0f ms", healthKitHRV),
                           icon: "heart.text.square", color: .purple)
            }
        }
    }

    private var impedancePassCount: Int {
        (0..<8).filter { gatt.session.impedancePassFlags & (1 << $0) != 0 }.count
    }

    private func coherenceColor(_ score: Float) -> Color {
        score >= 7 ? .green : score >= 4 ? .yellow : .orange
    }

    private var zoneModuleRow: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Zone Modules").font(.caption).foregroundColor(.secondary)
            HStack(spacing: 8) {
                ForEach(0..<5, id: \.self) { slot in
                    let isPresent = slot < gatt.zoneModules.count && gatt.zoneModules[slot] != 0
                    RoundedRectangle(cornerRadius: 6)
                        .fill(isPresent ? Color.green.opacity(0.2) : Color(.systemGray5))
                        .overlay {
                            Text("\(slot + 1)")
                                .font(.caption2.bold())
                                .foregroundColor(isPresent ? .green : .secondary)
                        }
                        .frame(height: 36)
                }
            }
        }
    }

    private var sessionControls: some View {
        VStack(spacing: 12) {
            Button {
                showProtocolPicker = true
            } label: {
                Label("Choose Protocol", systemImage: "list.bullet.rectangle")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .disabled(!setup.isFirstSetupComplete || gatt.connectionState != .connected)

            if gatt.session.status == .running {
                Button(role: .destructive) {
                    showStopConfirmation = true
                } label: {
                    Label("End Session", systemImage: "stop.circle")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .disabled(gatt.connectionState != .connected)
            }
        }
    }

    private var regulatoryFooter: some View {
        Text("NeuroPulse is a general wellness device. It is not a medical device and is not intended to diagnose, treat, cure, or prevent any disease or health condition.")
            .font(.caption2)
            .foregroundColor(.secondary)
            .multilineTextAlignment(.center)
    }
}

// MARK: - Supporting subviews

struct MetricCard: View {
    let title: String
    let value: String
    let icon: String
    let color: Color

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Label(title, systemImage: icon)
                .font(.caption)
                .foregroundColor(color)
            Text(value)
                .font(.title3.bold())
                .foregroundColor(.primary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(color.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 10))
    }
}

