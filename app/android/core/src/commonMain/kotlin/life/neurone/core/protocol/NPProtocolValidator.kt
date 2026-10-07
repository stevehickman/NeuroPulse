package life.neurone.core.protocol

import kotlin.concurrent.Volatile
import life.neurone.core.npps.NppsCore
import life.neurone.core.npps.toNppsCoreItem
import life.neurone.core.npps.toNppsCoreLimitsJson
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray
import kotlinx.serialization.json.putJsonObject
import life.neurone.core.common.UUID

// =============================================================================
// Protocol validation on Android: the shared NPPS core (common/npps-core, OI-NPPS-CORE-01), through JNI.
//
// This used to be a 770-line hand-written validator, one of three that drifted (the web one lacked the
// session-duration, dose and interlock checks this one had; this one lacked the layer-scale and TMS-with-
// electrical-stimulation checks the web one had). There is one validator now, and it is in Rust, and it has
// every check any of them had. What is left here is the mapping of these models to the core's shape and the
// words: the core returns locale keys and arguments, never text, and [NPValidationText] resolves them.
// =============================================================================

/** Which tier a limit came from. Display and attribution only. */
enum class NPLimitSource(val wire: String, val textKey: String) {
    HARDWARE("hardware", "VALIDATE_SOURCE_HARDWARE"),
    GLOBAL("global", "VALIDATE_SOURCE_GLOBAL"),
    HELMET("helmet", "VALIDATE_SOURCE_HELMET"),
    INDIVIDUAL("individual", "VALIDATE_SOURCE_INDIVIDUAL");

    companion object {
        fun fromWire(w: String): NPLimitSource = entries.firstOrNull { it.wire == w } ?: GLOBAL
    }
}

enum class NPValidationSeverity { ERROR, WARNING }

data class NPValidationIssue(
    val severity: NPValidationSeverity,
    val modality: NPModalityType?,          // null = protocol-level issue
    val parameterKey: String,               // camelCase field name, e.g. "intensityMilliamps"
    val parameterDisplayName: String,       // e.g. "Intensity"
    val actualValueDescription: String,     // e.g. "1.5 mA"
    val limitValueDescription: String,      // e.g. "1 mA (Hardware Limit)"
    val limitSource: NPLimitSource,
    val message: String,
    val id: UUID = UUID.randomUUID(),
)

/**
 * One axis of a protocol's dose in the zone model's caution band (docs/reference/safety-zones.md). The protocol
 * compiles only when the author has acknowledged every one, by [ackId]; the id names the dose, so a changed dose is a
 * new caution. Read from the core's `zones`, never computed here.
 */
data class NPZoneCaution(
    val ackId: String,
    val blockIndex: Int,
    val modality: NPModalityType?,
    /** Locale key of the axis name (`ZONE_AXIS_…`). */
    val axisNameKey: String,
    val unit: String,
    val value: Double,
    /** Where the caution band starts. */
    val caution: Double,
) {
    companion object {
        /** The cautions in a validator reply's `zones` array. */
        fun parse(zones: JsonArray?): List<NPZoneCaution> = zones.orEmpty().flatMap { b ->
            val block = b.jsonObject
            val modality = (block["modality"] as? JsonPrimitive)?.content?.let { m -> NPModalityType.entries.firstOrNull { it.rawValue == m } }
            val index = (block["index"] as? JsonPrimitive)?.content?.toIntOrNull() ?: 0
            (block["axes"] as? JsonArray).orEmpty().mapNotNull { a ->
                val axis = a.jsonObject
                val ack = (axis["ackId"] as? JsonPrimitive)?.takeIf { it.isString }?.content
                if ((axis["zone"] as? JsonPrimitive)?.content != "caution" || ack == null) return@mapNotNull null
                NPZoneCaution(
                    ackId = ack,
                    blockIndex = index,
                    modality = modality,
                    axisNameKey = axis["nameKey"]!!.jsonPrimitive.content,
                    unit = axis["unit"]!!.jsonPrimitive.content,
                    value = axis["value"]!!.jsonPrimitive.content.toDouble(),
                    caution = axis["caution"]!!.jsonPrimitive.content.toDouble(),
                )
            }
        }
    }
}

class NPValidationResult {
    val issues: MutableList<NPValidationIssue> = mutableListOf()

    /** What the author must acknowledge before this protocol compiles; empty outside the caution zone. */
    var zoneCautions: List<NPZoneCaution> = emptyList()

    val isValid: Boolean get() = issues.none { it.severity == NPValidationSeverity.ERROR }
    val hasWarnings: Boolean get() = issues.any { it.severity == NPValidationSeverity.WARNING }
    val errors: List<NPValidationIssue> get() = issues.filter { it.severity == NPValidationSeverity.ERROR }
    val warnings: List<NPValidationIssue> get() = issues.filter { it.severity == NPValidationSeverity.WARNING }
}

