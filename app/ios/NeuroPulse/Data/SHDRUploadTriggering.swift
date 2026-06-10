import Combine

/// Abstraction over any source of SHDR upload triggers.
/// Conforming types publish `true` when the hub signals a pending SHDR upload
/// (hub GATT SHDR_UPLOAD_STATUS characteristic sends 0x01).
///
/// Production conformance: NeuroPulseGATTManager (via extension below).
/// Test conformance:       MockSHDRUploadTrigger (PassthroughSubject-backed, in test target).
///
/// Principle of least privilege: SHDRUploader sees only this upload-trigger signal,
/// not the full NeuroPulseGATTManager interface (which carries UHDR session data).
protocol SHDRUploadTriggering: AnyObject {
    var shdrUploadPendingPublisher: AnyPublisher<Bool, Never> { get }
}

extension NeuroPulseGATTManager: SHDRUploadTriggering {
    var shdrUploadPendingPublisher: AnyPublisher<Bool, Never> {
        $shdrUploadPending.eraseToAnyPublisher()
    }
}
