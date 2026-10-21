import Foundation

// Shared session state model — consumed by the iOS app.

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
    public var epoch: UInt32                // Hub session clock (truncated UInt32, used for offset sync only)
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
