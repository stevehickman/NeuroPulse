import Combine

/// Abstraction over any source of consumable session counts.
/// Conforming types publish a `[UInt32]` array (4 values: intranasal / hydrogel / VNS / audio)
/// whenever the hub reports new CONSUMABLE_STATUS data.
///
/// Production conformance: NeurOneGATTManager (via extension below).
/// Test conformance:       MockCountsProvider (CurrentValueSubject-backed, in test target).
@MainActor
protocol ConsumableCountsProviding: AnyObject {
    var consumableCountsPublisher: AnyPublisher<[UInt32], Never> { get }
}

extension NeurOneGATTManager: ConsumableCountsProviding {
    var consumableCountsPublisher: AnyPublisher<[UInt32], Never> {
        $session.map(\.consumableSessionCounts).eraseToAnyPublisher()
    }
}
