import Foundation

// Abstraction over the BLE write path for Mode 2 protocol upload.
// Keeping SessionProtocolUploader independent of CoreBluetooth lets the
// upload logic be exercised in unit tests without a real BLE stack.

@MainActor
protocol ProtocolUploadGateway: AnyObject {
    var isHubConnected: Bool { get }

    /// Write one chunk of the protocol upload stream to the hub's PROTOCOL_UPLOAD GATT characteristic.
    /// This method is called **once per chunk**, not once per protocol blob.
    /// SessionProtocolUploader drives sequencing: it calls this method for each chunk produced
    /// by ProtocolChunker and advances only after the previous chunk's completion handler fires
    /// with `.success`. The chunks are one byte stream (a 4-byte little-endian length, then the
    /// blob) with no per-chunk header; the hub's transport reassembles it by the length
    /// (NP-FW-HUB-001 §2.3, REQ-FWHUB-80).
    func uploadProtocol(_ chunk: Data, completion: @escaping (Result<Void, GATTWriteError>) -> Void)

    /// True while a cardiac cutoff from a session that ran without the app is unread
    /// (NP-SW-FAULTMSG-001 P4).  A protocol containing cervical VNS is then refused.
    var cervicalRestartBlocked: Bool { get }

    /// True while someone other than the active user has an outstanding cardiac cutoff: a
    /// protocol containing cervical VNS then needs the "this is a different person" confirmation.
    var cervicalOutstandingForAnotherUser: Bool { get }

    /// The hub's 32-byte replay-guard serial (NP-FW-HUB-001 §4.2) once read over the encrypted
    /// link, else nil (OI-AND-WIRE-02). Stamped into every descriptor the uploader compiles.
    var deviceSerial: Data? { get }

    /// The helmet's hardware version (SWR-HV-06), 1 until it is read.
    var hardwareVersion: Int { get }

    /// The longest write the link carries now: the negotiated ATT_MTU less 3 bytes, which CoreBluetooth reports as
    /// `maximumWriteValueLength(for: .withResponse)`. The upload is cut at this size, to at most 512. A gateway that cannot tell
    /// answers the default ATT_MTU's 20, which the upload works at (OI-SIMFW-63).
    var maximumWriteLength: Int { get }

    /// What the hub says it received at the last upload (SWR-UP-01); nil before the first report or on a hub whose firmware
    /// does not publish it.
    var uploadCheck: UploadCheck? { get }

    /// Read the hub's current upload report, so a missed notification does not leave the outcome unknown.
    func requestUploadCheck()
}

extension ProtocolUploadGateway {
    /// Gateways with no cervical fault source never block.
    var cervicalRestartBlocked: Bool { false }
    var cervicalOutstandingForAnotherUser: Bool { false }
    var deviceSerial: Data? { nil }
    var hardwareVersion: Int { 1 }
    var uploadCheck: UploadCheck? { nil }
    func requestUploadCheck() {}
}

extension ProtocolUploadGateway {
    /// A gateway that does not report the negotiated MTU writes at the default ATT_MTU's 20 bytes.
    var maximumWriteLength: Int { BleTransfer.ATT_DEFAULT_WRITE_LENGTH }
}

// NeurOneGATTManager satisfies this protocol with the conformance declaration and the write length its peripheral reports.
extension NeurOneGATTManager: ProtocolUploadGateway {
    var isHubConnected: Bool { connectionState == .connected }
    var maximumWriteLength: Int { writeLengthWithResponse }
}
