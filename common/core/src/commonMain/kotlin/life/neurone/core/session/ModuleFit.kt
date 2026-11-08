package life.neurone.core.session

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore
import life.neurone.core.platform.toHex

// Stage 2 of vetting a protocol (OI-HWFIT-01): can the compiled descriptor run on the modules fitted now?
// The rule is the shared core's (`fitCheck`, common/npps-core/src/fit.rs); this marshals the hub's tile types in and the
// shortfalls out and decides nothing. The answer is advice: the hub's placement check stays the authority.

/** An element type a socket needs and does not have, and the part numbers of the module designs that carry it. */
data class MissingElement(val element: String, val candidateParts: List<String>)

/** Sockets the protocol drives that fall short in the same way, so a person reads one line and not one per socket. */
data class ShortfallGroup(val sockets: List<Int>, val missing: List<MissingElement>)

object ModuleFit {
    /**
     * @param descriptor a compiled descriptor, signed or not
     * @param tileTypes socket id to the tile type the hub reported there (`ZoneModuleType.rawValue`), for the sockets that hold
     *   an identified module without a fault. A socket not listed is empty.
     * @return the sockets that fall short, grouped by what they lack (the core groups them); empty when the protocol fits
     */
    fun check(descriptor: ByteArray, tileTypes: Map<Int, Int>): List<ShortfallGroup> {
        val answer = NppsCore.library(
            buildJsonObject {
                put("op", "fitCheck")
                put("descriptorHex", descriptor.toHex())
                put("inventory", JsonObject(tileTypes.entries.associate { (socket, type) -> socket.toString() to JsonPrimitive(type) }))
            },
        )
        return answer["groups"]!!.jsonArray.map { g ->
            val o = g.jsonObject
            ShortfallGroup(
                sockets = o["sockets"]!!.jsonArray.map { it.jsonPrimitive.int },
                missing = o["missing"]!!.jsonArray.map { m ->
                    MissingElement(
                        m.jsonObject["element"]!!.jsonPrimitive.content,
                        m.jsonObject["candidateParts"]!!.jsonArray.map { it.jsonPrimitive.content },
                    )
                },
            )
        }
    }
}
