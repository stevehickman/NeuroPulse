import Foundation

// OTA firmware update state model.
// Dual-bank OTA: hub has Bank A (current) and Bank B (staging).
// Per NP-FW-EMMC-001 Rev A §8.

enum OTAPhase: UInt8, CustomStringConvertible {
    case idle           = 0x00
    case preparing      = 0x01  // Hub clearing Bank B
    case transferring   = 0x02  // Receiving chunks
    case verifying      = 0x03  // Ed25519 + SHA-256 check
    case applying       = 0x04  // Boot bank flag swap + reboot
    case complete       = 0x05  // Post-reboot, running from Bank B
    case failed         = 0xFF

    init(rawPacketByte: UInt8) {
        self = OTAPhase(rawValue: rawPacketByte) ?? .idle
    }

    var description: String {
        switch self {
        case .idle:         return "Ready"
        case .preparing:    return "Preparing update…"
        case .transferring: return "Transferring firmware…"
        case .verifying:    return "Verifying signature…"
        case .applying:     return "Applying update — do not disconnect"
        case .complete:     return "Update complete"
        case .failed:       return "Update failed"
        }
    }
}

struct FirmwareImage: Identifiable {
    var id: UUID = UUID()
    var version: String
    var buildDate: Date
    var imageSizeBytes: Int
    var ed25519PublicKeyFingerprint: String  // hex — displayed before user confirms
    var isSafetyMCUFirmware: Bool           // requires explicit separate user confirmation
    var releaseNotes: String
}

struct OTASession {
    var image: FirmwareImage
    var phase: OTAPhase = .idle
    var progressPercent: Int = 0
    var errorCode: UInt16 = 0
    var startedAt: Date = Date()
    var completedAt: Date?

    var isActive: Bool { phase != .idle && phase != .complete && phase != .failed }
    var isError: Bool { errorCode != 0 || phase == .failed }

    // Chunk transfer tracking
    var totalChunks: Int = 0
    var sentChunks: Int = 0
    static let chunkSize = 496  // 512B ATT MTU minus opcode + handle overhead
}
