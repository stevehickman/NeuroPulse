import Foundation

// Cervical VNS faults that happened while the app was not connected, and the hub's re-enable
// state — the hub's CVNS_FAULT_STATUS characteristic (NP-SW-FAULTMSG-001 P3/P4).
//
// In Mode 3 the only fault signal is the red status LED (CLAUDE.md §4.7). When the phone next
// connects, this is what tells the wearer which fault it was and what to do — before the app
// offers another cervical session. It gates only what the app offers: the safety MCU owns the
// enable line and keeps a cardiac cutoff latched across power cycles on its own (P1, CLAUDE.md
// §4.2), so nothing here can release stimulation.
//
// UHDR-class — fault kinds from the user's own session records (docs/reference/
// data-architecture-detail.md §5.1). No heart-rate values and no timestamps cross: a device
// session counter orders the records. Held in memory; only the acknowledgement point
// (CervicalFaultLedger) is persisted, on the phone.
//
// Wire (4 + 8n bytes, n = 0…4):
//   byte 0 — version, 0x01
//   byte 1 — hub re-enable state (np_cvns_reenable_state_t): 0 idle, 1 lockout, 2 awaiting
//            confirmation, 3 impedance check, 4 re-enabling
//   byte 2 — n, the number of records that follow (newest last)
//   byte 3 — flags (from the safety MCU's np_safety_nv_report_t): bit 0 = the ACTIVE user's
//            cervical VNS is withheld; bit 1 = SOMEONE on this device has an outstanding
//            cardiac cutoff (the blanket warning); bits 2–7 reserved, 0.  Which user is never sent.
//   per record: uint32 LE device session counter · uint8 fault kind (np_cvns_fault_reason_t,
//               1…5) · uint8 failing-pad side mask (bit 0 left, bit 1 right; pad faults only)
//               · uint16 reserved, 0
// The records are the ACTIVE user's own (per-user cardiac scope, principal 2026-09-22).
// Anything else — wrong version or length, a reserved bit, an unknown kind or state — is
// discarded whole, never half-believed.
struct CervicalFaultRecord: Equatable, Identifiable {

    enum Kind: UInt8 {
        case heartRateChange = 1   // NP_CVNS_FAULT_HR_CHANGE — the cardiac cutoff
        case signalLost      = 2   // NP_CVNS_FAULT_DATA_LOSS — ear-clip R-peak signal lost
        case watchdog        = 3   // NP_CVNS_FAULT_WATCHDOG
        case padContact      = 4   // NP_CVNS_FAULT_IMPEDANCE
        case safetyMCU       = 5   // NP_CVNS_FAULT_SAFETY_MCU
    }

    let sessionCounter: UInt32
    let kind: Kind
    /// Where the failing pad is, for a pad-contact fault; empty otherwise.
    let padSides: Set<CervicalPadStatus.NeckSide>

    var id: UInt32 { sessionCounter }

    /// Which locale key explains this fault to the wearer.
    var messageKey: String {
        switch kind {
        case .heartRateChange:        return "CVNS_FAULT_HR_CHANGE"
        case .signalLost:             return "CVNS_FAULT_SIGNAL_LOST"
        case .watchdog, .safetyMCU:   return "CVNS_FAULT_DEVICE"
        case .padContact:
            switch (padSides.contains(.left), padSides.contains(.right)) {
            case (true, false):  return "CVNS_FAULT_PAD_LEFT"
            case (false, true):  return "CVNS_FAULT_PAD_RIGHT"
            default:             return "CVNS_FAULT_PAD_BOTH"
            }
        }
    }

    var message: String { String(localized: String.LocalizationValue(stringLiteral: messageKey)) }
}

struct CervicalFaultStatus: Equatable {

    enum ReenableState: UInt8 {
        case idle          = 0
        case lockout       = 1
        case awaitConfirm  = 2
        case impedance     = 3
        case reenabling    = 4
    }

    static let version: UInt8 = 0x01
    static let maxRecords = 4
    static let headerLength = 4
    static let recordLength = 8

