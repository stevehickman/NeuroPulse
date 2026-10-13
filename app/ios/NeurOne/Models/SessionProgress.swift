import Foundation

// SESSION_PROGRESS (SWR-SS-07, OI-UI-KMP-09 R6): where the helmet says a session is. The figures are the helmet's own
// clock, never the app's, so they are right across a pause, a ramp and a reconnect. Wire layout:
// firmware/hub_control/include/np_session_progress.h, 22 bytes little-endian. Session timing is UHDR-class once it is
// tied to a person (CLAUDE.md §5.1): display only, never persisted or sent to analytics.

/// The hub's session state (`np_session_state_t`).
enum HubSessionState: UInt8 {
    case idle = 0, loading, verifying, running, paused, stopping, complete, fault
}

/// The gating ramp of a pause or a resume (`np_ramp_phase_t`).
enum RampPhase: UInt8 {
    case none = 0
    /// A pause: stimulation is still on and falling.
    case down
    /// Paused, nothing ramping.
    case held
    /// Cervical VNS impedance and cardiac baseline checks, before its ramp.
    case precheck
    /// Ramping up after a start or a resume.
    case up
}

struct SessionProgress: Equatable {
    var state: HubSessionState
    /// Index of the highest-numbered running command of the signed descriptor, or nil when none runs.
    var activePart: Int?
    /// Bit n set = a running command drives modality type n (`np_hub_mod_type_t`).
    var activeModalityMask: UInt16
    var elapsedMs: UInt32
    var remainingMs: UInt32
    var totalMs: UInt32
    var rampPhase: RampPhase
    var rampPercent: Int
    /// Nil while it cannot be known (the cervical VNS checks).
    var rampRemainingMs: UInt32?

    static let length = 22

    /// A session is in progress, paused or not: the controls apply (SWR-SS-05).
    var isActive: Bool { state == .running || state == .paused || state == .stopping }
    var isPaused: Bool { state == .paused }

    /// Nil for a frame of the wrong length or with a state or ramp phase it does not know: no part of it is believed.
    init?(wire data: Data) {
        guard data.count == Self.length else { return nil }
        let bytes = [UInt8](data)
        func u32(_ offset: Int) -> UInt32 {
            (0..<4).reduce(UInt32(0)) { $0 | (UInt32(bytes[offset + $1]) << UInt32(8 * $1)) }
        }
        guard let state = HubSessionState(rawValue: bytes[0]), let ramp = RampPhase(rawValue: bytes[16]) else {
            return nil
        }
        self.state = state
        self.activePart = bytes[1] == 0xFF ? nil : Int(bytes[1])
        self.activeModalityMask = UInt16(bytes[2]) | (UInt16(bytes[3]) << 8)
        self.elapsedMs = u32(4)
        self.remainingMs = u32(8)
        self.totalMs = u32(12)
        self.rampPhase = ramp
        self.rampPercent = min(Int(bytes[17]), 100)
        let left = u32(18)
        self.rampRemainingMs = left == 0xFFFF_FFFF ? nil : left
    }

    /// A duration as a clock: `M:SS`, or `H:MM:SS` from an hour up. Digits only, so it needs no locale key.
    static func formatClock(_ ms: UInt32) -> String {
        let total = Int(ms) / 1000
        let h = total / 3600
        let m = (total % 3600) / 60
        let s = total % 60
        let ss = s < 10 ? "0\(s)" : "\(s)"
        if h > 0 { return "\(h):\(m < 10 ? "0\(m)" : "\(m)"):\(ss)" }
        return "\(m):\(ss)"
    }
}
