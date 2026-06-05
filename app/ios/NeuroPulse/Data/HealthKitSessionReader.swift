import HealthKit
import Combine
import os

// Reads HRV SDNN and heart rate from HealthKit during an active session only.
// Data is used exclusively for real-time SessionView display.
// Never persisted. Never transmitted. Session-scoped only.
// Per NP-APP-ROADMAP-001 Rev B §9.1.
//
// Privacy invariants (ISC-94/95/96):
//   - Authorization is requested only when an HRV biofeedback session is running
//     (callers gate on gatt.session.status == .running AND HRV active) — never at
//     app launch or consent time.
//   - latestHRVSDNN / latestHeartRate are @Published in-memory only. They are never
//     written to UserDefaults, a file, an app database, or any network call.
//   - This type only ever READS from HealthKit. It never writes: no HealthKit write
//     API (the save/add family) is ever called, and the share set passed to
//     authorization is always empty.
@MainActor
final class HealthKitSessionReader: ObservableObject {

    @Published private(set) var latestHRVSDNN: Double? = nil      // milliseconds
    @Published private(set) var latestHeartRate: Double? = nil    // beats per minute
    @Published private(set) var isAuthorized = false

    private let healthStore = HKHealthStore()
    private var hrvQuery: HKAnchoredObjectQuery?
    private var heartRateQuery: HKAnchoredObjectQuery?

    private let logger = Logger(subsystem: "com.neuropulse.app", category: "HealthKitSessionReader")

    private let hrvUnit = HKUnit.secondUnit(with: .milli)
    private let heartRateUnit = HKUnit.count().unitDivided(by: .minute())

    // MARK: - Lifecycle

    /// Requests read-only authorization for HRV SDNN and heart rate, then begins
    /// streaming the most recent sample of each. Call only when an HRV biofeedback
    /// session has started — not at app launch or consent time (ISC-94).
    func requestAuthorizationAndStart() async {
        guard HKHealthStore.isHealthDataAvailable() else {
            logger.info("HealthKit unavailable on this device; skipping HRV read.")
            return
        }

        guard
            let hrvType = HKObjectType.quantityType(forIdentifier: .heartRateVariabilitySDNN),
            let heartRateType = HKObjectType.quantityType(forIdentifier: .heartRate)
        else {
            logger.error("Failed to resolve HealthKit quantity types.")
            return
        }

        // Read-only: the share (write) set is intentionally empty (ISC-96).
        let readTypes: Set<HKObjectType> = [hrvType, heartRateType]

        do {
            try await healthStore.requestAuthorization(toShare: [], read: readTypes)
        } catch {
            logger.error("HealthKit authorization request failed: \(error.localizedDescription, privacy: .public)")
            return
        }

        // sharingAuthorizationStatus reflects write permission only; HealthKit never
        // discloses read-grant status for privacy reasons. We treat the absence of an
        // explicit denial as "proceed" and let the queries return nil if no data flows.
        let bothDetermined =
            healthStore.authorizationStatus(for: hrvType) != .notDetermined &&
            healthStore.authorizationStatus(for: heartRateType) != .notDetermined
        isAuthorized = bothDetermined

        startQuery(for: hrvType, unit: hrvUnit, assignTo: \.latestHRVSDNN, store: { self.hrvQuery = $0 })
        startQuery(for: heartRateType, unit: heartRateUnit, assignTo: \.latestHeartRate, store: { self.heartRateQuery = $0 })
    }

    /// Stops streaming and clears all in-memory values immediately. No persistence (ISC-95).
    /// Call when the session ends.
    func stopAndClear() {
        if let q = hrvQuery {
            healthStore.stop(q)
            hrvQuery = nil
        }
        if let q = heartRateQuery {
            healthStore.stop(q)
            heartRateQuery = nil
        }
        latestHRVSDNN = nil
        latestHeartRate = nil
    }

    // MARK: - Query construction

    private func startQuery(
        for type: HKQuantityType,
        unit: HKUnit,
        assignTo keyPath: ReferenceWritableKeyPath<HealthKitSessionReader, Double?>,
        store: @escaping (HKAnchoredObjectQuery) -> Void
    ) {
        let handler: (HKAnchoredObjectQuery, [HKSample]?, [HKDeletedObject]?, HKQueryAnchor?, Error?) -> Void = {
            [weak self] _, samples, _, _, error in
            guard let self else { return }
            if let error {
                self.logger.error("HealthKit query error: \(error.localizedDescription, privacy: .public)")
                return
            }
            guard
                let latest = (samples as? [HKQuantitySample])?
                    .max(by: { $0.endDate < $1.endDate })
            else { return }
            let value = latest.quantity.doubleValue(for: unit)
            // In-memory publish only — never persisted or transmitted (ISC-95).
            Task { @MainActor in self[keyPath: keyPath] = value }
        }

        let query = HKAnchoredObjectQuery(
            type: type,
            predicate: nil,
            anchor: nil,
            limit: HKObjectQueryNoLimit,
            resultsHandler: handler
        )
        query.updateHandler = handler
        healthStore.execute(query)
        store(query)
    }
}