/** Total session seconds if the protocol uses a duration timing mode; null for interval-count. */
val NPProtocolDefinition.totalDurationSeconds: Int?
    get() = when (val t = timingMode) {
        is NPTimingMode.Duration -> t.seconds
        is NPTimingMode.IntervalCount -> null
    }

/**
 * How a locale key becomes text. `:core` is a pure-JVM module (ISC-2..4) and cannot reach Android resources,
 * so the app installs a resolver at startup (`NeurOneApplication`): `{(key, args) -> getString(...)}`. With none
 * installed a message renders as its key, which is what the web's `t()` does for a key it does not know, so a
 * missing resolver is visible rather than silent. Tests install one over `locales/en.json`.
 */
object NPValidationText {
    @Volatile private var resolver: ((String, List<String>) -> String?)? = null

    /** Whether a resolver is installed; a host that has its own installs it first and the shared one stands aside. */
    val isInstalled: Boolean get() = resolver != null

    /** Install the resolver: the text of [key] with positional [args], or null when the key is unknown. */
    fun use(resolver: ((String, List<String>) -> String?)?) {
        this.resolver = resolver
    }

    /** A core message as text: a plain string stays, a `{key, args}` object is resolved (its args are messages too). */
    fun render(m: JsonElement): String {
        if (m is JsonPrimitive) return m.content
        val o = m.jsonObject
        val key = o["key"]!!.jsonPrimitive.content
        val args = (o["args"] as? JsonArray)?.map { render(it) } ?: emptyList()
        return resolver?.invoke(key, args) ?: if (args.isEmpty()) key else "$key(${args.joinToString(", ")})"
    }
}

class NPProtocolValidator(
    private val resolvedLimits: NPLimitsSet,
    /** Where each configured limit came from (`{"besTacs":{"maxFrequencyHz":"helmet"}}`); a limit with no entry takes the set's level. */
    private val limitSources: JsonObject? = null,
) {

    // MARK: Public interface

    fun validate(definition: NPProtocolDefinition): NPValidationResult =
        run(NPProtocolEntry.Single(definition), null)

    /**
     * Validate any entry. Composites resolve their member protocols via [resolveSingle]; a name it cannot resolve
     * is an error naming the layer.
     */
    fun validate(
        entry: NPProtocolEntry,
        resolveSingle: (String) -> NPProtocolDefinition? = { null },
    ): NPValidationResult = when (entry) {
        is NPProtocolEntry.Single -> run(entry, null)
        is NPProtocolEntry.Composite -> run(entry, entry.composite.layers.mapNotNull { resolveSingle(it.protocolName) }
            .distinctBy { it.name }.map { NPProtocolEntry.Single(it) })
        is NPProtocolEntry.Limits,
        is NPProtocolEntry.Zone,
        is NPProtocolEntry.Condition -> NPValidationResult()
    }

    private fun run(entry: NPProtocolEntry, library: List<NPProtocolEntry>?): NPValidationResult {
        val request = buildJsonObject {
            put("entry", entry.toNppsCoreItem())
            put("limits", resolvedLimits.toNppsCoreLimitsJson())
            if (library == null) put("allProtocols", JsonNull) else put("allProtocols", JsonArray(library.map { it.toNppsCoreItem() }))
            // The namespace the protocol resolves its named zones against (a zone the library does not define is an error).
            putJsonObject("zones") {
                for ((name, z) in NPZoneRegistry.zones) putJsonArray(name) { z.sockets.forEach { add(JsonPrimitive(it)) } }
            }
            put("limitSources", limitSources ?: JsonNull)
        }
        val out = NppsCore.validate(request)
        val result = NPValidationResult()
        result.zoneCautions = NPZoneCaution.parse(out["zones"] as? JsonArray)
        for (i in out["issues"]!!.jsonArray) {
            val o = i.jsonObject
            val source = NPLimitSource.fromWire(o["limitSource"]!!.jsonPrimitive.content)
            val limit = NPValidationText.render(o["limitValueDescription"]!!)
            result.issues.add(
                NPValidationIssue(
                    severity = if (o["severity"]!!.jsonPrimitive.content == "error") NPValidationSeverity.ERROR else NPValidationSeverity.WARNING,
                    modality = (o["modality"] as? JsonPrimitive)?.content?.let { m -> NPModalityType.entries.firstOrNull { it.rawValue == m } },
                    parameterKey = o["parameterKey"]!!.jsonPrimitive.content,
                    parameterDisplayName = NPValidationText.render(o["parameterName"]!!),
                    actualValueDescription = NPValidationText.render(o["actualValueDescription"]!!),
                    limitValueDescription = NPValidationText.render(buildJsonObject {
                        put("key", "VALIDATE_LIMIT_WITH_SOURCE")
                        putJsonArray("args") { add(JsonPrimitive(limit)); add(buildJsonObject { put("key", source.textKey) }) }
                    }),
                    limitSource = source,
                    message = NPValidationText.render(o["message"]!!),
                ),
            )
        }
        return result
    }
}
