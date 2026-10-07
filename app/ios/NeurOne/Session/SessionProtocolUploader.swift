import Foundation
import Combine

// Mode 2: Protocol upload to hub.
// Compiles the definition to the binary descriptor of NP-FW-HUB-001 §4 (OI-AND-WIRE-01), signs
// it with the device's Ed25519 key, chunks it into BLE MTU-sized packets, and uploads to the PROTOCOL_UPLOAD GATT characteristic.
// Hub firmware verifies the Ed25519 signature and rejects unsigned or corrupted protocols.

enum UploadError: LocalizedError {
    case signingFailed(Error)
    case bleNotReady
    case hubRejected(String)
    case timeout
    case validationFailed([NPValidationIssue])
    /// A PBM target could not be resolved to helmet sockets. Distinct from
    /// `validationFailed` so it is obvious when the backstop fired rather than
    /// the validator — that combination means a target the validator does not
    /// yet check.
    case targetUnresolvable(Error)
    /// The protocol contains cervical VNS, and a cardiac cutoff from a session that ran without
    /// the app has not been read yet (NP-SW-FAULTMSG-001 P4).
    case cervicalRestartBlocked
    /// The protocol contains cervical VNS and another person on this device has an outstanding
    /// cardiac cutoff: the wearer must confirm they are a different person first.
    case differentPersonConfirmationRequired

    var errorDescription: String? {
        switch self {
        case .signingFailed(let e):
            return "Protocol signing failed: \(e.localizedDescription)"
        case .bleNotReady:
            return "Hub not connected. Please connect via USB-C or Bluetooth."
        case .hubRejected(let r):
            return "Hub rejected protocol: \(r)"
        case .timeout:
            return "Upload timed out. Check hub connection and try again."
        case .validationFailed(let issues):
            let summary = issues.prefix(3).map(\.message).joined(separator: "; ")
            let extra = issues.count > 3 ? " (and \(issues.count - 3) more)" : ""
            return "Protocol validation failed: \(summary)\(extra)"
        case .targetUnresolvable(let e):
            return "Protocol target could not be resolved: \(e.localizedDescription)"
        case .cervicalRestartBlocked:
            return String(localized: "UPLOAD_CERVICAL_BLOCKED")
        case .differentPersonConfirmationRequired:
            return String(localized: "CVNS_DIFFERENT_PERSON_BODY")
        }
    }
}

@MainActor
final class SessionProtocolUploader: ObservableObject {

    @Published private(set) var isUploading = false
    @Published private(set) var lastError: UploadError?

    private let gatt: any ProtocolUploadGateway

    init(gatt: some ProtocolUploadGateway) {
        self.gatt = gatt
    }

    // Upload an NPProtocolDefinition to the hub (Mode 2 Programming).
    // Validates the definition against hardware safety limits, compiles it to
    // the §4 descriptor, signs, and uploads.
    //
    // `deviceSerial` is the 32-byte replay guard the hub checks (§4.2). Omitted, it is the one the
    // gateway read from the hub's DEVICE_SERIAL characteristic (OI-AND-WIRE-02); nil on both is
    // bench-only, since a hub with a provisioned serial refuses the descriptor. `clinicianSockets` is the operator's choice for
    // `clinician_selected` PBM targets.
    func upload(_ definition: NPProtocolDefinition,
                deviceSerial: Data? = nil,
                clinicianSockets: [Int]? = nil,
                acknowledgedCautions: [String] = []) async throws {
        try await send(try buildDescriptor(from: definition, deviceSerial: deviceSerial,
                                           clinicianSockets: clinicianSockets,
                                           acknowledgedCautions: acknowledgedCautions))
    }

    /// What the author must acknowledge before `upload` or `programAutonomous` will compile this protocol
    /// (docs/reference/safety-zones.md). Empty when it is safe; a protocol in the danger zone is a validation
    /// error and is refused whatever is acknowledged. The caller shows `CautionAcknowledgementView` and passes
    /// the ids back as `acknowledgedCautions`; nothing is kept between calls.
    func cautions(for definition: NPProtocolDefinition) -> [NPZoneCaution] {
        NPProtocolValidator(resolvedLimits: .unlimited).validate(definition).zoneCautions
    }

    /// Set by the "this is a different person" confirmation; consumed by the next cervical
    /// upload.  One confirmation covers one upload, so it cannot go stale.
    private var differentPersonConfirmed = false

    func confirmDifferentPerson() {
        differentPersonConfirmed = true
    }

    /// NP-SW-FAULTMSG-001 P4 and the per-user cardiac scope.  The safety MCU holds every cutoff
    /// regardless (P1); these make the app say why, and put a profile switch on the record.
    ///  - an unread cardiac cutoff from an offline session → refused until it is read;
    ///  - another person's outstanding cutoff → one explicit "different person" confirmation.
    /// A blocked user can still start a cervical session: the device holds cervical VNS and the
    /// re-enable confirmation runs inside it.
    private func checkCervicalGate(_ definition: NPProtocolDefinition) throws {
        guard definition.modalities.contains(where: { $0.enabled && $0.modalityType == .cervicalVns })
        else { return }
        if gatt.cervicalRestartBlocked {
            let err = UploadError.cervicalRestartBlocked
            lastError = err
            throw err
        }
        if gatt.cervicalOutstandingForAnotherUser {
            guard differentPersonConfirmed else {
                let err = UploadError.differentPersonConfirmationRequired
                lastError = err
                throw err
            }
            differentPersonConfirmed = false
        }
    }

