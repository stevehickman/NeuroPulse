import Foundation

// Abstraction over the BLE write path for Mode 2 protocol upload.
// Keeping SessionProtocolUploader independent of CoreBluetooth lets the
// upload logic be exercised in unit tests without a real BLE stack.

protocol ProtocolUploadGateway: AnyObject {
    var isHubConnected: Bool { get }
    func uploadProtocol(_ blob: Data, completion: @escaping (Result<Void, GATTWriteError>) -> Void)
}

// NeuroPulseGATTManager satisfies this protocol without any code changes —
// we just add the conformance declaration here.
extension NeuroPulseGATTManager: ProtocolUploadGateway {
    var isHubConnected: Bool { connectionState == .connected }
}
