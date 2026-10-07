package life.neurone.core.npps

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray
import kotlinx.serialization.json.putJsonObject

/**
 * The shared NPPS core (common/npps-core, OI-NPPS-CORE-01) as Android sees it: one lexer,
 * parser and hub-descriptor compiler written once in Rust and called through JNI
 * (common/npps-jni). This object does marshalling and nothing else; what an input means is
 * decided in the core, so it means the same here as on every other runtime.
 *
 * A refusal is thrown as [IllegalArgumentException] carrying the core's message, which is the
 * message the web parser or compiler gives for the same input.
 *
 * The native library `neurone_npps_jni` must be loadable: on a JVM test host through
 * `java.library.path` (the :core test task builds it), on a device from the APK's jniLibs.
 */
object NppsCore {
    init {
        System.loadLibrary("neurone_npps_jni")
    }

    @JvmStatic private external fun nativeParse(source: String): String

    @JvmStatic private external fun nativeCompile(request: String): ByteArray

    @JvmStatic private external fun nativeNamespace(request: String): String

    @JvmStatic private external fun nativeSerialize(request: String): String

    @JvmStatic private external fun nativeValidate(request: String): String

    @JvmStatic private external fun nativeResolveLimits(request: String): String

    /**
     * Parse NPPS text into everything the file declares:
     * `{"entries":[{"kind":"single","protocol":{…}} | {"kind":"composite","composite":{…}}],
     * "zones":[…], "conditions":[…], "wavelengthRules":[…], "limits":[…]}`. Ids and timestamps are
     * left out; the caller that builds a model supplies them.
     */
    fun parse(source: String): JsonObject = Json.parseToJsonElement(nativeParse(source)) as JsonObject

    /**
     * Fold several [parse] results, in load order, into one namespace and check its references:
     * `{"entries", "zones", "conditions", "errors", "referenceErrors"}`. A name two files define
     * is left undefined and reported in `errors`.
     */
    fun namespace(files: List<JsonObject>): JsonObject {
        val request = buildJsonObject { put("files", JsonArray(files)) }
        return Json.parseToJsonElement(nativeNamespace(request.toString())) as JsonObject
    }

    /**
     * Write models as `.npps` text. Each item is one of the shapes [parse] returns (`{"kind":"single",
     * "protocol":…}`, `{"kind":"composite","composite":…}`, `{"kind":"zone","zone":…}`, `{"kind":"condition",
     * "condition":…}`, `{"kind":"wavelengthRules",…}`, `{"kind":"limits","limits":…}`); items are separated by a
     * blank line. A model that cannot be written (a zone with an id that is not a socket) is refused with
     * [IllegalArgumentException] rather than written into a file the parser would reject.
     */
    fun serialize(items: List<JsonObject>): String {
        val request = buildJsonObject { put("items", JsonArray(items)) }
        return nativeSerialize(request.toString())
    }

    /**
     * Validate an entry against resolved limits: `{"issues":[…], "isValid", "hasWarnings"}`. The core returns
     * locale keys and arguments, never text; see `NPValidationText` for how they become words. The request
     * is `{"entry":…, "limits":…, "allProtocols":[…]|null, "zones":{…}|null, "limitSources":{…}|null}`.
     */
    fun validate(request: JsonObject): JsonObject = Json.parseToJsonElement(nativeValidate(request.toString())) as JsonObject

    /**
     * Resolve three limit sets (null for a tier that does not exist), most specific first, field by field:
     * `{"limits":{"level":"global", <modality blocks>}, "sources":{<modalityProperty>:{<limitField>:tier}}}`. `sources` is
     * what the validator takes as `limitSources`.
     */
    fun resolveLimits(global: JsonObject?, helmet: JsonObject?, individual: JsonObject?): JsonObject {
        val request = buildJsonObject {
            put("global", global ?: JsonNull)
            put("helmet", helmet ?: JsonNull)
            put("individual", individual ?: JsonNull)
        }
        return Json.parseToJsonElement(nativeResolveLimits(request.toString())) as JsonObject
    }

    /** What a compile needs that is not in the protocol. Everything non-deterministic is an input. */
    class CompileOptions(
        /** Zone name → its 1-based socket ids. */
        val zones: Map<String, List<Int>>? = null,
        val clinicianSockets: List<Int>? = null,
        val deviceSerial: ByteArray? = null,
        val nowUnix: Long,
        val sessionUuid: ByteArray,
        /** Channel windows overriding the shipped defaults; null keeps the defaults. */
        val wavelengthRules: JsonObject? = null,
        /**
         * Ids of the zone-model cautions the author acknowledged ([life.neurone.core.protocol.NPZoneCaution.ackId]); a
         * protocol in the caution zone compiles only when every one of its cautions is here
         * (docs/reference/safety-zones.md).
         */
        val acknowledgedCautions: List<String> = emptyList(),
    )

    /**
     * Compile a protocol (`{"timingMode":…,"modalities":[…]}`, the shape [parse] returns under
     * `protocol`) into the NP-FW-HUB-001 §4 descriptor. The 64-byte signature slot at the end is
     * zeroed: sign the region before it and write the signature in.
     */
    fun compile(protocol: JsonObject, options: CompileOptions): ByteArray {
        require(options.sessionUuid.size == 16) { "sessionUuid must be 16 bytes" }
        val request = buildJsonObject {
            put("def", protocol)
            if (options.zones == null) put("zones", JsonNull) else putJsonObject("zones") {
                for ((name, ids) in options.zones) putJsonArray(name) { ids.forEach { add(JsonPrimitive(it)) } }
            }
            if (options.clinicianSockets == null) put("clinicianSockets", JsonNull)
            else putJsonArray("clinicianSockets") { options.clinicianSockets.forEach { add(JsonPrimitive(it)) } }
            if (options.deviceSerial == null) put("deviceSerialHex", JsonNull)
            else put("deviceSerialHex", hex(options.deviceSerial))
            put("nowUnix", options.nowUnix)
            put("sessionUuidHex", hex(options.sessionUuid))
            put("wavelengthRules", options.wavelengthRules ?: JsonNull)
            putJsonArray("acknowledgedCautions") { options.acknowledgedCautions.forEach { add(JsonPrimitive(it)) } }
        }
        return nativeCompile(request.toString())
    }

    private fun hex(b: ByteArray): String = b.joinToString("") { "%02x".format(it) }

    /** For tests and diagnostics: the JSON element the core would parse a request from. */
    internal fun asElement(s: String): JsonElement = Json.parseToJsonElement(s)
}
