import Foundation

/// The zone definitions in force.
///
/// **A `zone` block in a `.npps` file is the only ORIGIN of a zone**
/// (NP-NPPS-REF-001 §8). This type is a lookup over the loaded namespace, not a
/// store of zone content: it holds no socket list of its own, it derives nothing
/// from geometry, and a zone that no loaded file defines simply does not exist
/// here.
///
/// It replaces the generated `SocketZones` table. That table did not violate the
/// origin rule — it was generated from `00-zones.npps` and regenerated with it,
/// which §8 explicitly allows. It was replaced because generation happens at
/// BUILD time and so could only ever contain the SHIPPED zones: a zone a user
/// defined in their own `.npps` file was absent from it, so user-defined zones
/// (§8 *User-defined zones*) did not resolve on this platform at all. Reading
/// the loaded namespace covers both on one path. The generated artefact now
/// carries the physical lattice only (`SocketLattice`), which is hardware and
/// not a zone.
enum NPZoneRegistry {

    /// The namespace zones resolve against. Starts as the bundled library and is
    /// replaced by ``use(_:)`` once user files are loaded.
    private static var namespace: NPNamespace = NPBundledProtocols.namespace

    /// Point the registry at a namespace built from every loaded `.npps` file —
    /// bundled and user-authored. Call after loading user files so a
    /// user-defined zone resolves exactly like a shipped one.
    static func use(_ namespace: NPNamespace) {
        self.namespace = namespace
    }

    /// Zone definitions by name.
    static var zones: [String: NPZoneDefinition] { namespace.zones }

    /// The 1-based socket ids a zone contains, or nil when no loaded file defines
    /// a zone by that name — an authoring error in the protocol, not a device
    /// state.
    static func sockets(forZone zoneName: String) -> [Int]? {
        namespace.zones[zoneName]?.sockets
    }

    /// Every defined zone name, in the order the loaded files declare them.
    ///
    /// Declaration order rather than dictionary order, so a UI list built from
    /// this does not reshuffle between launches. A name left unbound by a
    /// duplicate-definition collision (§1.6) is not listed — it is not defined.
    static var zoneNames: [String] {
        var seen = Set<String>()
        var names: [String] = []
        for entry in namespace.entries {
            guard case .zone(let z) = entry else { continue }
            guard namespace.zones[z.name] != nil, seen.insert(z.name).inserted else { continue }
            names.append(z.name)
        }
        return names
    }

    /// Every zone containing this socket, smallest first.
    ///
    /// Sockets belong to several zones by design: a midline socket is listed in
    /// both hemisphere zones of its region (`00-zones.npps`). Smallest first
    /// makes the head of the list the most specific description available.
    /// Equal-sized zones keep declaration order, so the answer is stable.
    static func zones(forSocket socketID: Int) -> [String] {
        let order = zoneNames
        return order
            .filter { namespace.zones[$0]?.sockets.contains(socketID) ?? false }
            .enumerated()
            .sorted { lhs, rhs in
                let l = namespace.zones[lhs.element]?.sockets.count ?? 0
                let r = namespace.zones[rhs.element]?.sockets.count ?? 0
                return l == r ? lhs.offset < rhs.offset : l < r
            }
            .map(\.element)
    }

    /// Smallest zone containing this socket — the most specific description
    /// available, and what the app speaks. nil if no loaded zone lists it.
    static func primaryZone(forSocket socketID: Int) -> String? {
        zones(forSocket: socketID).first
    }
}
