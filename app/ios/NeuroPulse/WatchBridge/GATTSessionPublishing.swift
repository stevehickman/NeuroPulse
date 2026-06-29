import Combine
import NeuroPulseShared

/// Abstraction over any source of SessionState publications.
/// Conforming types emit an updated SessionState whenever hub GATT session data changes.
///
/// Production conformance: NeuroPulseGATTManager (via extension below).
/// Test conformance:       MockGATTSessionPublisher (CurrentValueSubject-backed, in test target).
///
/// Principle of least privilege: PhoneSessionManager sees only the session-state
/// publisher, not the full NeuroPulseGATTManager interface (which carries UHDR data).
@MainActor
protocol GATTSessionPublishing: AnyObject {
    var sessionPublisher: AnyPublisher<SessionState, Never> { get }
}

// NeuroPulseGATTManager already provides sessionPublisher via SetupGATTProviding.
// Conformance is declared here without re-implementing the property.
extension NeuroPulseGATTManager: GATTSessionPublishing {}
