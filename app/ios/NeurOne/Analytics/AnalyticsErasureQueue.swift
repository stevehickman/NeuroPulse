import Foundation
import os

// Erasing a withdrawn person's pseudonymous analytics ID from the vendor (NP-APP-TELEMETRY-001 §5, REQ-SW03-19).
//
// The policy says the ID must be deleted from the vendor's servers within 30 days of a research-consent withdrawal, by a vendor
// deletion call made at withdrawal time. The SDK's `reset()` makes a NEW anonymous ID, so the old one is gone from the phone the
// moment it runs: unless it was kept, nothing can ever ask for its deletion. So the gate captures it first
// (`ResearchAnalyticsGate.reset()`), and this queue holds it, durably, until the erasure relay has accepted it or refused it for
// good. A withdrawal made offline is therefore not lost: the request is sent when the phone next can.
//
// The relay is a NeurOne service that holds the vendor's private API key, which must never be in the app. It is not built yet
// (OI-ANALYTICS-01); until it exists a queued ID stays queued, which is the honest state: the erasure has not happened.

/// What the relay said about one ID.
enum ErasureOutcome: Equatable {
    /// The relay accepted the request (any 2xx). Erasing an ID the vendor does not hold is also accepted: the call is idempotent.
    case done
    /// The relay refused it for good (a 4xx other than 408 and 429). Retrying the same request cannot change the answer.
    case refused
    /// A network error, a timeout, a 408, a 429 or a 5xx. The ID stays queued.
    case retryLater
}

@MainActor
protocol AnalyticsErasureRelaying: AnyObject {
    func erase(distinctID: String) async -> ErasureOutcome
}

/// The real relay: one POST of `{"distinct_id": …}` and nothing else, over an ephemeral session (no cookies, no cache).
///
/// The ID is a random value the person's phone made, known only to the phone and the vendor, so knowing it is the proof that
/// the request is the owner's (the relay must still rate-limit by source). The request carries no other identifier, no account,
/// no device name and no consent record.
@MainActor
final class HTTPAnalyticsErasureRelay: AnalyticsErasureRelaying {
    /// Same self-hosted instance as the analytics host (`PostHogAnalyticsBackend`); change both to the production domain.
    static let endpoint = URL(string: "https://analytics.neurone.internal/np/erase")!

    private let session: URLSession

    init(session: URLSession = URLSession(configuration: .ephemeral)) {
        self.session = session
    }

    func erase(distinctID: String) async -> ErasureOutcome {
        var request = URLRequest(url: Self.endpoint)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try? JSONSerialization.data(withJSONObject: ["distinct_id": distinctID])

        guard let (_, response) = try? await session.data(for: request), let http = response as? HTTPURLResponse else {
            return .retryLater
        }
        switch http.statusCode {
        case 200..<300: return .done
        case 408, 429: return .retryLater
        case 400..<500: return .refused
        default: return .retryLater
        }
    }
}

/// The IDs awaiting erasure, kept on the phone until the relay has answered for each.
///
/// Stored in `ProtectedDefaults` (backup-excluded, file-protected): the list is per install, and an ID in it is a pseudonym the
/// vendor holds data under, so it does not go into a backup.
@MainActor
final class AnalyticsErasureQueue {
    static let shared = AnalyticsErasureQueue()

    private static let log = Logger(subsystem: "life.neurone.analytics", category: "erasure-queue")
    private static let key = "np.analytics.pending-erasures"

    private let store: ProtectedDefaults
    private let relay: any AnalyticsErasureRelaying
    private var isDraining = false

    init(store: ProtectedDefaults = .standard, relay: any AnalyticsErasureRelaying = HTTPAnalyticsErasureRelay()) {
        self.store = store
        self.relay = relay
    }

    /// The IDs still waiting, oldest first.
    var pending: [String] {
        guard let data = store.data(forKey: Self.key),
              let ids = try? JSONDecoder().decode([String].self, from: data)
        else { return [] }
        return ids
    }

    /// Keep `id` until the relay has answered for it. Persisted before it returns, so a crash right after loses nothing.
    /// An ID already queued is not queued twice.
    func enqueue(_ id: String) {
        guard !id.isEmpty else { return }
        var ids = pending
        guard !ids.contains(id) else { return }
        ids.append(id)
        save(ids)
    }

    /// Ask the relay about every queued ID. An ID leaves the queue when the relay accepted it or refused it for good; on a
    /// transient failure it stays, and the rest wait for the next call (the network is likely down for all of them).
    /// Does nothing, and makes no network call, when the queue is empty.
    func drain() async {
        guard !isDraining else { return }
        isDraining = true
        defer { isDraining = false }

        for id in pending {
            switch await relay.erase(distinctID: id) {
            case .done:
                remove(id)
            case .refused:
                // Never log the ID: it is the pseudonym the vendor holds data under.
                Self.log.fault("AnalyticsErasureQueue: the relay refused an erasure request for good; dropping it from the queue.")
                remove(id)
            case .retryLater:
                return
            }
        }
    }

    private func remove(_ id: String) {
        save(pending.filter { $0 != id })
    }

    private func save(_ ids: [String]) {
        if ids.isEmpty {
            store.removeObject(forKey: Self.key)
        } else if let data = try? JSONEncoder().encode(ids) {
            store.set(data, forKey: Self.key)
        }
    }
}
