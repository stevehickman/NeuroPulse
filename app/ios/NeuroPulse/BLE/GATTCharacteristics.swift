import CoreBluetooth

// UUIDs are placeholders — replace at firmware BLE implementation stage (OI-WA-03).
// Format: 128-bit custom service / characteristic UUIDs.

enum NPUUID {
    static let service          = CBUUID(string: "4E455550-0001-1000-8000-00805F9B34FB")

    // Notify-only characteristics — match NP-APP-ROADMAP-001 §5
    static let sessionState     = CBUUID(string: "4E455550-0002-1000-8000-00805F9B34FB") // NOTIFY 4B
    static let sessionStatus    = CBUUID(string: "4E455550-0003-1000-8000-00805F9B34FB") // NOTIFY 2B
    static let hrvCoherence     = CBUUID(string: "4E455550-0004-1000-8000-00805F9B34FB") // NOTIFY 4B
    static let pacerPhase       = CBUUID(string: "4E455550-0005-1000-8000-00805F9B34FB") // NOTIFY 2B
    static let impedanceResult  = CBUUID(string: "4E455550-0006-1000-8000-00805F9B34FB") // NOTIFY 2B
    static let consumableStatus = CBUUID(string: "4E455550-0007-1000-8000-00805F9B34FB") // READ/NOTIFY 8B

    // Write characteristics — Mode 2 protocol upload, Mode 4 EDF request, OTA, calibration
    static let protocolUpload   = CBUUID(string: "4E455550-0008-1000-8000-00805F9B34FB") // WRITE (signed blob)
    static let edfRequest       = CBUUID(string: "4E455550-0009-1000-8000-00805F9B34FB") // WRITE (trigger EDF+ download)
    static let otaCommand       = CBUUID(string: "4E455550-000A-1000-8000-00805F9B34FB") // WRITE/NOTIFY
    static let otaStatus        = CBUUID(string: "4E455550-000B-1000-8000-00805F9B34FB") // NOTIFY
    static let calibrationCmd   = CBUUID(string: "4E455550-000C-1000-8000-00805F9B34FB") // WRITE
    static let zoneModuleStatus = CBUUID(string: "4E455550-000D-1000-8000-00805F9B34FB") // READ/NOTIFY 5B
    static let shdrUploadStatus = CBUUID(string: "4E455550-000E-1000-8000-00805F9B34FB") // NOTIFY

    // All characteristics for discovery
    static let all: [CBUUID] = [
        sessionState, sessionStatus, hrvCoherence, pacerPhase,
        impedanceResult, consumableStatus, protocolUpload, edfRequest,
        otaCommand, otaStatus, calibrationCmd, zoneModuleStatus, shdrUploadStatus
    ]
}

// MARK: - OTA command opcodes

enum OTAOpcode: UInt8 {
    case begin          = 0x01  // Begin OTA transfer — hub prepares Bank B
    case chunk          = 0x02  // Firmware chunk (payload follows opcode)
    case commit         = 0x03  // All chunks sent; hub verifies Ed25519, swaps bank flag
    case abort          = 0x04  // Cancel in-flight OTA
    case safetyMCUBegin = 0x10  // Safety MCU firmware update — requires explicit user confirmation
    case safetyMCUChunk = 0x11
    case safetyMCUCommit = 0x12
}

// MARK: - Calibration command opcodes

enum CalibrationOpcode: UInt8 {
    case impedanceCheck   = 0x01  // Trigger ADS1299 electrode impedance check
    case ads1299SelfCal   = 0x02  // Trigger ADS1299 internal reference self-calibration
    case zoneIDRefresh    = 0x03  // Re-read all ZONE_ID resistors
    case fluxgateNullZero = 0x04  // Fluxgate zero-field nulling
}

// MARK: - Characteristic parsers (little-endian, matching hub firmware layout)

struct GATTParser {

    /// SESSION_STATE: uint32 — Unix epoch milliseconds
    static func parseSessionState(_ data: Data) -> UInt32? {
        guard data.count >= 4 else { return nil }
        return data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 0, as: UInt32.self) }
    }

    /// SESSION_STATUS: uint8 protocolID + uint8 statusFlags
    static func parseSessionStatus(_ data: Data) -> (protocolID: UInt8, status: SessionStatus)? {
        guard data.count >= 2 else { return nil }
        let pid = data[0]
        guard let status = SessionStatus(rawValue: data[1]) else { return nil }
        return (pid, status)
    }

    /// HRV_COHERENCE: uint16 coherence×100 + uint16 RMSSD ms
    static func parseHRVCoherence(_ data: Data) -> HRVData? {
        guard data.count >= 4 else { return nil }
        let cohRaw = data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 0, as: UInt16.self) }
        let rmssd  = data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 2, as: UInt16.self) }
        return HRVData(coherenceScore: Float(cohRaw) / 100.0, rmssdMilliseconds: rmssd)
    }

    /// PACER_PHASE: uint8 phase + uint8 elapsed%
    static func parsePacerPhase(_ data: Data) -> (phase: PacerPhase, percent: UInt8)? {
        guard data.count >= 2 else { return nil }
        guard let phase = PacerPhase(rawValue: data[0]) else { return nil }
        return (phase, data[1])
    }

    /// IMPEDANCE_RESULT: uint16 bitmask (bit n = electrode n passed)
    static func parseImpedanceResult(_ data: Data) -> UInt16? {
        guard data.count >= 2 else { return nil }
        return data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 0, as: UInt16.self) }
    }

    /// CONSUMABLE_STATUS: 4 × uint16 session counts (intranasal, hydrogel, VNS, audio)
    static func parseConsumableStatus(_ data: Data) -> [UInt16]? {
        guard data.count >= 8 else { return nil }
        return (0..<4).map { i in
            data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: i * 2, as: UInt16.self) }
        }
    }

    /// ZONE_MODULE_STATUS: 5 bytes — one uint8 per zone slot (0=absent, 1–5=zone ID confirmed)
    static func parseZoneModuleStatus(_ data: Data) -> [UInt8]? {
        guard data.count >= 5 else { return nil }
        return (0..<5).map { data[$0] }
    }

    /// OTA_STATUS: uint8 phase + uint8 progressPercent + uint16 errorCode
    static func parseOTAStatus(_ data: Data) -> OTAStatusPacket? {
        guard data.count >= 4 else { return nil }
        let phase    = data[0]
        let progress = data[1]
        let errCode  = data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 2, as: UInt16.self) }
        return OTAStatusPacket(phaseRaw: phase, progressPercent: progress, errorCode: errCode)
    }
}

struct OTAStatusPacket {
    let phaseRaw: UInt8
    let progressPercent: UInt8
    let errorCode: UInt16
    var isError: Bool { errorCode != 0 }
}

// (Duplicate GATTParser struct removed — canonical definition is above.)
