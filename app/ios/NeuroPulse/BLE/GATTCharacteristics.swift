import CoreBluetooth

// UUIDs are placeholders — replace at firmware BLE implementation stage (OI-WA-03).
// Format: 128-bit custom service / characteristic UUIDs.

enum NPUUID {
    static let service          = CBUUID(string: "4E455550-0001-1000-8000-00805F9B34FB")

    // Characteristics — match NP-APP-ROADMAP-001 §5
    static let sessionState     = CBUUID(string: "4E455550-0002-1000-8000-00805F9B34FB") // NOTIFY 4B
    static let sessionStatus    = CBUUID(string: "4E455550-0003-1000-8000-00805F9B34FB") // NOTIFY 2B
    static let hrvCoherence     = CBUUID(string: "4E455550-0004-1000-8000-00805F9B34FB") // NOTIFY 4B
    static let pacerPhase       = CBUUID(string: "4E455550-0005-1000-8000-00805F9B34FB") // NOTIFY 2B
    static let impedanceResult  = CBUUID(string: "4E455550-0006-1000-8000-00805F9B34FB") // NOTIFY 2B
    static let consumableStatus = CBUUID(string: "4E455550-0007-1000-8000-00805F9B34FB") // READ/NOTIFY 8B
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
}
