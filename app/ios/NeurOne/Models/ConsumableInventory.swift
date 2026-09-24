import Foundation

// Consumable kinds and their session-count thresholds.
// Session counts come from the hub CONSUMABLE_STATUS GATT characteristic.
// Each sessionLimit must equal the threshold in the Trigger cell of
// docs/reference/commercial-model.md §2.3 — scripts/check-consumable-triggers.ts enforces it.

enum ConsumableKind: Int, CaseIterable, Identifiable {
    case intranasalSleeves = 0
    case electrodeHydrogel = 1
    case vnsPads           = 2
    case audioCupFoam      = 3

    var id: Int { rawValue }

    var displayName: String {
        switch self {
        case .intranasalSleeves: return String(localized: "CONSUMABLE_INTRANASAL_NAME")
        case .electrodeHydrogel: return String(localized: "CONSUMABLE_HYDROGEL_NAME")
        case .vnsPads:           return String(localized: "CONSUMABLE_VNS_NAME")
        case .audioCupFoam:      return String(localized: "CONSUMABLE_AUDIO_NAME")
        }
    }

    var sessionLimit: Int {
        switch self {
        case .intranasalSleeves: return 1           // single use
        // UNVALIDATED PLACEHOLDER — OI-ACC-09. Midpoint of a 30–60 range with no stated source,
        // and no document names the mechanism this count drives. The intended trigger is an
        // impedance trend (a condition measurement), which nothing implements yet.
        case .electrodeHydrogel: return 45
        // UNVALIDATED PLACEHOLDER — OI-VNSCLIP-07. Exposure count, mechanism = electrochemical
        // degradation from VNS current; 30 is the midpoint of a quoted 20–40, not a derived life.
        case .vnsPads:           return 30
        // UNVALIDATED PLACEHOLDER — OI-ACC-04. Exposure count, mechanism = compression set under
        // wear. The value is NOT derived from that mechanism: it was back-derived from the retired
        // "6–12 months" calendar interval (OI-ACC-02), which CLAUDE.md §2.3 forbids and which this
        // device cannot implement anyway — no VBAT rail, so wall time dies on every disconnect.
        // Kept rather than re-derived, because re-deriving from a void derivation is not an
        // improvement. Same defect class as NP-FW-EMMC-002 §G.2's accelerometer thresholds; it is
        // superseded by a condition measurement — loss of seal read by an in-cup noise-cancelling
        // mic (principal, 2026-09-23; OI-AUDIOHW-08) — not by another arithmetic pass.
        // Comfort/hygiene replacement is the user's call and gets no prompt.
        case .audioCupFoam:      return 150
        }
    }

    // Remind when this many sessions remain in the current consumable unit.
    var lowThreshold: Int {
        switch self {
        case .intranasalSleeves: return 0   // single-use: alert only when at/past limit, not before
        case .electrodeHydrogel: return 8
        case .vnsPads:           return 4
        case .audioCupFoam:      return 20
        }
    }

    var packDisplayPrice: String {
        switch self {
        case .intranasalSleeves: return String(localized: "CONSUMABLE_INTRANASAL_PRICE")
        case .electrodeHydrogel: return String(localized: "CONSUMABLE_HYDROGEL_PRICE")
        case .vnsPads:           return String(localized: "CONSUMABLE_VNS_PRICE")
        case .audioCupFoam:      return String(localized: "CONSUMABLE_AUDIO_PRICE")
        }
    }

    var reminderPriority: ReminderPriority {
        switch self {
        case .intranasalSleeves: return .safetyBlocking   // hygiene — single use, blocks session
        case .electrodeHydrogel: return .performanceCritical
        case .vnsPads:           return .performanceCritical
        case .audioCupFoam:      return .comfortLongevity
        }
    }

    // Stable URL slug for the shop product page. Using a slug instead of rawValue
    // integer ensures URLs survive future enum reordering or new kind insertion.
    var shopSlug: String {
        switch self {
        case .intranasalSleeves: return "intranasal-sleeves"
        case .electrodeHydrogel: return "electrode-hydrogel-tips"
        case .vnsPads:           return "vns-clip-pads"
        case .audioCupFoam:      return "audio-cup-foam"
        }
    }
}

enum ReminderPriority {
    case safetyBlocking      // cannot be dismissed — blocks session start
    case performanceCritical // snooze max 3×
    case comfortLongevity    // snooze max 5×
}

struct ConsumableState: Identifiable {
    let kind: ConsumableKind
    var sessionCount: Int           // sessions since last replacement (from hub SHDR)
    var snoozeCount: Int = 0

    var id: Int { kind.rawValue }
    var sessionsRemaining: Int { max(0, kind.sessionLimit - sessionCount) }
    var isLow: Bool { sessionsRemaining <= kind.lowThreshold }
    var isExceeded: Bool { sessionCount >= kind.sessionLimit }
    var maxSnooze: Int { kind.reminderPriority == .comfortLongevity ? 5 : 3 }
    var canSnooze: Bool { kind.reminderPriority != .safetyBlocking && snoozeCount < maxSnooze }

    mutating func snooze() {
        guard canSnooze else { return }
        snoozeCount += 1
    }

    mutating func resetAfterReplacement() {
        sessionCount = 0
        snoozeCount = 0
    }
}

struct ConsumableInventory {
    var states: [ConsumableState] = ConsumableKind.allCases.map { ConsumableState(kind: $0, sessionCount: 0) }

    mutating func update(fromGATTCounts counts: [UInt16]) {
        for kind in ConsumableKind.allCases {
            guard kind.rawValue < counts.count else { continue }
            states[kind.rawValue].sessionCount = Int(counts[kind.rawValue])
        }
    }

    var activeReminders: [ConsumableState] {
        states.filter { $0.isLow }
    }

    var blockingReminders: [ConsumableState] {
        states.filter { $0.kind.reminderPriority == .safetyBlocking && $0.isExceeded }
    }
}
