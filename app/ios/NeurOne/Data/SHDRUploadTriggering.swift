import Combine
import Foundation

/// Abstraction over any source of SHDR upload triggers.
/// Conforming types publish `true` when the hub signals a pending SHDR upload
/// (hub GATT SHDR_UPLOAD_STATUS characteristic sends 0x01), and publish the
/// hub-provisioned 256-bit TRNG warranty token when it becomes available
/// (hub GATT warrantyToken characteristic, OI-WA-03 / OI-BLE-01).
///
/// Production conformance: NeurOneGATTManager (via extension below).
/// Test conformance:       MockSHDRUploadTrigger (subject-backed, in test target).
///
/// Principle of least privilege: SHDRUploader sees only these two SHDR-class signals,
/// not the full NeurOneGATTManager interface (which carries UHDR session data).
/// Both the upload-pending flag and the warranty token are SHDR-class (device
/// condition / opaque device identity) — never user biology — so surfacing them
/// through this SHDR-facing protocol keeps the UHDR/SHDR boundary intact.
@MainActor
protocol SHDRUploadTriggering: AnyObject {
    var shdrUploadPendingPublisher: AnyPublisher<Bool, Never> { get }

    /// Emits the hub-provisioned 256-bit TRNG warranty token (32 bytes) once the hub
    /// delivers it over GATT, `nil` before provisioning and on disconnect.
    /// SHDRUploader upgrades from its Keychain fallback to this token when non-nil
    /// (NP-FW-EMMC-002 Rev A §A — SHDR fleet DB no-join rule).
    var warrantyTokenPublisher: AnyPublisher<Data?, Never> { get }
}

extension NeurOneGATTManager: SHDRUploadTriggering {
    var shdrUploadPendingPublisher: AnyPublisher<Bool, Never> {
        $shdrUploadPending.eraseToAnyPublisher()
    }

    var warrantyTokenPublisher: AnyPublisher<Data?, Never> {
        $warrantyToken.eraseToAnyPublisher()
    }
}
