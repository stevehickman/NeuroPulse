import SwiftUI

@main
struct NeuroPulseApp: App {

    // Singletons shared through the environment.
    @StateObject private var gatt   = NeuroPulseGATTManager()
    @StateObject private var bridge = PhoneSessionManager(gatt: NeuroPulseGATTManager())

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(gatt)
                .environmentObject(bridge)
        }
    }
}

// Minimal iOS content view — hosts the hub connection status and session overview.
// Full iOS app UI is out of scope for this file; the Watch app is the primary deliverable.
struct ContentView: View {

    @EnvironmentObject private var gatt: NeuroPulseGATTManager

    var body: some View {
        NavigationStack {
            VStack(spacing: 16) {
                connectionStatusView
                sessionSummaryView
                Spacer()
                regulatoryFooter
            }
            .padding()
            .navigationTitle("NeuroPulse")
        }
    }

    private var connectionStatusView: some View {
        HStack {
            Circle()
                .fill(gatt.connectionState == .connected ? Color.green : Color.orange)
                .frame(width: 10, height: 10)
            Text(connectionLabel)
                .font(.subheadline)
                .foregroundColor(.secondary)
            Spacer()
        }
        .padding(.horizontal, 4)
    }

    private var connectionLabel: String {
        switch gatt.connectionState {
        case .disconnected: return "Searching for NeuroPulse hub…"
        case .scanning:     return "Scanning…"
        case .connecting:   return "Connecting…"
        case .connected:    return "Hub connected"
        }
    }

    private var sessionSummaryView: some View {
        VStack(alignment: .leading, spacing: 8) {
            if gatt.session.status == .running {
                Label("Session active", systemImage: "brain.head.profile")
                    .foregroundColor(.green)
                if let hrv = gatt.session.hrv {
                    Label(String(format: "Coherence: %.1f / 10", hrv.coherenceScore),
                          systemImage: "waveform.path.ecg")
                    Label("RMSSD: \(hrv.rmssdMilliseconds) ms",
                          systemImage: "heart.text.clipboard")
                }
            } else {
                Label("No active session", systemImage: "moon.zzz")
                    .foregroundColor(.secondary)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    private var regulatoryFooter: some View {
        Text("The Apple Watch app provides session monitoring and user interface aids only. All therapeutic functions are delivered exclusively by NeuroPulse hardware.")
            .font(.caption2)
            .foregroundColor(.secondary)
            .multilineTextAlignment(.center)
    }
}
