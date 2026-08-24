package life.neurone.core.protocol

/**
 * The zone definitions in force.
 *
 * **A `zone` block in a `.npps` file is the only ORIGIN of a zone**
 * (NP-NPPS-REF-001 §8). This object is a lookup over the loaded namespace, not a
 * store of zone content: it holds no socket list of its own, it derives nothing
 * from geometry, and a zone that no loaded file defines simply does not exist
 * here.
 *
 * It replaces the generated `SocketZones` table. That table did not violate the
 * origin rule — it was generated from `00-zones.npps` and regenerated with it,
 * which §8 explicitly allows. It was replaced because generation happens at
 * BUILD time and so could only ever contain the SHIPPED zones: a zone a user
 * defined in their own `.npps` file was absent from it, so user-defined zones
 * (§8 *User-defined zones*) did not resolve on this platform at all. Reading the
 * loaded namespace covers both on one path. The generated artefact now carries
 * the physical lattice only ([SocketLattice]), which is hardware and not a zone.
 */
object NPZoneRegistry {

    /**
     * The namespace zones resolve against. Starts as the bundled library and is
     * replaced by [use] once user files are loaded.
     */
    @Volatile
    private var namespace: NPNamespace? = null

    private val current: NPNamespace
        get() = namespace ?: NPBundledProtocols.namespace.also { namespace = it }

    /**
     * Point the registry at a namespace built from every loaded `.npps` file —
     * bundled and user-authored. Call after loading user files so a user-defined
     * zone resolves exactly like a shipped one.
     */
    fun use(namespace: NPNamespace) {
        this.namespace = namespace
    }

    /** Zone definitions by name. */
    val zones: Map<String, NPZoneDefinition> get() = current.zones

    /**
     * The 1-based socket ids [zone] contains, or null when no loaded file defines
     * a zone by that name — an authoring error in the protocol, not a device
     * state.
     */
    fun sockets(zone: String): List<Int>? = current.zones[zone]?.sockets

    /**
     * Every defined zone name, in the order the loaded files declare them.
     *
     * Declaration order rather than map order, so a UI list built from this does
     * not reshuffle between launches. A name left unbound by a
     * duplicate-definition collision (§1.6) is not listed — it is not defined.
     */
    val zoneNames: List<String>
        get() {
            val ns = current
            return ns.entries
                .filterIsInstance<NPProtocolEntry.Zone>()
                .map { it.zone.name }
                .distinct()
                .filter { ns.zones.containsKey(it) }
        }

    /**
     * Every zone containing [socketId], smallest first.
     *
     * Sockets belong to several zones by design: a midline socket is listed in
     * both hemisphere zones of its region (`00-zones.npps`). Smallest first makes
     * the head of the list the most specific description available. Equal-sized
     * zones keep declaration order, so the answer is stable.
     */
    fun zones(socketId: Int): List<String> {
        val ns = current
        return zoneNames
            .filter { ns.zones[it]?.sockets?.contains(socketId) == true }
            .sortedBy { ns.zones[it]?.sockets?.size ?: 0 }
    }

    /**
     * Smallest zone containing [socketId] — the most specific description
     * available, and what the app speaks. Null if no loaded zone lists it.
     */
    fun primaryZone(socketId: Int): String? = zones(socketId).firstOrNull()
}