    let reenableState: ReenableState
    let records: [CervicalFaultRecord]
    /// The active user's cervical VNS is withheld by the safety MCU.
    let userBlocked: Bool
    /// Someone on this device — the active user or another — has an outstanding cardiac cutoff.
    let outstanding: Bool

    /// Another person has an outstanding cutoff and the active user does not: the case in which
    /// switching profiles could otherwise be used to get around a block.
    var outstandingForAnotherUser: Bool { outstanding && !userBlocked }

    init(reenableState: ReenableState, records: [CervicalFaultRecord],
         userBlocked: Bool = false, outstanding: Bool = false) {
        self.reenableState = reenableState
        self.records = records
        self.userBlocked = userBlocked
        self.outstanding = outstanding
    }

    init?(wire data: Data) {
        let b = [UInt8](data)
        guard b.count >= Self.headerLength,
              b[0] == Self.version,
              let state = ReenableState(rawValue: b[1]),
              Int(b[2]) <= Self.maxRecords,
              b[3] & ~UInt8(0x03) == 0,
              b.count == Self.headerLength + Int(b[2]) * Self.recordLength else { return nil }

        var records: [CervicalFaultRecord] = []
        for i in 0..<Int(b[2]) {
            let o = Self.headerLength + i * Self.recordLength
            let counter = UInt32(b[o]) | UInt32(b[o + 1]) << 8 | UInt32(b[o + 2]) << 16 | UInt32(b[o + 3]) << 24
            guard let kind = CervicalFaultRecord.Kind(rawValue: b[o + 4]),
                  b[o + 5] & ~UInt8(0x03) == 0,
                  b[o + 6] == 0, b[o + 7] == 0 else { return nil }
            var sides = Set<CervicalPadStatus.NeckSide>()
            if b[o + 5] & 0x01 != 0 { sides.insert(.left) }
            if b[o + 5] & 0x02 != 0 { sides.insert(.right) }
            // A side mask belongs to a pad fault only; on any other kind it is malformed.
            guard kind == .padContact || sides.isEmpty else { return nil }
            records.append(CervicalFaultRecord(sessionCounter: counter, kind: kind, padSides: sides))
        }
        self.reenableState = state
        self.records = records
        self.userBlocked = b[3] & 0x01 != 0
        self.outstanding = b[3] & 0x02 != 0
    }
}

/// The opaque tag that tells the device which person is using it (per-user cardiac scope,
/// principal 2026-09-22).  Derived from the app's individual profile id — a random UUID — so it
/// carries no name or identity.  The device's two reserved values are never produced.
enum ActiveUserTag {
    static func from(profileId: UUID?) -> UInt32? {
        guard let id = profileId else { return nil }
        let u = id.uuid
        var tag = UInt32(u.0) | UInt32(u.1) << 8 | UInt32(u.2) << 16 | UInt32(u.3) << 24
        if tag == 0 { tag = 1 }
        if tag == 0xFFFF_FFFF { tag = 0xFFFF_FFFE }
        return tag
    }
}

/// Which offline faults the wearer has already read. Persists one number — the device session
/// counter of the newest acknowledged fault — so a fault is explained once, not on every connect.
///
/// A counter LOWER than the stored one means a different or reset device; the ledger then treats
/// every record as unread rather than hiding faults behind a stale number.
struct CervicalFaultLedger: Equatable {
    var lastAcknowledgedSession: UInt32?

    func unacknowledged(_ records: [CervicalFaultRecord]) -> [CervicalFaultRecord] {
        guard let last = lastAcknowledgedSession else { return records }
        if let newest = records.map(\.sessionCounter).max(), newest < last { return records }
        return records.filter { $0.sessionCounter > last }
    }

    /// True while an unread cardiac cutoff exists: the app then refuses to upload a protocol
    /// containing cervical VNS until the wearer has read why stimulation stopped.
    func blocksCervicalRestart(_ records: [CervicalFaultRecord]) -> Bool {
        unacknowledged(records).contains { $0.kind == .heartRateChange }
    }

    mutating func acknowledge(_ records: [CervicalFaultRecord]) {
        guard let newest = records.map(\.sessionCounter).max() else { return }
        lastAcknowledgedSession = newest
    }
}
