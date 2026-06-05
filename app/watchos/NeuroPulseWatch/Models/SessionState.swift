import Foundation

// Shared session state model — mirrored from iOS target.
// Must be kept in sync with app/ios/NeuroPulse/Models/SessionState.swift.
// WatchConnectivity dictionary keys must match on both sides.

enum SessionStatus: UInt8 {
    case idle       = 0
    case running    = 1
    case paused     = 2
    case completed  = 3
}

enum PacerPhase: UInt8 {
    case inhale = 0
    case exhale = 1
}

struct HRVData: Equatable {
    var coherenceScore: Float       // 0.0–10.0
    var rmssdMilliseconds: UInt16
}

struct SessionState {
    var epoch: UInt32               // Unix ms from hub SESSION_STATE characteristic
    var protocolID: UInt8
    var status: SessionStatus
    var hrv: HRVData?
    var pacerPhase: PacerPhase
    var pacerElapsedPercent: UInt8  // 0–100, fraction of current breath phase elapsed
    var impedancePassFlags: UInt16  // bitmask, bit n = electrode n passed
    var consumableSessionCounts: [UInt16] // 4 values: intranasal/hydrogel/VNS/audio

    static let empty = SessionState(
        epoch: 0,
        protocolID: 0,
        status: .idle,
        hrv: nil,
        pacerPhase: .inhale,
        pacerElapsedPercent: 0,
        impedancePassFlags: 0,
        consumableSessionCounts: [0, 0, 0, 0]
    )
}

// Keys used in WatchConnectivity message dictionaries.
enum WCKey {
    static let epoch             = "ep"
    static let protocolID        = "pid"
    static let status            = "st"
    static let coherenceX100     = "coh"
    static let rmssd             = "rmssd"
    static let pacerPhase        = "pp"
    static let pacerPercent      = "pct"
    static let impedanceFlags    = "imp"
    static let consumableCounts  = "con"
}

extension SessionState {
    static func from(wcMessage msg: [String: Any]) -> SessionState? {
        guard
            let epoch      = msg[WCKey.epoch]      as? Int,
            let pid        = msg[WCKey.protocolID]  as? Int,
            let statusRaw  = msg[WCKey.status]      as? Int,
            let status     = SessionStatus(rawValue: UInt8(statusRaw)),
            let phaseRaw   = msg[WCKey.pacerPhase]  as? Int,
            let phase      = PacerPhase(rawValue: UInt8(phaseRaw)),
            let pct        = msg[WCKey.pacerPercent]   as? Int,
            let impFlags   = msg[WCKey.impedanceFlags] as? Int,
            let conRaw     = msg[WCKey.consumableCounts] as? [Int]
        else { return nil }

        var hrv: HRVData?
        if let cohRaw = msg[WCKey.coherenceX100] as? Int,
           let rmssd  = msg[WCKey.rmssd] as? Int {
            hrv = HRVData(coherenceScore: Float(cohRaw) / 100.0, rmssdMilliseconds: UInt16(rmssd))
        }

        return SessionState(
            epoch: UInt32(epoch),
            protocolID: UInt8(pid),
            status: status,
            hrv: hrv,
            pacerPhase: phase,
            pacerElapsedPercent: UInt8(pct),
            impedancePassFlags: UInt16(impFlags),
            consumableSessionCounts: conRaw.prefix(4).map { UInt16($0) }
        )
    }
}
