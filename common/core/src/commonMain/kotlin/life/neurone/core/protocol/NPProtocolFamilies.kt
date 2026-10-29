package life.neurone.core.protocol

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray
import kotlinx.serialization.json.putJsonObject
import life.neurone.core.npps.NppsCore
import life.neurone.core.npps.toNppsCoreJson

// The protocol library's rules as the app asks them (SWR-PL-02/03/06, SWR-UP-02, SWR-WO-01/02/03, SWR-FM-03…07,
// SWR-HV-04). Nothing is decided here: every answer is the shared core's (`common/npps-core/src/library.rs`, reached
// through NppsCore.library), so the five apps cannot disagree. This file is the typed shape of the answers, the entry
// summaries the core is given, and the calls. The library (NPProtocolLibrary) keeps the state and the screens read it.

// ── Answers ────────────────────────────────────────────────────────────────────────────────────────────────────

/** What a save met. */
sealed class SaveResult {
    object Saved : SaveResult()

    /** A predefined protocol: never saved over; the way to change it is a clone. */
    object ReadOnly : SaveResult()

    /** The protocol's family has been used: nothing was saved, and a clone of the whole family is offered (SWR-PL-02, SWR-FM-03). */
    data class RefusedFamilyUsed(val familyId: String, val memberIds: List<String>) : SaveResult()

    /** A variant for this hardware version already exists in the family (SWR-FM-06). */
    object RefusedVariantExists : SaveResult()
}

/** What a delete met. */
sealed class DeleteResult {
    object Deleted : DeleteResult()
    object ReadOnly : DeleteResult()
    object NotFound : DeleteResult()

    /** A used family is archived, never deleted (SWR-FM-05). */
    object RefusedFamilyUsed : DeleteResult()
}

/** What an upload meets before any byte is sent. A refusal is a local decision: nothing is sent and nothing is marked used. */
sealed class UploadGate {
    object Allow : UploadGate()

    /**
     * The protocol has no layout for the helmet's hardware version (SWR-HV-04): the helmet's version, the versions the
     * protocol supports, and how the first of those layouts overlaps the helmet's (sockets in both, only in the protocol's,
     * only in the helmet's). The counts are null while the helmet's layout is not in the table, so no overlap can be drawn.
     */
    data class HardwareMismatch(
        val helmetVersion: Int,
        val protocolVersions: List<Int>,
        val sharedSockets: Int?,
        val onlyProtocolSockets: Int?,
        val onlyHelmetSockets: Int?,
    ) : UploadGate()

    /** The clone's content is its source's (SWR-PL-06). The three outcomes are upload the source, edit the clone, cancel. */
    data class UnmodifiedClone(val sourceId: life.neurone.core.common.UUID) : UploadGate()
}

/** The three outcomes of an upload that was sent, and the two refusals that are not failed uploads (SWR-UP-02). */
enum class UploadOutcome(val rule: String) {
    CONFIRMED("confirmed"),
    UNKNOWN("unknown"),
    CONFIRMED_FAILED("confirmed_failed"),
    NOT_SENT("not_sent"),
    NOT_A_FAILED_UPLOAD("not_a_failed_upload"),
}

// ── Entry helpers ──────────────────────────────────────────────────────────────────────────────────────────────

/** A protocol's or composite's family id; null for the other kinds and for a protocol that states none. */
val NPProtocolEntry.familyId: String?
    get() = when (this) {
        is NPProtocolEntry.Single -> protocol.familyId
        is NPProtocolEntry.Composite -> composite.familyId
        else -> null
    }

/** The key of the family an entry belongs to: its family id, or its own id when it has none (a family of one). */
fun NPProtocolEntry.familyKey(): String = familyId ?: id.toString()

/** A protocol or a composite: the entries a person runs, as opposed to the zone, condition and limits definitions. */
val NPProtocolEntry.isRunnable: Boolean get() = this is NPProtocolEntry.Single || this is NPProtocolEntry.Composite

private val NPProtocolEntry.versionText: String
    get() = when (this) {
        is NPProtocolEntry.Single -> protocol.version
        is NPProtocolEntry.Composite -> composite.version
        else -> ""
    }

// ── The core's questions ───────────────────────────────────────────────────────────────────────────────────────

/** The core's reading of a helmet upload report: the library's `helmetRefusal`, whether it was only "not ready yet", and the cause to say. */
internal data class HelmetCauseAnswer(val refusalKind: String?, val notReady: Boolean, val cause: String?)

internal object NPLibraryRules {

    private fun ask(op: String, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit = {}): JsonObject =
        NppsCore.library(buildJsonObject { put("op", op); build() })

    /** The zone namespace as the core takes it: every zone's socket list per hardware version. One layout exists, version 1. */
    fun zonesByVersion(): JsonObject = buildJsonObject {
        for ((name, zone) in NPZoneRegistry.zones) {
            putJsonObject(name) { putJsonArray("1") { zone.sockets.forEach { add(JsonPrimitive(it)) } } }
        }
    }

    /** Hardware versions a protocol can be sent to (SWR-HV-04); a composite is a version-1 object until composites carry zones of their own. */
    fun supportedVersions(entry: NPProtocolEntry): List<Int> = when (entry) {
        is NPProtocolEntry.Single -> ask("supportedVersions") {
            put("def", entry.protocol.toNppsCoreJson())
            put("zonesByVersion", zonesByVersion())
        }["versions"]!!.jsonArray.map { it.jsonPrimitive.content.toInt() }
        is NPProtocolEntry.Composite -> listOf(1)
        else -> emptyList()
    }

