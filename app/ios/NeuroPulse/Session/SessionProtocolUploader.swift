import Foundation
import Combine

// Mode 2: Protocol upload to hub.
// Signs the session protocol with the device's Ed25519 key, chunks it into
// BLE MTU-sized packets, and uploads to the PROTOCOL_UPLOAD GATT characteristic.
// Hub firmware verifies the Ed25519 signature and rejects unsigned or corrupted protocols.

enum UploadError: LocalizedError {
    case signingFailed(Error)
    case bleNotReady
    case hubRejected(String)
    case timeout

    var errorDescription: String? {
        switch self {
        case .signingFailed(let e): return "Protocol signing failed: \(e.localizedDescription)"
        case .bleNotReady:          return "Hub not connected. Please connect via USB-C or Bluetooth."
        case .hubRejected(let r):   return "Hub rejected protocol: \(r)"
        case .timeout:              return "Upload timed out. Check hub connection and try again."
        }
    }
}

@MainActor
final class SessionProtocolUploader: ObservableObject {

    @Published private(set) var isUploading = false
    @Published private(set) var lastError: UploadError?

    private let gatt: NeuroPulseGATTManager

    init(gatt: NeuroPulseGATTManager) {
        self.gatt = gatt
    }

    // Upload a session protocol to the hub (Mode 2 Programming).
    // On success the hub enters Mode 2: it will run this protocol when triggered.
    func upload(_ proto: NPSessionProtocol) async throws {
        guard gatt.connectionState == .connected else { throw UploadError.bleNotReady }
        isUploading = true
        lastError = nil
        defer { isUploading = false }

        let blob: SignedProtocolBlob
        do {
            blob = try SessionProtocolSigner.sign(proto)
        } catch {
            let err = UploadError.signingFailed(error)
            lastError = err
            throw err
        }

        // Frame the blob per the hub's BLE chunking protocol (START/CONT/END
        // or a single SINGLE chunk). Chunking is a pure transform, separate
        // from the BLE write path, so it stays unit-testable.
        let chunks = ProtocolChunker.chunk(blob.wireFormat)

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

    // Program a session protocol onto the hub for Mode 3 Autonomous use.
    // Forces the protocol's operating mode to `.mode3Autonomous` so the hub
    // runs it standalone from any USB-C PD power bank without a phone present
    // (CLAUDE.md §4.6). Intended to be invoked from the setup flow.
    func programAutonomous(_ proto: NPSessionProtocol) async throws {
        var autonomous = proto
        autonomous.mode = .mode3Autonomous
        try await upload(autonomous)
    }
}
