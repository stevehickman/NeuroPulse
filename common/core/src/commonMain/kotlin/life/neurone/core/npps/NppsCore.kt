package life.neurone.core.npps

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonPrimitive
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
 * The native library must be reachable through [NppsBackend]. On the JVM (Android and the
 * desktop apps) that is `neurone_npps_jni`, loadable through `java.library.path` on a test host
 * or from the APK's jniLibs on a device. Targets with no backend wired yet (iOS, web) throw
 * [UnsupportedOperationException] on first use rather than answer from a second implementation
 * (OI-UI-KMP-01).
 */
object NppsCore {
    private val backend: NppsBackend by lazy { defaultNppsBackend() }

    private fun nativeParse(source: String): String = backend.parse(source)

    private fun nativeCompile(request: String): ByteArray = backend.compile(request)

    private fun nativeNamespace(request: String): String = backend.namespace(request)

    private fun nativeSerialize(request: String): String = backend.serialize(request)

    private fun nativeValidate(request: String): String = backend.validate(request)

    private fun nativeResolveLimits(request: String): String = backend.resolveLimits(request)

    private fun nativeContentHash(request: String): String = backend.contentHash(request)

    private fun nativeLibrary(request: String): String = backend.library(request)
    private fun nativeMigration(request: String): String {
        requestObserver?.invoke(request)
        return backend.migration(request)
    }

    /**
     * For tests that no key is ever text on its way to the core (`SWR-MG-30` (e)): shown the JSON of every migration request,
     * through either entry point, exactly as it is handed to the binding. Nothing in the apps sets it.
     */
    internal var requestObserver: ((String) -> Unit)? = null

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

    /**
     * A protocol's identity (`SWR-HV-01`): 64 lowercase hex digits. `request` is
     * `{"def":…, "hardwareVersion":n, "zonesByVersion":{…}, "clinicianSockets":[…]|null}` (or `"zones"` for one layout), the
     * same zone and socket inputs as [compile]; the serial, rules, Mode 3 flag and cautions are not inputs.
     */
    fun contentHash(request: JsonObject): String = nativeContentHash(request.toString())

    /**
     * The protocol library's rules, decided once in the core for every app: `{"op": "saveCheck" | "deleteCheck" |
     * "cloneFamily" | …, …}` in, the answer out. A refusal of the request itself (a missing field, an unknown
     * operation) is an [IllegalArgumentException]; a rule that says no is an ordinary answer (`"allowed": false`).
     */
    fun library(request: JsonObject): JsonObject = Json.parseToJsonElement(nativeLibrary(request.toString())) as JsonObject

    /**
     * The device-replacement migration and UHDR backup rules, decided once in the core for every app (`SWR-MG`):
     * `{"op": "inventory" | "readHeader" | "mergeWithdrawal" | "claimDecision" | …, …}` in, the answer out. The core is
     * given item names and ids, except by `sealBundle` and `openBundle`, which are handed a key and the items for one call
     * and keep nothing. A refusal of the request itself is an
     * [IllegalArgumentException]; a rule that says no is an ordinary answer.
     */
    fun migration(request: JsonObject): JsonObject = Json.parseToJsonElement(nativeMigration(request.toString())) as JsonObject

    /**
     * What [migrationSecret] answers: the JSON, and the secrets it names as `{"$secret": n}`, as bytes. Call [clear] when the
     * secrets have been used or handed to something that keeps them (a keystore): it overwrites them.
     */
    class SecretAnswer(val answer: JsonObject, val secrets: List<ByteArray>) {
        fun clear() = secrets.forEach { it.fill(0) }
        override fun toString(): String = "SecretAnswer(${secrets.size} secret(s), <redacted>)"
    }

