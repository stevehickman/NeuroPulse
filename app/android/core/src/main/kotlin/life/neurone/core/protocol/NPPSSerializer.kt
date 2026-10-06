package life.neurone.core.protocol

import life.neurone.core.npps.NppsCore
import life.neurone.core.npps.toNppsCoreItem

// =============================================================================
// NPPS serialization on Android: the shared NPPS core (common/npps-core, OI-NPPS-CORE-01), through JNI.
//
// This used to be a 490-line hand-written writer, one of four that drifted from the parser: it wrote `1h`
// (a unit the lexer does not have), left a limits-set name unescaped, and wrote limit values with units the
// parser reads differently. There is one serializer now, and it is in Rust: what the text looks like, and what
// it refuses to write, is decided there and is the same on every runtime. What is left here is the mapping of
// these models to the core's shape (npps/NppsCoreMapping.kt).
//
// A model that cannot be written (a zone whose socket list holds an id that is not a socket) is refused with
// [IllegalArgumentException] carrying the core's message, rather than serialized into a file the parser rejects.
// =============================================================================

class NPPSSerializer {

    fun serialize(entry: NPProtocolEntry): String = NppsCore.serialize(listOf(entry.toNppsCoreItem()))

    /** Entries as one `.npps` file: blocks separated by a blank line. */
    fun serialize(entries: List<NPProtocolEntry>): String = NppsCore.serialize(entries.map { it.toNppsCoreItem() })

    fun serializeLimits(limits: NPLimitsSet): String = serialize(NPProtocolEntry.Limits(limits))
}

// MARK: - Round-trip ---------------------------------------------------------

/** Parse then write: what the app would save for this text. */
fun nppsRoundTrip(text: String): String = NPPSSerializer().serialize(NPPSParser.parse(text))
