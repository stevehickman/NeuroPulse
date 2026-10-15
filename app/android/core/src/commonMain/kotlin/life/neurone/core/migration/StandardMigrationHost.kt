package life.neurone.core.migration

import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.RandomSource
import life.neurone.core.data.hexToBytes
import life.neurone.core.data.toHex

/**
 * SWR-MG-06, SWR-MG-09: the old phone's clear steps that are the platform's own. The core runs the rest itself through the key
 * store (the last paired helmet id, every key the lists name, the cleared marker, and which steps are finished), so an app
 * writes only these three: `destroyEncryptionKey`, `overwriteAndDeleteFiles` and `forgetPairingRecord`. Returns true when the
 * step finished.
 */
interface ClearActions {
    suspend fun run(step: String): Boolean
}

/**
 * The [MigrationHost] for this app: the effects the driver ([MigrationFlow]) does not answer itself. These are randomness, the
 * items stored another way ([MigrationItemStore]), a route the app writes itself ([BackupRoute]) and the platform's clear steps
 * ([ClearActions]). A route that is an object store needs none of the route effects: pass it to [MigrationFlow.run] as `store`.
 */
class StandardMigrationHost(
    private val random: RandomSource,
    adapters: List<MigrationItemStore> = emptyList(),
    private val route: BackupRoute? = null,
    private val actions: ClearActions? = null,
) : MigrationHost {
    private val byItem = adapters.associateBy { it.item }

    /** The names of the items this host stores another way: the flow's `hostItems` param. */
    val hostItems: List<String> = byItem.keys.toList()

    private fun strings(a: JsonElement?): List<String> = a?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList()
    private fun typed(): BackupRoute = route ?: error("this host has no BackupRoute; the route is an object store the core drives")

    override suspend fun perform(effect: JsonObject): JsonObject = when (val type = effect["type"]!!.jsonPrimitive.content) {
        "random" -> {
            val n = effect["bytes"]!!.jsonPrimitive.content.toInt()
            val bytes = random.nextBytes(n)
            require(bytes.size == n) { "The random source gave ${bytes.size} bytes, not $n" }
            buildJsonObject { put("bytes", bytes.toHex()) }
        }
        "readItems" -> buildJsonObject {
            put("items", buildJsonObject {
                strings(effect["names"]).forEach { n -> byItem[n]?.export()?.let { put(n, it) } }
            })
        }
        "applyItem" -> {
            byItem[effect["name"]!!.jsonPrimitive.content]!!.import(effect["value"]!!)
            buildJsonObject { put("ok", true) }
        }
        "getBundle" -> buildJsonObject { put("bundle", typed().getBundle()?.let { JsonPrimitive(it.toHex()) } ?: JsonNull) }
        "putBundle" -> {
            typed().putBundle(effect["bundle"]!!.jsonPrimitive.content.hexToBytes())
            buildJsonObject { put("ok", true) }
        }
        "readClaim" -> buildJsonObject { put("claim", typed().readClaim() ?: JsonNull) }
        "createClaim" -> buildJsonObject { put("created", typed().createClaimIfAbsent(effect["claim"]!!.jsonObject)) }
        "writeConfirmation" -> {
            typed().writeConfirmation(effect["confirmation"]!!.jsonObject)
            buildJsonObject { put("ok", true) }
        }
        "readConfirmation" -> buildJsonObject { put("confirmation", typed().readConfirmation() ?: JsonNull) }
        "runClearStep" -> buildJsonObject { put("ok", actions!!.run(effect["step"]!!.jsonPrimitive.content)) }
        else -> error("unknown effect '$type'")
    }
}