    /**
     * [migration] for an operation that takes or returns a key (`SWR-MG-30`). The request names a secret as
     * `{"$secret": n}` and `secrets[n]` holds its bytes; a secret in the answer comes back the same way. **A key is never
     * text on this path**: not in the request, not in the answer, not in a `String` this function makes. The arrays in
     * [secrets] are the caller's to overwrite; the framed copy made here is overwritten before this returns, whether the
     * core answered or refused. A key the core keeps is named `{"$handle": n}` and needs no bytes: it goes through
     * [migration].
     */
    fun migrationSecret(request: JsonObject, secrets: List<ByteArray>): SecretAnswer {
        val framed = ByteArray(secrets.sumOf { 4 + it.size })
        var at = 0
        for (s in secrets) {
            putLength(framed, at, s.size)
            s.copyInto(framed, at + 4)
            at += 4 + s.size
        }
        val out = try {
            val text = request.toString()
            requestObserver?.invoke(text)
            backend.migrationSecret(text, framed)
        } finally {
            framed.fill(0)
        }
        try {
            val answerLength = length(out, 0)
            val answer = Json.parseToJsonElement(out.decodeToString(4, 4 + answerLength)) as JsonObject
            val answered = mutableListOf<ByteArray>()
            var i = 4 + answerLength
            while (i < out.size) {
                val n = length(out, i)
                answered += out.copyOfRange(i + 4, i + 4 + n)
                i += 4 + n
            }
            return SecretAnswer(answer, answered)
        } finally {
            out.fill(0)
        }
    }

    /** How a request names the n-th secret given beside it. */
    fun secretRef(n: Int): JsonObject = buildJsonObject { put("\$secret", n) }

    /** The bytes of the secret [answer] names at [field], or null if it names none there. The array is the answer's: [SecretAnswer.clear] overwrites it. */
    fun secretAt(answer: SecretAnswer, field: String): ByteArray? {
        val n = (answer.answer[field] as? JsonObject)?.get("\$secret")?.jsonPrimitive?.intOrNull ?: return null
        return answer.secrets.getOrNull(n)
    }

    private fun putLength(into: ByteArray, at: Int, n: Int) {
        for (k in 0..3) into[at + k] = (n ushr (24 - 8 * k)).toByte()
    }

    private fun length(from: ByteArray, at: Int): Int {
        require(at + 4 <= from.size) { "The core's answer is cut short" }
        var n = 0
        for (k in 0..3) n = (n shl 8) or (from[at + k].toInt() and 0xff)
        require(n >= 0 && at + 4 + n <= from.size) { "The core's answer is cut short" }
        return n
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
        /**
         * The hardware version of the helmet the descriptor is compiled for (1 to 15), stamped in the signed header; the
         * helmet refuses a different one (`SWR-HV-08`). Null leaves the core's default of 1 (`SWR-HV-07`).
         */
        val hardwareVersion: Int? = null,
        /**
         * Mode 3: the descriptor's header carries the autonomous flag (`NP_PROTO_FLAG_AUTONOMOUS`, NP-FW-HUB-001 §4.1) so
         * the hub is meant to run it without a phone (`CLAUDE.md` §4.6). No firmware reader exists for it yet.
         */
        val autonomous: Boolean = false,
    )

    /**
     * Compile a protocol (`{"timingMode":…,"modalities":[…]}`, the shape [parse] returns under
     * `protocol`) into the NP-FW-HUB-001 §4 descriptor. The 64-byte signature slot at the end is
     * zeroed: sign the region before it and write the signature in.
     */
    fun compile(protocol: JsonObject, options: CompileOptions): ByteArray = compileRequest(options) { put("def", protocol) }

    /**
     * Compile a composite (`OI-NPPS-COMP-01`): [composite] is the shape [parse] returns under `composite`, and [members] maps
     * each protocol name a layer uses to that protocol's definition. One descriptor comes back, signed like a protocol's.
     */
    fun compileComposite(composite: JsonObject, members: JsonObject, options: CompileOptions): ByteArray =
        compileRequest(options) { put("composite", composite); put("members", members) }

    private fun compileRequest(options: CompileOptions, what: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit): ByteArray {
        require(options.sessionUuid.size == 16) { "sessionUuid must be 16 bytes" }
        val request = buildJsonObject {
            what()
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
            options.hardwareVersion?.let { put("hardwareVersion", it) }
            if (options.autonomous) put("autonomous", true)
        }
        return nativeCompile(request.toString())
    }

    private fun hex(b: ByteArray): String = b.joinToString("") { it.toUByte().toString(16).padStart(2, '0') }

    /** For tests and diagnostics: the JSON element the core would parse a request from. */
    internal fun asElement(s: String): JsonElement = Json.parseToJsonElement(s)
}
