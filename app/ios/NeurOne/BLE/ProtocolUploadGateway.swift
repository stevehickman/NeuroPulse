import Foundation

// Abstraction over the BLE write path for Mode 2 protocol upload.
// Keeping SessionProtocolUploader independent of CoreBluetooth lets the
// upload logic be exercised in unit tests without a real BLE stack.

@MainActor
protocol ProtocolUploadGateway: AnyObject {
    var isHubConnected: Bool { get }

    /// Write a single pre-framed BLE chunk to the hub's PROTOCOL_UPLOAD GATT characteristic.
    /// This method is called **once per chunk**, not once per protocol blob.
    /// SessionProtocolUploader drives sequencing: it calls this method for each chunk produced
    /// by ProtocolChunker (SINGLE / START / CONT / END frame headers) and advances only after
    /// the previous chunk's completion handler fires with `.success`.
    /// Hub firmware's reassembly state machine expects these framing headers in order.
    func uploadProtocol(_ chunk: Data, completion: @escaping (Result<Void, GATTWriteError>) -> Void)

    /// True while a cardiac cutoff from a session that ran without the app is unread
    /// (NP-SW-FAULTMSG-001 P4).  A protocol containing cervical VNS is then refused.
    var cervicalRestartBlocked: Bool { get }

    /// True while someone other than the active user has an outstanding cardiac cutoff: a
    /// protocol containing cervical VNS then needs the "this is a different person" confirmation.
    var cervicalOutstandingForAnotherUser: Bool { get }
}

extension ProtocolUploadGateway {
    /// Gateways with no cervical fault source never block.
    var cervicalRestartBlocked: Bool { false }
    var cervicalOutstandingForAnotherUser: Bool { false }
}

// NeurOneGATTManager satisfies this protocol without any code changes —
// we just add the conformance declaration here.
extension NeurOneGATTManager: ProtocolUploadGateway {
    var isHubConnected: Bool { connectionState == .connected }
}
