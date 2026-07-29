import Foundation

// Zone module state — socket-keyed, variable length.
//
// The helmet interior is tiled with a single universal 40 mm hexagonal module
// SKU across ~80 sockets out of a 128-socket addressing domain (NP-HEX-ZM-001
// §3.4). Modules are type-agnostic: any tile type fits any socket, and the hub
// auto-inventories what is actually plugged. So the app cannot know in advance
// how many sockets exist, which are filled, or what a given socket is called.
//
// This replaces the fixed five-slot model that came from the retired ZONE_ID
// resistor-ladder architecture (five position-unique SKUs, `[UInt8]` of length
// 5, hardcoded anatomical labels). That model could represent five of eighty
// sockets and named all five wrongly.
//
// ── Anatomy is not stored here ───────────────────────────────────────────────
//
// The socket lattice is PROVISIONAL and has already been re-cut twice (30 → 78
// → 80 sockets). The device holds the authoritative geometry table, so lobe and
// side arrive on the wire with each record rather than from an app-side copy
// that would drift silently at the next re-cut.

/// Tile type as reported by the hub. Raw values are the firmware wire values
/// (`np_zn_module_type_t`) and are frozen once shipped — append only.
/// Cases are deliberately not named `none`: `x?.type == .none` would resolve to
/// `Optional.none` (nil) rather than the case, which is a silent-wrong-answer
/// bug at every optional-chained comparison site.
enum ZoneModuleType: UInt8 {
    case absent    = 0    // socket empty
    case pbmBase   = 1    // T1-A — 660/808 nm base PBM tile
    case eeg       = 2    // T1-B — dual-rated electrode + reduced PBM
    case pbm1064   = 3    // T1-C — 1064 nm smart PBM tile
    case pbm1170   = 4    // T2-D — 1170 nm deep-PBM laser tile
    case unknown   = 255  // seated but not identified

    /// Localised name, or nil when the hub cannot name the type. A nil here is
    /// honest: the retired ZONE_ID detector reports socket position but cannot
    /// distinguish a base tile from an electrode tile, so callers omit the type
    /// rather than assert one.
    var displayName: String? {
        switch self {
        case .absent:  return nil
        case .pbmBase: return String(localized: "ZONE_MODULE_TYPE_PBM_BASE")
        case .eeg:     return String(localized: "ZONE_MODULE_TYPE_EEG")
        case .pbm1064: return String(localized: "ZONE_MODULE_TYPE_PBM_1064")
        case .pbm1170: return String(localized: "ZONE_MODULE_TYPE_PBM_1170")
        case .unknown: return nil
        }
    }
}

/// Cortical lobe of a socket, from the hub's geometry table (`np_zn_lobe_t`).
enum ZoneLobe: UInt8 {
    /// Hub reports no lobe for this socket. Named `unspecified`, not `none`,
    /// for the same Optional-collision reason as ZoneModuleType.
    case unspecified = 0
    case frontal   = 1
    case temporal  = 2
    case parietal  = 3
    case occipital = 4
}

/// Hemisphere of a socket (`np_zn_side_t`).
///
/// `.midline` is a real position, not "unspecified": a midline socket straddles
/// the hemispheres and belongs to BOTH the Left and Right zone of its lobe
/// (see the membership rule in `np_module_map.h`).
enum ZoneSide: UInt8 {
    case midline = 0
    case left    = 1
    case right   = 2
}

/// One socket's reported state.
struct ZoneModuleStatus: Identifiable, Equatable {

    /// 1-based socket id, matching NPPS zone files, `hardware/np_socket_map.json`
    /// and the generated app-side socket map. Firmware ids are 0-based; the
    /// conversion happens at the firmware encoder boundary.
    let socketID: UInt8

    let moduleType: ZoneModuleType
    let lobe: ZoneLobe
    let side: ZoneSide

    /// A module is seated and identified. Never true when `hasFault` is true —
    /// firmware clears presence on fault so an unidentified module is never
    /// treated as usable by a placement gate.
    let isPresent: Bool

