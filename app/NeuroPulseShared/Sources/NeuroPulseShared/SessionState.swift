import Foundation

// Shared session state model — consumed by the iOS app and watchOS companion app.
// This is the single source of truth. Do not duplicate this file in either target.

public enum SessionStatus: UInt8, Sendable {
    case idle       = 0
    case running    = 1
    case paused     = 2
    case completed  = 3
}

public enum PacerPhase: UInt8, Sendable {
    case inhale = 0
    case exhale = 1
}

public struct HRVData: Equatable, Sendable {
    // coherenceScore: derived from GATT HRV_COHERENCE characteristic (uint16 / 100).
    public var coherenceScore: Float        // 0.0–10.0
    public var rmssdMilliseconds: UInt16

    public init(coherenceScore: Float, rmssdMilliseconds: UInt16) {
        self.coherenceScore = coherenceScore
        self.rmssdMilliseconds = rmssdMilliseconds
    }
}

public struct SessionState: Sendable {
    public var epoch: UInt32                // Unix ms from hub SESSION_STATE characteristic
    public var protocolID: UInt8
    public var status: SessionStatus
    public var hrv: HRVData?
    public var pacerPhase: PacerPhase
    public var pacerElapsedPercent: UInt8   // 0–100, fraction of current breath phase elapsed
    public var impedancePassFlags: UInt16   // bitmask, bit n = electrode n passed
    public var consumableSessionCounts: [UInt16] // 4 values: intranasal/hydrogel/VNS/audio

    public init(
        epoch: UInt32,
        protocolID: UInt8,
        status: SessionStatus,
        hrv: HRVData?,
        pacerPhase: PacerPhase,
        pacerElapsedPercent: UInt8,
        impedancePassFlags: UInt16,
        consumableSessionCounts: [UInt16]
    ) {
        self.epoch = epoch
        self.protocolID = protocolID
        self.status = status
        self.hrv = hrv
        self.pacerPhase = pacerPhase
        self.pacerElapsedPercent = pacerElapsedPercent
        self.impedancePassFlags = impedancePassFlags
        self.consumableSessionCounts = consumableSessionCounts
    }

    public static let empty = SessionState(
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

// WatchConnectivity dictionary keys — must match on both sides of the bridge.
// epoch is intentionally excluded: it is UHDR-class data (session timestamp,
// CLAUDE.md §5.1) and is not needed for Watch display — elapsed time is driven
// by a local Timer on the Watch side. (NP-PRIV-ANALYSIS-002 MEDIUM-08)
public enum WCKey {
    public static let protocolID        = "pid"
    public static let status            = "st"
    public static let coherenceX100     = "coh"
    public static let rmssd             = "rmssd"
    public static let pacerPhase        = "pp"
    public static let pacerPercent      = "pct"
    public static let impedanceFlags    = "imp"
    public static let consumableCounts  = "con"
}

extension SessionState {
    public func toWCMessage() -> [String: Any] {
        var msg: [String: Any] = [
            WCKey.protocolID:       Int(protocolID),
            WCKey.status:           Int(status.rawValue),
            WCKey.pacerPhase:       Int(pacerPhase.rawValue),
            WCKey.pacerPercent:     Int(pacerElapsedPercent),
            WCKey.impedanceFlags:   Int(impedancePassFlags),
            WCKey.consumableCounts: consumableSessionCounts.map { Int($0) },
        ]
        if let h = hrv {
            msg[WCKey.coherenceX100] = Int((h.coherenceScore * 100).rounded())
            msg[WCKey.rmssd]         = Int(h.rmssdMilliseconds)
        }
        return msg
    }

    public static func from(wcMessage msg: [String: Any]) -> SessionState? {
        guard
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
            epoch: 0,   // not transmitted over WC bridge (UHDR boundary)
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
