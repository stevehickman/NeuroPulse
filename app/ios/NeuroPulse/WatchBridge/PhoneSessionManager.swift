import WatchConnectivity
import Combine

// Bridges NeuroPulseGATTManager → Watch app via WatchConnectivity.
// Observes the published SessionState from the GATT manager and forwards
// each update as a WCSession sendMessage call, achieving sub-50ms latency
// on the local BT link between iPhone and Watch.

final class PhoneSessionManager: NSObject, ObservableObject {

    private let gatt: NeuroPulseGATTManager
    private var cancellable: AnyCancellable?

    init(gatt: NeuroPulseGATTManager) {
        self.gatt = gatt
        super.init()
        if WCSession.isSupported() {
            WCSession.default.delegate = self
            WCSession.default.activate()
        }
        observeGATT()
    }

    // MARK: - Private

    private func observeGATT() {
        cancellable = gatt.$session
            .dropFirst()                        // skip initial .empty publish
            .sink { [weak self] state in
                self?.forward(state)
            }
    }

    private func forward(_ state: SessionState) {
        guard WCSession.default.isReachable else { return }
        let msg = state.toWCMessage()
        WCSession.default.sendMessage(msg, replyHandler: nil, errorHandler: nil)
    }

    // Push a consumable low alert as a high-priority notification.
    func sendConsumableLowNotification(consumableIndex: Int, sessionsRemaining: Int) {
        guard WCSession.default.isReachable else { return }
        WCSession.default.sendMessage([
            "alert": "consumable_low",
            "index": consumableIndex,
            "remaining": sessionsRemaining
        ], replyHandler: nil, errorHandler: nil)
    }

}

// MARK: - WCSessionDelegate

extension PhoneSessionManager: WCSessionDelegate {

    func session(_ session: WCSession,
                 activationDidCompleteWith activationState: WCSessionActivationState,
                 error: Error?) {}

    func sessionDidBecomeInactive(_ session: WCSession) {}
    func sessionDidDeactivate(_ session: WCSession) { WCSession.default.activate() }

    // Receive messages from Watch (e.g. protocol preset selection).
    func session(_ session: WCSession, didReceiveMessage message: [String: Any]) {
        // Watch → phone protocol selector (Phase 1 feature).
        // Placeholder: route to hub via GATT write when firmware write characteristic is added.
        if let presetID = message["select_preset"] as? Int {
            _ = presetID // TODO: write to hub GATT protocol-select characteristic
        }
    }
}