    /// The module failed identification (unrecognised type, bad contact, failed
    /// debounce).
    let hasFault: Bool

    var id: UInt8 { socketID }

    /// Anatomical name of the socket's position, e.g. "Frontal left".
    /// Empty when the hub reports no lobe for this socket.
    var anatomicalLabel: String {
        let lobeName: String
        switch lobe {
        case .unspecified: return ""
        case .frontal:   lobeName = String(localized: "ZONE_LOBE_FRONTAL")
        case .temporal:  lobeName = String(localized: "ZONE_LOBE_TEMPORAL")
        case .parietal:  lobeName = String(localized: "ZONE_LOBE_PARIETAL")
        case .occipital: lobeName = String(localized: "ZONE_LOBE_OCCIPITAL")
        }
        let sideName: String
        switch side {
        case .midline: sideName = String(localized: "ZONE_SIDE_MIDLINE")
        case .left:    sideName = String(localized: "ZONE_SIDE_LEFT")
        case .right:   sideName = String(localized: "ZONE_SIDE_RIGHT")
        }
        return String(format: String(localized: "ZONE_POSITION_FORMAT"), lobeName, sideName)
    }

    /// Full label for the setup list: "Socket 12 — Frontal left".
    var displayName: String {
        let position = anatomicalLabel
        if position.isEmpty {
            return String(format: String(localized: "ZONE_SOCKET_LABEL"), Int(socketID))
        }
        return String(format: String(localized: "ZONE_SOCKET_POSITION_LABEL"),
                      Int(socketID), position)
    }

    /// Sentence the app speaks when this socket's module is seated.
    /// Names the type only when the hub could identify one.
    var spokenConfirmation: String {
        if let type = moduleType.displayName {
            return String(format: String(localized: "ZONE_ANNOUNCE_WITH_TYPE"),
                          displayName, type)
        }
        return String(format: String(localized: "ZONE_ANNOUNCE_NO_TYPE"), displayName)
    }
}

/// The live socket map: a sparse, variable-length collection keyed by socket id.
///
/// Sparse by design — the hub reports the sockets it knows about, which is not
/// necessarily a contiguous run and is certainly not a fixed count.
struct ZoneModuleConfiguration: Equatable {

    /// Socket id → state. Only sockets the hub has reported appear.
    private(set) var sockets: [UInt8: ZoneModuleStatus]

    init(sockets: [UInt8: ZoneModuleStatus] = [:]) {
        self.sockets = sockets
    }

    /// Sockets in ascending id order — the order the setup list renders.
    var orderedSockets: [ZoneModuleStatus] {
        sockets.keys.sorted().compactMap { sockets[$0] }
    }

    var presentSockets: [ZoneModuleStatus] { orderedSockets.filter(\.isPresent) }
    var faultedSockets: [ZoneModuleStatus] { orderedSockets.filter(\.hasFault) }

    /// Sockets the hub has reported as empty. Distinct from "sockets that exist"
    /// — the hub may not have reported every socket yet.
    var emptySockets: [ZoneModuleStatus] {
        orderedSockets.filter { !$0.isPresent && !$0.hasFault }
    }

    var isEmpty: Bool { sockets.isEmpty }

    func status(forSocket id: UInt8) -> ZoneModuleStatus? { sockets[id] }

    func isPresent(socket id: UInt8) -> Bool { sockets[id]?.isPresent ?? false }

    /// Apply a decoded frame.
    ///
    /// A snapshot REPLACES the map — it is a complete inventory claim, so a
    /// socket absent from it is a socket the hub no longer reports. A delta
    /// updates only the sockets it names and leaves the rest untouched.
    mutating func apply(_ frame: ZoneModuleFrame) {
        if frame.isSnapshot {
            sockets = [:]
        }
        for record in frame.records {
            sockets[record.socketID] = record
        }
    }

    mutating func removeAll() { sockets = [:] }
}
