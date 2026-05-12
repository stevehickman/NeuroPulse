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
    private let chunkSize = 496     // 512-byte ATT MTU minus BLE overhead

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

        let wire = blob.wireFormat
        // Split into chunks and upload sequentially.
        // For typical protocol blobs (<2KB) this is usually a single write.
        var offset = 0
        while offset < wire.count {
            let end = min(offset + chunkSize, wire.count)
            let chunk = wire[offset..<end]
            try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
                gatt.uploadProtocol(Data(chunk)) { result in
                    switch result {
                    case .success:
                        continuation.resume()
                    case .failure(let e):
                        continuation.resume(throwing: UploadError.hubRejected(e.localizedDescription))
                    }
                }
            }
            offset = end
        }
    }
}
