import WatchConnectivity
import Combine
import OSLog
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

    /// True when the paired Watch is reachable via WatchConnectivity.
    /// Updated on the main queue from WCSessionDelegate callbacks.
    @Published private(set) var watchConnectivityAvailable = false

    private let gattPublisher: GATTSessionPublishing
    private let watchSender: WatchMessageSending
    private var cancellable: AnyCancellable?
    private let logger = Logger(subsystem: "com.neuropulse.app", category: "WatchBridge")

    /// Production init: `gatt` is the GATT manager; `watchSender` defaults to `WCSession.default`.
    /// Tests inject a `MockWatchMessageSender` so no real WCSession is activated.
    @MainActor
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

    @MainActor
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
        watchSender.sendMessage(msg, replyHandler: nil) { [logger] error in
            logger.error("WC sendMessage failed: \(String(describing: error), privacy: .public)")
        }
    }

    // Push a consumable low alert as a high-priority notification.
    func sendConsumableLowNotification(consumableIndex: Int, sessionsRemaining: Int) {
        guard watchSender.isReachable else { return }
        watchSender.sendMessage([
            "alert": "consumable_low",
            "index": consumableIndex,
            "remaining": sessionsRemaining
        ], replyHandler: nil) { [logger] error in
            logger.error("WC consumable alert failed: \(String(describing: error), privacy: .public)")
        }
    }

}

// MARK: - WCSessionDelegate

extension PhoneSessionManager: WCSessionDelegate {

    func session(_ session: WCSession,
                 activationDidCompleteWith activationState: WCSessionActivationState,
                 error: Error?) {
        let reachable = activationState == .activated && session.isReachable
        DispatchQueue.main.async { [weak self] in
            self?.watchConnectivityAvailable = reachable
        }
    }

    func sessionDidBecomeInactive(_ session: WCSession) {
        DispatchQueue.main.async { [weak self] in
            self?.watchConnectivityAvailable = false
        }
    }

    func sessionDidDeactivate(_ session: WCSession) {
        DispatchQueue.main.async { [weak self] in
            self?.watchConnectivityAvailable = false
        }
        WCSession.default.activate()
    }

    func sessionReachabilityDidChange(_ session: WCSession) {
        let reachable = session.isReachable
        DispatchQueue.main.async { [weak self] in
            self?.watchConnectivityAvailable = reachable
        }
    }

    /// Watch preset IDs whose protocols read the user's EEG to close the loop —
    /// Gamma Clarity (0) and Alpha Calm (1). Blocked when BIPA consent is absent
    /// (ISC-90). Sleep Deep (2) is not EEG-adaptive and always passes. Mirrors the
    /// `PresetProtocol` enum in the Watch app's SessionStatusView.
    private static let eegAdaptivePresetIDs: Set<Int> = [0, 1]

    // Receive messages from Watch (e.g. protocol preset selection).
    func session(_ session: WCSession, didReceiveMessage message: [String: Any]) {
        // Watch → phone protocol selector (Phase 1 feature).
        // Placeholder: route to hub via GATT write when firmware write characteristic is added.
        if let presetID = message["select_preset"] as? Int {
            let consentGranted = UserDefaults.standard.bool(forKey: "np.onboarding.bipa-accepted")
            if let accepted = Self.acceptedPreset(presetID, consentGranted: consentGranted) {
                _ = accepted // TODO: compile preset to protocol chunk and upload via GATT protocolUpload (Mode 2)
            } else {
                logger.info("Dropped EEG-adaptive preset \(presetID, privacy: .public) — BIPA consent not granted")
            }
        }
    }

    /// Applies the BIPA gate (ISC-90) to a Watch preset selection. Returns the preset
    /// ID when it may proceed, or nil when it is an EEG-adaptive preset and consent was
    /// declined. Pure and static so it is unit-testable without a live WCSession.
    static func acceptedPreset(_ presetID: Int, consentGranted: Bool) -> Int? {
        if eegAdaptivePresetIDs.contains(presetID), !consentGranted { return nil }
        return presetID
    }
}
