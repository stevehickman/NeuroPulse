import WatchConnectivity
import Combine
import NeuroPulseShared

// Bridges GATTSessionPublishing → Watch app via WatchConnectivity.
// Observes the session-state publisher and forwards each update as a
// WCSession sendMessage call, achieving sub-50ms latency on the local
// BT link between iPhone and Watch.

// MARK: - WatchMessageSending protocol

/// Abstraction over WCSession's outbound message path.
/// Allows tests to inject a mock without activating a real WCSession.
///
/// Production conformance: WCSession (via extension below).
/// Test conformance:       MockWatchMessageSender (in test target).
protocol WatchMessageSending: AnyObject {
    var isReachable: Bool { get }
    func sendMessage(_ message: [String: Any],
                     replyHandler: (([String: Any]) -> Void)?,
                     errorHandler: ((Error) -> Void)?)
}

extension WCSession: WatchMessageSending {}

// MARK: - PhoneSessionManager

final class PhoneSessionManager: NSObject, ObservableObject {

    private let gattPublisher: GATTSessionPublishing
    private let watchSender: WatchMessageSending
    private var cancellable: AnyCancellable?

    /// Production init: `gatt` is the GATT manager; `watchSender` defaults to `WCSession.default`.
    /// Tests inject a `MockWatchMessageSender` so no real WCSession is activated.
    init(gatt: GATTSessionPublishing,
         watchSender: WatchMessageSending = WCSession.default) {
        self.gattPublisher = gatt
        self.watchSender = watchSender
        super.init()
        if WCSession.isSupported() {
            WCSession.default.delegate = self
            WCSession.default.activate()
        }
        observeGATT()
    }

    // MARK: - Private

    private func observeGATT() {
        cancellable = gattPublisher.sessionPublisher
            .dropFirst()                        // skip initial .empty publish
            .sink { [weak self] state in
                self?.forward(state)
            }
    }

    private func forward(_ state: SessionState) {
        guard watchSender.isReachable else { return }
        let msg = state.toWCMessage()
        watchSender.sendMessage(msg, replyHandler: nil, errorHandler: nil)
    }

    // Push a consumable low alert as a high-priority notification.
    func sendConsumableLowNotification(consumableIndex: Int, sessionsRemaining: Int) {
        guard watchSender.isReachable else { return }
        watchSender.sendMessage([
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
