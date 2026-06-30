import Combine

/// Abstraction over any source of consumable session counts.
/// Conforming types publish a `[UInt16]` array (4 values: intranasal / hydrogel / VNS / audio)
/// whenever the hub reports new CONSUMABLE_STATUS data.
///
/// Production conformance: NeuroPulseGATTManager (via extension below).
/// Test conformance:       MockCountsProvider (CurrentValueSubject-backed, in test target).
@MainActor
protocol ConsumableCountsProviding: AnyObject {
    var consumableCountsPublisher: AnyPublisher<[UInt16], Never> { get }
}

extension NeuroPulseGATTManager: ConsumableCountsProviding {
    var consumableCountsPublisher: AnyPublisher<[UInt16], Never> {
        $session.map(\.consumableSessionCounts).eraseToAnyPublisher()
    }
}
