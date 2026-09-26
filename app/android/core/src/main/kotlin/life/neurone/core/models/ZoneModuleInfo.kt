package life.neurone.core.models

// Port of iOS ZoneModuleInfo.swift (app/ios/NeurOne/Models/ZoneModuleInfo.swift).
//
// Zone module state — socket-keyed, variable length, split static from dynamic.
//
// The helmet interior is tiled with a single universal 40 mm hexagonal module SKU across
// ~80 sockets out of a 128-socket addressing domain (NP-HEX-ZM-001 §3.4). Modules are
// type-agnostic: any tile type fits any socket, and the hub auto-inventories what is plugged.
// So the app cannot know in advance how many sockets exist, which are filled, or what a
// given socket is called.
//
// This replaces the fixed five-slot model from the retired ZONE_ID resistor-ladder
// architecture (a `List<Int>` of length 5, one byte per slot). That model could represent
// five of eighty sockets (NP-HFE-002 OI-HFE2-02).
//
//   SocketMap        — read ONCE at link. Where every socket physically is, plus whether the
//                      shell wires it. The helmet is the single source of truth; the app never
//                      keeps its own copy of the lattice.
//   ZoneModuleStatus / ZoneModuleConfiguration — what actually changes: socket id, module
//                      type, flags. Three bytes on the wire per change.
//
// There is no lobe and no side anywhere here (ZONE-1). A socket's anatomical meaning is its
// zone membership, authored in `.npps` files and resolved app-side via NPZoneRegistry.
//
// SHDR class: socket occupancy, module type and module health are component facts
// (firmware/zone_announce/include/np_zone_notify.h). Nothing here is UHDR.

/**
 * Tile type as reported by the hub. Raw values are the firmware wire values
 * (`np_zn_module_type_t`) and are frozen once shipped — append only.
 */
enum class ZoneModuleType(val rawValue: Int) {
    ABSENT(0),     // socket empty
    PBM_BASE(1),   // T1-A — 660/808 nm base PBM tile
    EEG(2),        // T1-B — dual-rated electrode + reduced PBM
    PBM_1064(3),   // T1-C — 1064 nm smart PBM tile
    PBM_1170(4),   // T2-D — 1170 nm deep-PBM laser tile
    UNKNOWN(255);  // seated but not identified

    companion object {
        /** An unrecognised wire value is UNKNOWN, never a guessed type. */
        fun from(raw: Int): ZoneModuleType = entries.firstOrNull { it.rawValue == raw } ?: UNKNOWN
    }
}

/**
 * A socket's position in 3-space, millimetres. Aircraft body axes, origin at the centre of the
 * shell: +x forward (toward the face), +y right (toward the right ear), +z down (toward the
 * neck). The vault sits above the origin, so `downMm` is negative for every socket.
 */
data class SocketPosition(val forwardMm: Int, val rightMm: Int, val downMm: Int)

/**
 * One socket's PERMANENT description, from the helmet's geometry table. Read once at link
 * time; never carried in a change notification.
 */
data class SocketDescriptor(
    /** 1-based socket id, matching NPPS zone `sockets:` lists and hardware/np_socket_map.json. */
    val socketId: Int,
    /** Provisional until the lattice is registered against shell CAD (NP-HEX-ZM-001 REG-1). */
    val position: SocketPosition,
    /** This shell physically wires this socket. */
    val isWiredInShell: Boolean,
)

/**
 * The helmet's permanent socket geometry, as read at link time. Deliberately never persisted:
 * a firmware update can re-cut the lattice, and a cached copy would then place sockets wrongly.
 */
data class SocketMap(val descriptors: Map<Int, SocketDescriptor> = emptyMap()) {

    val isEmpty: Boolean get() = descriptors.isEmpty()
    val count: Int get() = descriptors.size

    /** All sockets in ascending id order. */
    val orderedSockets: List<SocketDescriptor>
        get() = descriptors.keys.sorted().mapNotNull { descriptors[it] }

    fun descriptor(socketId: Int): SocketDescriptor? = descriptors[socketId]

    /** A map is always a complete statement of the helmet's geometry — it replaces wholesale. */
    fun applying(frame: SocketMapFrame): SocketMap =
        SocketMap(frame.descriptors.associateBy { it.socketId })

    companion object {
        val EMPTY = SocketMap()
    }
}

/** One socket's changeable state. No anatomy — that is in the SocketMap. */
data class ZoneModuleStatus(
    /** 1-based socket id (see SocketDescriptor.socketId). */
    val socketId: Int,
    val moduleType: ZoneModuleType,
    /**
     * A module is seated and identified. Never true when [hasFault] is true — an unidentified
     * module is never treated as usable by a placement gate.
     */
    val isPresent: Boolean,
    /** The module failed identification (unrecognised type, bad contact, failed debounce). */
    val hasFault: Boolean,
)

/**
 * The live occupancy map: sparse, variable-length, keyed by socket id. Sparse by design — the
 * hub reports the sockets it knows about, which is not necessarily a contiguous run and is
 * certainly not a fixed count.
 */
data class ZoneModuleConfiguration(val sockets: Map<Int, ZoneModuleStatus> = emptyMap()) {

    /** Sockets in ascending id order — the order the setup list renders. */
    val orderedSockets: List<ZoneModuleStatus>
        get() = sockets.keys.sorted().mapNotNull { sockets[it] }

    val presentSockets: List<ZoneModuleStatus> get() = orderedSockets.filter { it.isPresent }
    val faultedSockets: List<ZoneModuleStatus> get() = orderedSockets.filter { it.hasFault }

    /** Sockets the hub has reported as empty — not "sockets that exist". */
    val emptySockets: List<ZoneModuleStatus>
        get() = orderedSockets.filter { !it.isPresent && !it.hasFault }

    val isEmpty: Boolean get() = sockets.isEmpty()

    fun status(socketId: Int): ZoneModuleStatus? = sockets[socketId]

    fun isPresent(socketId: Int): Boolean = sockets[socketId]?.isPresent ?: false

    /**
     * Apply a decoded status frame. A snapshot REPLACES the map — it is a complete inventory
     * claim, so a socket absent from it is a socket the hub no longer reports. A delta updates
     * only the sockets it names and leaves the rest untouched.
     */
    fun applying(frame: ZoneModuleFrame): ZoneModuleConfiguration {
        val next = if (frame.isSnapshot) mutableMapOf() else sockets.toMutableMap()
        for (record in frame.records) next[record.socketId] = record
        return ZoneModuleConfiguration(next)
    }

    companion object {
        val EMPTY = ZoneModuleConfiguration()
    }
}
