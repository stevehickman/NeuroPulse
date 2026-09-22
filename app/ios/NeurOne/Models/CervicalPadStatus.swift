import Foundation

// Cervical VNS gel pad contact result — the hub's CVNS_PAD_STATUS characteristic.
//
// The pad is replaced when it FAILS, not after a count (docs/reference/commercial-model.md §2.3,
// OI-ACC-03): it is guaranteed for one use and may be reused while it still makes contact.
// Failure is detected on the hub — the pre-enable per-electrode impedance check
// (NP-HW-CVNS-001 REQ-CVNS-06, ≤ 5.0 kΩ at 1 kHz, on which the safety MCU refuses enable) or
// mid-session open detection (REQ-CVNS-08, 500 ms). This type only WORDS that refusal for the
// wearer, naming WHERE the failing pad is (OI-ACC-07). It gates nothing: the safety MCU already
// has, and no app path may stand in for it (CLAUDE.md §4.2).
//
// UHDR-class — derived from tissue impedance (docs/reference/data-architecture-detail.md §5.1).
// Held in memory for display only: never persisted, never uploaded, cleared on disconnect.
//
// Wire (4 bytes):
//   byte 0 — failed-electrode bitmask: bit 0 = electrode 1, bit 1 = electrode 2; bits 2–7
//            reserved, must be 0. 0x00 = both pads passed, which clears any alert.
//   byte 1 — which check: 0x00 = pre-enable (REQ-CVNS-06), 0x01 = mid-session (REQ-CVNS-08).
//   byte 2 — where electrode 1's pad is: 0x01 = left side of the neck, 0x02 = right side.
//   byte 3 — where electrode 2's pad is, same codes.
// The HUB supplies the location, from the session's montage (np_cvns_session_config_t
// electrode_config: bilateral puts one pad on each side, unilateral puts both on one side) and
// its own electrode-to-side wiring. The app never maps an electrode number to a side itself.
// The hub notifies when the result changes within a session attempt and at each new attempt.
struct CervicalPadStatus: Equatable, Identifiable {

    enum Check: UInt8 {
        case preEnable  = 0x00
        case midSession = 0x01
    }

    enum NeckSide: UInt8, CaseIterable {
        case left  = 0x01
        case right = 0x02
    }

    /// NP_CVNS_ELECTRODE_COUNT — the assembly checks two pads every session.
    static let padCount = 2

    /// Where each failing pad is, one entry per failing electrode, in electrode order.
    /// Empty = both passed. Two entries may name the same side (a unilateral montage).
    let failedPadSides: [NeckSide]
    let check: Check

    var id: String { failedPadSides.map { String($0.rawValue) }.joined() + "-\(check.rawValue)" }

    var hasFailure: Bool { !failedPadSides.isEmpty }

    /// The sides the neck diagram lights.
    var failingSides: Set<NeckSide> { Set(failedPadSides) }

    /// Decodes the 4-byte frame. Returns nil for anything not exactly well-formed — a reserved
    /// bit, an unknown check or an unknown location — so a malformed frame is discarded, never
    /// half-believed. A location the app cannot name is malformed, not "somewhere".
    init?(wire data: Data) {
        guard data.count >= 4 else { return nil }
        let bytes = [UInt8](data.prefix(4))
        let mask = bytes[0]
        guard mask & ~UInt8((1 << Self.padCount) - 1) == 0,
              let check = Check(rawValue: bytes[1]),
              let side1 = NeckSide(rawValue: bytes[2]),
              let side2 = NeckSide(rawValue: bytes[3]) else { return nil }
        let sides = [side1, side2]
        self.failedPadSides = (0..<Self.padCount).filter { mask & (1 << $0) != 0 }.map { sides[$0] }
        self.check = check
    }

    init(failedPadSides: [NeckSide], check: Check) {
        self.failedPadSides = failedPadSides
        self.check = check
    }

    /// Which locale key words this status. nil when both pads passed. A single failure names
    /// its side — "a gel pad on the left side", which stays true when a unilateral montage puts
    /// both pads there.
    var messageKey: String? {
        guard let first = failedPadSides.first else { return nil }
        let both = failedPadSides.count >= Self.padCount
        switch (check, both, first) {
        case (.preEnable, true, _):       return "CVNS_PAD_ALERT_PRE_BOTH"
        case (.midSession, true, _):      return "CVNS_PAD_ALERT_MID_BOTH"
        case (.preEnable, false, .left):  return "CVNS_PAD_ALERT_PRE_LEFT"
        case (.preEnable, false, .right): return "CVNS_PAD_ALERT_PRE_RIGHT"
        case (.midSession, false, .left): return "CVNS_PAD_ALERT_MID_LEFT"
        case (.midSession, false, .right): return "CVNS_PAD_ALERT_MID_RIGHT"
        }
    }

    /// The instruction shown to the wearer. nil when both pads passed.
    var alertMessage: String? {
        messageKey.map { String(localized: String.LocalizationValue(stringLiteral: $0)) }
    }
}
