import SwiftUI

// watchOS app entry point.
// Hosts all four phase views in a TabView. Audio and haptic managers are created
// at app init as @StateObject and injected via .environmentObject().

@main
struct NeurOneWatchApp: App {

    @StateObject private var audioMgr   = AudioSyncManager()
    @StateObject private var hapticMgr  = HapticSyncManager()
    private let watchMgr = WatchSessionManager.shared

    var body: some Scene {
        WindowGroup {
            RootTabView(audioMgr: audioMgr, hapticMgr: hapticMgr)
                .environmentObject(audioMgr)
                .environmentObject(hapticMgr)
                .onReceive(watchMgr.$session) { state in
                    // Forward session status and pacer events to active managers.
                    if state.status == .running {
                        audioMgr.onPacerPhaseChanged(state.pacerPhase)
                    }
                    if state.status == .idle || state.status == .completed {
                        audioMgr.stop()
                        hapticMgr.stop()
                    }
                    if state.status == .running && !audioMgr.isPlaying {
                        audioMgr.start(sessionEpochMs: state.epoch)
                    }
                    if state.status == .running && !hapticMgr.isPlaying {
                        hapticMgr.start(sessionEpochMs: state.epoch)
                    }
                }
        }
    }
}

struct RootTabView: View {

    @ObservedObject var audioMgr:  AudioSyncManager
    @ObservedObject var hapticMgr: HapticSyncManager

    var body: some View {
        TabView {
            // Phase 1 — Session status + HRV breathing ring (always available).
            SessionStatusView()
                .tag(0)

            // Phase 2 — Audio sync channel indicator.
            AudioStatusView()
                .tag(1)

            // Phase 3 — haptic session cue status (WKInterfaceDevice, not 40Hz).
            HapticStatusView()
                .tag(2)

            // Phase 4 — Visual flicker + EMDR (gated on OI-WA-02).
            VisualFlickerView()
                .tag(3)
        }
        .tabViewStyle(.page)
    }
}

// MARK: - Simple status views for Phase 2 and 3

struct AudioStatusView: View {
    @EnvironmentObject var audioMgr: AudioSyncManager
    @ObservedObject  var watchMgr = WatchSessionManager.shared

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            VStack(spacing: 10) {
                Image(systemName: audioMgr.isPlaying ? "airpodspro" : "airpodspro.chargingcase")
                    .font(.system(size: 32))
                    .foregroundColor(audioMgr.isPlaying ? .cyan : .gray)
                Text(audioMgr.isPlaying ? "Audio sync active" : "Audio sync idle")
                    .font(.system(size: 13, weight: .medium))
                    .foregroundColor(.white)
                Text(String(localized: "WATCH_AIRPODS_HINT"))
                    .font(.system(size: 10))
                    .foregroundColor(.white.opacity(0.4))
                    .multilineTextAlignment(.center)
            }
            .padding(.horizontal, 12)
        }
    }
}

struct HapticStatusView: View {
    @EnvironmentObject var hapticMgr: HapticSyncManager

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            VStack(spacing: 10) {
                Image(systemName: "hand.raised.fingers.spread")
                    .font(.system(size: 32))
                    .foregroundColor(hapticStatusColor)
                Text(hapticStatusLabel)
                    .font(.system(size: 13, weight: .medium))
                    .foregroundColor(.white)
                thermalNote
            }
            .padding(.horizontal, 12)
            .hapticUnavailableOverlay(manager: hapticMgr)
        }
    }

    private var hapticStatusColor: Color {
        if !hapticMgr.isAvailable { return .gray }
        return hapticMgr.isPlaying ? .purple : .gray
    }

    private var hapticStatusLabel: String {
        if !hapticMgr.isAvailable { return "Not available" }
        return hapticMgr.isPlaying ? "Haptic cue active" : "Haptic cue idle"
    }

    @ViewBuilder
    private var thermalNote: some View {
        switch hapticMgr.thermalState {
        case .elevated:
            Text(String(localized: "WATCH_WARM"))
                .font(.system(size: 10))
                .foregroundColor(.orange.opacity(0.7))
        case .serious, .critical:
            Text(String(localized: "WATCH_HAPTIC_PAUSED"))
                .font(.system(size: 10))
                .foregroundColor(.red.opacity(0.8))
        default:
            Text(String(localized: "WATCH_MASTOID_NOTE"))
                .font(.system(size: 10))
                .foregroundColor(.white.opacity(0.35))
                .multilineTextAlignment(.center)
        }
    }
}