    /** The summaries the core decides over. [withVersions] costs a core call per protocol, so only the questions that need it ask. */
    fun summaries(entries: List<NPProtocolEntry>, used: Set<String>, withVersions: Boolean): JsonArray = buildJsonArray {
        for (e in entries.filter { it.isRunnable }) {
            add(buildJsonObject {
                put("id", e.id.toString())
                e.familyId?.let { put("familyId", it) }
                put("used", e.familyKey() in used)
                put("version", e.versionText)
                put("scope", "owner")
                if (withVersions) putJsonArray("hardwareVersions") { supportedVersions(e).forEach { add(JsonPrimitive(it)) } }
            })
        }
    }

    fun saveCheck(entries: JsonArray, id: String): JsonObject = ask("saveCheck") { put("entries", entries); put("id", id) }

    fun deleteCheck(entries: JsonArray, id: String): JsonObject = ask("deleteCheck") { put("entries", entries); put("id", id) }

    fun cloneFamily(entries: JsonArray, id: String, newFamilyId: String, newIds: List<String>): JsonObject =
        ask("cloneFamily") {
            put("entries", entries); put("id", id); put("newFamilyId", newFamilyId)
            putJsonArray("newIds") { newIds.forEach { add(JsonPrimitive(it)) } }
        }

    fun addVariantCheck(entries: JsonArray, familyId: String, hardwareVersion: Int): JsonObject =
        ask("addVariantCheck") { put("entries", entries); put("familyId", familyId); put("hardwareVersion", hardwareVersion) }

    fun variantFor(entries: JsonArray, familyId: String, helmetVersion: Int): JsonObject =
        ask("variantFor") { put("entries", entries); put("familyId", familyId); put("helmetVersion", helmetVersion) }

    fun offered(entries: JsonArray, archivedFamilies: Collection<String>): List<String> =
        ask("offered") {
            put("entries", entries)
            putJsonArray("archivedFamilies") { archivedFamilies.forEach { add(JsonPrimitive(it)) } }
        }["offered"]!!.jsonArray.map { it.jsonPrimitive.content }

    fun uploadGate(helmetVersion: Int, protocolVersions: List<Int>, contentHash: String, sourceHash: String?): JsonObject =
        ask("uploadGate") {
            put("helmetVersion", helmetVersion)
            putJsonArray("protocolVersions") { protocolVersions.forEach { add(JsonPrimitive(it)) } }
            put("contentHash", contentHash)
            if (sourceHash == null) put("sourceHash", JsonNull) else put("sourceHash", sourceHash)
        }

    fun uploadResult(
        sentHash: String,
        helmetStatus: String?,
        helmetHash: String?,
        helmetRefusal: String?,
        refusedBeforeSend: Boolean,
    ): JsonObject = ask("uploadResult") {
        put("sentHash", sentHash)
        helmetStatus?.let { put("helmetStatus", it) }
        helmetHash?.let { put("helmetHash", it) }
        helmetRefusal?.let { put("helmetRefusal", it) }
        put("refusedBeforeSend", refusedBeforeSend)
    }

    /**
     * What a helmet's upload report means to a person (SWR-UP-02, SWR-HV-08, REQ-UPG-01), decided by the shared core. `status` is
     * `"accepted"`, `"refused"` or null; `reason` is the refusal's `np_hub_status_t` as a positive number; `sentHash` is given when the
     * screen is about the upload whose bytes hash to it.
     */
    fun helmetCause(status: String?, reason: Int?, helmetHash: String?, sentHash: String?): HelmetCauseAnswer {
        val answer = ask("helmetCause") {
            status?.let { put("status", it) }
            reason?.let { put("reason", it) }
            helmetHash?.let { put("helmetHash", it) }
            sentHash?.let { put("sentHash", it) }
        }
        return HelmetCauseAnswer(
            refusalKind = answer["refusalKind"]?.jsonPrimitive?.contentOrNull,
            notReady = answer["notReady"]?.jsonPrimitive?.boolean == true,
            cause = answer["cause"]?.jsonPrimitive?.contentOrNull,
        )
    }

    fun mergeImport(usedHashes: Collection<String>, incoming: List<Triple<String, String?, Boolean>>): Map<String, Boolean> =
        ask("mergeImport") {
            putJsonArray("usedHashes") { usedHashes.forEach { add(JsonPrimitive(it)) } }
            putJsonArray("incoming") {
                for ((id, hash, used) in incoming) add(buildJsonObject {
                    put("id", id)
                    hash?.let { put("hash", it) }
                    put("used", used)
                })
            }
        }["entries"]!!.jsonArray.associate {
            val o = it.jsonObject
            o["id"]!!.jsonPrimitive.content to o["used"]!!.jsonPrimitive.boolean
        }

    /** The record a used family is stored as: the family id and nothing else (SWR-WO-02). */
    fun usedRecord(familyId: String): JsonObject = ask("usedRecord") { put("familyId", familyId) }["record"]!!.jsonObject

    fun isValidUsedRecord(record: JsonElement): Boolean =
        ask("usedRecord") { put("check", record) }["valid"]!!.jsonPrimitive.boolean

    /** A protocol's identity (SWR-HV-01) for the one layout there is; null for a kind that has none or a protocol that cannot be hashed yet. */
    fun contentHash(entry: NPProtocolEntry): String? {
        val protocol = (entry as? NPProtocolEntry.Single)?.protocol ?: return null
        return try {
            NppsCore.contentHash(buildJsonObject {
                put("def", protocol.toNppsCoreJson())
                put("hardwareVersion", 1)
                put("zonesByVersion", zonesByVersion())
                put("clinicianSockets", JsonNull)
            })
        } catch (e: IllegalArgumentException) {
            null // for example a clinician-selected target the operator has not chosen yet
        }
    }

    fun string(o: JsonObject, key: String): String? = o[key]?.jsonPrimitive?.contentOrNull
}