    // Send an already-compiled descriptor to the hub. PRIVATE on purpose: every upload enters
    // through a definition (upload(_:) / programAutonomous(_:)), so it passes
    // buildDescriptor's checks — including the cervical gate — and the protocol menu shows
    // the same message or confirmation whichever mode it was sent in. A public wire-level
    // entry point was a way round that gate (NP-SW-FAULTMSG-001 §9.5).
    private func send(_ descriptor: HubDescriptor) async throws {
        guard gatt.isHubConnected else { throw UploadError.bleNotReady }
        isUploading = true
        lastError = nil
        defer { isUploading = false }

        // Frame the blob per the hub's BLE chunking protocol (START/CONT/END
        // or a single SINGLE chunk). Chunking is a pure transform, separate
        // from the BLE write path, so it stays unit-testable.
        let chunks = ProtocolChunker.chunk(descriptor.blob)

        // Send chunks sequentially via a recursive completion-handler chain:
        // each chunk's success callback dispatches the next. The final
        // completion only resolves with success after the last chunk ACKs, and
        // resolves with failure immediately on the first chunk that fails.
        do {
            try await sendChunksSequentially(chunks)
        } catch let err as UploadError {
            lastError = err
            throw err
        }
    }

    /// Upload framed chunks one at a time, advancing only after each ACK.
    /// Bridges the recursive completion-handler chain into async/await.
    private func sendChunksSequentially(_ chunks: [Data]) async throws {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            sendChunk(chunks, index: 0, continuation: continuation)
        }
    }

    /// Send chunk at `index`; on success recurse to `index + 1`; on failure
    /// resume the continuation with an error immediately. When all chunks have
    /// ACKed, resume with success.
    private func sendChunk(_ chunks: [Data],
                           index: Int,
                           continuation: CheckedContinuation<Void, Error>) {
        guard index < chunks.count else {
            continuation.resume()
            return
        }
        gatt.uploadProtocol(chunks[index]) { result in
            // GATT callbacks fire on the central manager's main queue; hop onto
            // the main actor to safely touch isolated state and recurse.
            Task { @MainActor [weak self] in
                switch result {
                case .success:
                    guard let self else {
                        continuation.resume(throwing: UploadError.bleNotReady)
                        return
                    }
                    self.sendChunk(chunks, index: index + 1, continuation: continuation)
                case .failure(let e):
                    continuation.resume(throwing: UploadError.hubRejected(e.localizedDescription))
                }
            }
        }
    }

    // Program a session onto the hub for Mode 3 Autonomous use, so it runs standalone from any
    // USB-C PD power bank without a phone present (CLAUDE.md §4.6). Intended to be invoked from
    // the setup flow.
    //
    // The header's `NP_PROTO_FLAG_AUTONOMOUS` bit (bit 1) says so. No firmware reader exists for it
    // yet, so today the hub runs both modes the same way.
    func programAutonomous(_ definition: NPProtocolDefinition,
                           deviceSerial: Data? = nil,
                           clinicianSockets: [Int]? = nil,
                           acknowledgedCautions: [String] = []) async throws {
        try await send(try buildDescriptor(from: definition, deviceSerial: deviceSerial,
                                           clinicianSockets: clinicianSockets, autonomous: true,
                                           acknowledgedCautions: acknowledgedCautions))
    }

    // Validate a definition against hardware safety limits and compile it to the signed §4
    // descriptor. Sets lastError and throws on any failure so upload and programAutonomous
    // share identical error handling and both populate lastError consistently.
    private func buildDescriptor(
        from definition: NPProtocolDefinition,
        deviceSerial: Data?,
        clinicianSockets: [Int]?,
        autonomous: Bool = false,
        acknowledgedCautions: [String] = []
    ) throws -> HubDescriptor {
        guard gatt.isHubConnected else {
            let err = UploadError.bleNotReady
            lastError = err
            throw err
        }
        try checkCervicalGate(definition)
        let result = NPProtocolValidator(resolvedLimits: .unlimited).validate(definition)
        guard result.isValid else {
            let err = UploadError.validationFailed(result.errors)
            lastError = err
            throw err
        }
        // Target resolution and drive-register conversion can still throw here even though the
        // validator ran: this is the backstop that keeps a bad target from ever reaching a
        // signed blob. A signing failure is its own case.
        let unsigned: HubDescriptor
        do {
            unsigned = try HubDescriptorCompiler.build(definition, deviceSerial: deviceSerial ?? gatt.deviceSerial,
                                                       clinicianSockets: clinicianSockets,
                                                       autonomous: autonomous,
                                                       acknowledgedCautions: acknowledgedCautions)
        } catch {
            let err = UploadError.targetUnresolvable(error)
            lastError = err
            throw err
        }
        do {
            return try HubDescriptorCompiler.signed(unsigned)
        } catch {
            let err = UploadError.signingFailed(error)
            lastError = err
            throw err
        }
    }
}
