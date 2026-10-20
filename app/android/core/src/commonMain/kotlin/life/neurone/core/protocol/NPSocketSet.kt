package life.neurone.core.protocol

/**
 * A socket set is a SET: validated against the lattice, deduplicated, in ascending id order, so two equal sets compare
 * equal and a listing is stable. Port of `common/lib/socketSet.ts`. A socket id is a plain decimal integer that names a
 * real socket on this helmet, or it is not a socket id; nothing here guesses what the author meant.
 */
class NPSocketSetResult(
    /** Valid ids, deduplicated, ascending. */
    val sockets: List<Int>,
    /** The inputs that are not usable socket ids, as written, in first-seen order. */
    val invalid: List<String>,
)

object NPSocketSet {

    private val DECIMAL = Regex("^[0-9]+$")

    /** Canonicalise [tokens] (`"4"`, `"21"`, …) into a set. */
    fun of(tokens: Iterable<String>): NPSocketSetResult {
        val seen = LinkedHashSet<Int>()
        val invalid = LinkedHashSet<String>()
        for (raw in tokens) {
            val t = raw.trim()
            val n = if (DECIMAL.matches(t)) t.toIntOrNull() else null
            if (n != null && SocketLattice.isValid(n)) seen.add(n) else if (t.isNotEmpty()) invalid.add(t)
        }
        return NPSocketSetResult(seen.sorted(), invalid.toList())
    }

    /** Every run of digits in [text] is one token, so `"4, 6 21"` and `"4-6"` are read the way the web editor reads them. */
    fun parse(text: String): NPSocketSetResult = of(Regex("[0-9]+").findAll(text).map { it.value }.toList())

    /** Union of any number of lists: deduplicated and sorted. The call to reach for whenever more than one zone contributes sockets. */
    fun union(vararg lists: Iterable<Int>): List<Int> = lists.flatMap { it }.filter(SocketLattice::isValid).toSortedSet().toList()
}

/**
 * Which zone each socket is drawn in on the map: the SMALLEST zone containing it (the most specific true statement about
 * it; every socket is in "All"), ties broken by name so the map does not reshuffle between reloads. Port of the web
 * `groupByZone`. A zone's colour is its position in name order, so the legend and the tiles agree.
 */
class NPZoneGrouping(zones: Collection<NPZoneDefinition>) {
    val byName: List<NPZoneDefinition> = zones.sortedBy { it.name }

    /** Zone name → its index in name order (the caller maps it to a colour). */
    val colorIndexOf: Map<String, Int> = byName.mapIndexed { i, z -> z.name to i }.toMap()

    /** Most specific zone containing each socket. */
    val primaryOf: Map<Int, String>

    /** Every zone containing each socket, smallest first. */
    val membershipOf: Map<Int, List<String>>

    /** Zones that are the primary zone of at least one socket, in name order. */
    val legend: List<String>

    init {
        val bySpecificity = byName.sortedWith(compareBy({ it.sockets.size }, { it.name }))
        val primary = LinkedHashMap<Int, String>()
        val membership = LinkedHashMap<Int, MutableList<String>>()
        for (zone in bySpecificity) for (id in zone.sockets) {
            primary.putIfAbsent(id, zone.name)
            membership.getOrPut(id) { mutableListOf() }.add(zone.name)
        }
        primaryOf = primary
        membershipOf = membership
        val claimed = primary.values.toSet()
        legend = byName.map { it.name }.filter { it in claimed }
    }
}
