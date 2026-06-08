import Foundation

// Abstraction over the BLE write path for Mode 2 protocol upload.
// Keeping SessionProtocolUploader independent of CoreBluetooth lets the
// upload logic be exercised in unit tests without a real BLE stack.

protocol ProtocolUploadGateway: AnyObject {
    var isHubConnected: Bool { get }

    /// Write a single pre-framed BLE chunk to the hub's PROTOCOL_UPLOAD GATT characteristic.
    /// This method is called **once per chunk**, not once per protocol blob.
    /// SessionProtocolUploader drives sequencing: it calls this method for each chunk produced
    /// by ProtocolChunker (SINGLE / START / CONT / END frame headers) and advances only after
    /// the previous chunk's completion handler fires with `.success`.
    /// Hub firmware's reassembly state machine expects these framing headers in order.
    func uploadProtocol(_ blob: Data, completion: @escaping (Result<Void, GATTWriteError>) -> Void)
}

// NeuroPulseGATTManager satisfies this protocol without any code changes —
// we just add the conformance declaration here.
extension NeuroPulseGATTManager: ProtocolUploadGateway {
    var isHubConnected: Bool { connectionState == .connected }
}
