import WatchConnectivity
import Combine

// Watch-side WatchConnectivity manager.
// Receives forwarded SessionState messages from PhoneSessionManager and
// publishes them for consumption by all phase views.

final class WatchSessionManager: NSObject, ObservableObject {

    static let shared = WatchSessionManager()

    @Published private(set) var session: SessionState = .empty
    @Published private(set) var isPhoneReachable = false

    // Alerts — non-session push messages.
    @Published private(set) var latestImpedancePassCount: Int? = nil
    @Published private(set) var latestConsumableLowIndex: Int? = nil

    private override init() {
        super.init()
        if WCSession.isSupported() {
            WCSession.default.delegate = self
            WCSession.default.activate()
        }
    }

    // Send a protocol preset selection back to phone (Phase 1 basic selector).
    func selectPreset(_ presetID: Int) {
        guard WCSession.default.isReachable else { return }
        WCSession.default.sendMessage(["select_preset": presetID], replyHandler: nil, errorHandler: nil)
    }
}

// MARK: - WCSessionDelegate

extension WatchSessionManager: WCSessionDelegate {

    func session(_ session: WCSession,
                 activationDidCompleteWith activationState: WCSessionActivationState,
                 error: Error?) {}

    func sessionReachabilityDidChange(_ session: WCSession) {
        DispatchQueue.main.async { self.isPhoneReachable = session.isReachable }
    }

    func session(_ session: WCSession, didReceiveMessage message: [String: Any]) {
        DispatchQueue.main.async {
            if let alert = message["alert"] as? String {
                switch alert {
                case "impedance_result":
                    // Only pass count (non-UHDR) — raw bitmask is UHDR-class and must not
                    // flow over the unencrypted WC bridge (CLAUDE.md §5.1).
                    self.latestImpedancePassCount = message["pass_count"] as? Int
                case "consumable_low":
                    self.latestConsumableLowIndex = message["index"] as? Int
                default: break
                }
                return
            }
            if let state = SessionState.from(wcMessage: message) {
                self.session = state
            }
        }
    }
}
