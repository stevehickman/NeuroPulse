import Foundation

// Consumable kinds and their session-count thresholds.
// Session counts come from the hub CONSUMABLE_STATUS GATT characteristic.
// Thresholds are locked per CLAUDE.md §2.3.

enum ConsumableKind: Int, CaseIterable, Identifiable {
    case intranasalSleeves = 0
    case electrodeHydrogel = 1
    case vnsPads           = 2
    case audioCupFoam      = 3

    var id: Int { rawValue }

    var displayName: String {
        switch self {
        case .intranasalSleeves: return "Intranasal Sleeves"
        case .electrodeHydrogel: return "Electrode Hydrogel Tips"
        case .vnsPads:           return "VNS Clip Pads"
        case .audioCupFoam:      return "Audio Cup Foam"
        }
    }

    var sessionLimit: Int {
        switch self {
        case .intranasalSleeves: return 1           // single use
        case .electrodeHydrogel: return 45          // midpoint of 30–60 range
        case .vnsPads:           return 30          // midpoint of 20–40 range
        case .audioCupFoam:      return 150         // ~180 sessions in 6 months assuming daily use
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
        case .intranasalSleeves: return "$19 / 30-pack"
        case .electrodeHydrogel: return "$12–16 / 8-pack"
        case .vnsPads:           return "$8 / 2-pack"
        case .audioCupFoam:      return "$24 / set"
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
