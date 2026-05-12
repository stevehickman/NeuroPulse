import Foundation

// Zone module identification — maps ZONE_ID resistor values to human-readable names.
// Zone IDs correspond to NP-HW-FPC-001 Rev D pin 18 resistor ladder:
// ZM-01=10kΩ, ZM-02=22kΩ, ZM-03=47kΩ, ZM-04=100kΩ, ZM-05=220kΩ

enum ZoneID: UInt8, CaseIterable {
    case absent = 0
    case frontalLeft  = 1   // ZM-01
    case frontalRight = 2   // ZM-02
    case parietalLeft = 3   // ZM-03
    case parietalRight = 4  // ZM-04
    case occipital    = 5   // ZM-05

    var displayName: String {
        switch self {
        case .absent:        return "Empty"
        case .frontalLeft:   return "Frontal Left (ZM-01)"
        case .frontalRight:  return "Frontal Right (ZM-02)"
        case .parietalLeft:  return "Parietal Left (ZM-03)"
        case .parietalRight: return "Parietal Right (ZM-04)"
        case .occipital:     return "Occipital (ZM-05)"
        }
    }

    var slotIndex: Int { Int(rawValue) - 1 }

    var isPresent: Bool { self != .absent }
}

struct ZoneModuleStatus: Identifiable {
    let slotIndex: Int       // 0-based slot position
    let zoneID: ZoneID

    var id: Int { slotIndex }

    var isPresent: Bool { zoneID.isPresent }
    var displayName: String { zoneID.displayName }

    // Slot positions (0–4) correspond to headset anatomy
    var anatomicalLabel: String {
        switch slotIndex {
        case 0: return "Zone 1 — Frontal Left"
        case 1: return "Zone 2 — Frontal Right"
        case 2: return "Zone 3 — Parietal Left"
        case 3: return "Zone 4 — Parietal Right"
        case 4: return "Zone 5 — Occipital"
        default: return "Zone \(slotIndex + 1)"
        }
    }
}

struct ZoneModuleConfiguration {
    var slots: [ZoneModuleStatus]

    init(rawSlotData: [UInt8] = [0, 0, 0, 0, 0]) {
        slots = rawSlotData.prefix(5).enumerated().map { idx, raw in
            ZoneModuleStatus(slotIndex: idx, zoneID: ZoneID(rawValue: raw) ?? .absent)
        }
    }

    var presentSlots: [ZoneModuleStatus] { slots.filter(\.isPresent) }
    var allPresent: Bool { slots.allSatisfy(\.isPresent) }
    var nonePresent: Bool { slots.allSatisfy { !$0.isPresent } }

    mutating func update(rawSlotData: [UInt8]) {
        for (idx, raw) in rawSlotData.prefix(5).enumerated() {
            slots[idx] = ZoneModuleStatus(slotIndex: idx, zoneID: ZoneID(rawValue: raw) ?? .absent)
        }
    }
}
