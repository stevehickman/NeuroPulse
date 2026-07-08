import Combine
import NeurOneShared

/// Abstraction over any source of SessionState publications.
/// Conforming types emit an updated SessionState whenever hub GATT session data changes.
///
/// Production conformance: NeurOneGATTManager (via extension below).
/// Test conformance:       MockGATTSessionPublisher (CurrentValueSubject-backed, in test target).
///
/// Principle of least privilege: PhoneSessionManager sees only the session-state
/// publisher, not the full NeurOneGATTManager interface (which carries UHDR data).
@MainActor
protocol GATTSessionPublishing: AnyObject {
    var sessionPublisher: AnyPublisher<SessionState, Never> { get }
}

// NeurOneGATTManager already provides sessionPublisher via SetupGATTProviding.
// Conformance is declared here without re-implementing the property.
extension NeurOneGATTManager: GATTSessionPublishing {}
