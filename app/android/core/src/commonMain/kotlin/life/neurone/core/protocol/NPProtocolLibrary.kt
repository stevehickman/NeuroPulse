package life.neurone.core.protocol

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.UUID
import life.neurone.core.npps.NppsCore
import life.neurone.core.npps.toNppsCoreItem

// Port of iOS NPProtocolLibrary (app/ios/NeurOne/Protocol/NPProtocolLibrary.swift).
// Pure-JVM: bundled + user protocol store, device-tier/EEG-consent availability, and
// validation integration. On iOS this is an @MainActor ObservableObject; on Android the
// UI layer observes it (a ViewModel wraps it). Persistence is via KeyValueStore using the
// NPPS text format (round-tripped through the existing serializer/parser) instead of iOS's
// JSON file — the on-device format is an implementation detail, behavior is identical.

// MARK: - Modality tier helpers (port of NPModalityType.tier / t1Set / t2Set) -----------

enum class NPModalityTier { T1, T2, ACCESSORY }

val NPModalityType.tier: NPModalityTier
    get() = when (this) {
        NPModalityType.PBM_TRANSCRANIAL, NPModalityType.PBM_INTRANASAL,
        NPModalityType.EEG_NEUROFEEDBACK, NPModalityType.BES_TACS, NPModalityType.TDCS,
        NPModalityType.VNS_HRV, NPModalityType.AUDIO_ENTRAINMENT, NPModalityType.VISUAL_STIMULATION ->
            NPModalityTier.T1
        NPModalityType.QEEG_21CH, NPModalityType.TMS, NPModalityType.PBM_DEEP_1170NM,
        NPModalityType.CLINICAL_TACS, NPModalityType.HD_TDCS, NPModalityType.CERVICAL_VNS ->
            NPModalityTier.T2
        NPModalityType.VIBROTACTILE_40HZ -> NPModalityTier.ACCESSORY
    }

val NP_T1_MODALITY_SET: Set<NPModalityType> = setOf(
    NPModalityType.PBM_TRANSCRANIAL, NPModalityType.PBM_INTRANASAL, NPModalityType.EEG_NEUROFEEDBACK,
    NPModalityType.BES_TACS, NPModalityType.TDCS, NPModalityType.VNS_HRV,
    NPModalityType.AUDIO_ENTRAINMENT, NPModalityType.VISUAL_STIMULATION,
)

val NP_T2_MODALITY_SET: Set<NPModalityType> = NP_T1_MODALITY_SET + setOf(
    NPModalityType.QEEG_21CH, NPModalityType.TMS, NPModalityType.PBM_DEEP_1170NM,
    NPModalityType.CLINICAL_TACS, NPModalityType.HD_TDCS, NPModalityType.CERVICAL_VNS,
)

// MARK: - NPProtocolEntry / NPProtocolDefinition computed helpers ------------------------

val NPProtocolEntry.id: UUID
    get() = when (this) {
        is NPProtocolEntry.Single -> protocol.id
        is NPProtocolEntry.Composite -> composite.id
        is NPProtocolEntry.Limits -> limits.id
        // Definitions are namespace entries, not library items: they are keyed
        // by name and never appear in a protocol list, so a stable synthetic id
        // is enough and must at least be distinct per name.
        is NPProtocolEntry.Zone -> UUID.nameUUIDFromBytes("zone:${zone.name}".encodeToByteArray())
        is NPProtocolEntry.Condition ->
            UUID.nameUUIDFromBytes("condition:${condition.name}".encodeToByteArray())
    }

val NPProtocolEntry.name: String
    get() = when (this) {
        is NPProtocolEntry.Single -> protocol.name
        is NPProtocolEntry.Composite -> composite.name
        is NPProtocolEntry.Limits -> limits.name
        is NPProtocolEntry.Zone -> zone.name
        is NPProtocolEntry.Condition -> condition.name
    }

val NPProtocolEntry.isReadOnly: Boolean
    get() = when (this) {
        is NPProtocolEntry.Single -> protocol.isReadOnly
        is NPProtocolEntry.Composite -> composite.isReadOnly
        is NPProtocolEntry.Limits -> false
        // A shipped definition carries an id; a user-authored one does not.
        is NPProtocolEntry.Zone -> zone.isPredefined
        is NPProtocolEntry.Condition -> condition.id != null
    }

val NPProtocolEntry.isComposite: Boolean get() = this is NPProtocolEntry.Composite

val NPProtocolDefinition.requiredModalityTypes: Set<NPModalityType>
    get() = modalities.filter { it.enabled }.map { it.modalityType }.toSet()

fun NPProtocolDefinition.requires1064SmartModule(): Boolean =
    modalities.filter { it.enabled }.any {
        val p = it.params
        p is NPModalityParams.PbmTranscranial && p.params.wavelength.requiresSmartModule
    }

// MARK: - Availability ------------------------------------------------------------------

enum class DeviceTier { NONE, T1, T2 }

/** Port of iOS ProtocolAvailability, incl. the ISC-90 `.eegConsentRequired` case. */
sealed class ProtocolAvailability {
    object Available : ProtocolAvailability()
    data class Unavailable(val missingModalities: List<NPModalityType>) : ProtocolAvailability()
    object EegConsentRequired : ProtocolAvailability()

    val isAvailable: Boolean get() = this is Available
}

// MARK: - NPProtocolLibrary -------------------------------------------------------------

class NPProtocolLibrary(
    private val kv: KeyValueStore,
) {
    companion object {
        const val USER_PROTOCOLS_KEY = "np.user-protocols.npps"
        const val BIPA_ACCEPTED_KEY = "np.onboarding.bipa-accepted"

        /** The families that have been used (SWR-WO-02): a list of `{"familyId"}` records, the family id and nothing else. */
        const val USED_FAMILIES_KEY = "np.used-families"

        /** The families the warranty owner has archived (SWR-WO-01): a list of family ids. A list state, never a deletion. */
        const val ARCHIVED_FAMILIES_KEY = "np.archived-families"

        /** Clone entry id → the entry it was cloned from, for the unmodified-clone check at upload (SWR-PL-06). */
        const val CLONE_SOURCES_KEY = "np.clone-sources"
    }

    /** BIPA biometric consent (ISC-90). Seeded from the shared onboarding key. */
    var eegConsentGranted: Boolean = kv.getBoolean(BIPA_ACCEPTED_KEY)
        private set

    var connectedDeviceTier: DeviceTier = DeviceTier.NONE
        private set
    var has1064SmartModules: Boolean = false
        private set
    var availableModalities: Set<NPModalityType> = NP_T1_MODALITY_SET
        private set

    val bundledProtocols: List<NPProtocolEntry> = loadBundledProtocols()

    private val _userProtocols: MutableList<NPProtocolEntry> = mutableListOf()
    val userProtocols: List<NPProtocolEntry> get() = _userProtocols

    val allProtocols: List<NPProtocolEntry> get() = bundledProtocols + _userProtocols

    private val validationCache: MutableMap<UUID, NPValidationResult> = mutableMapOf()

    // The state the library rules decide over (declared before `init`, which loads it).
    private val usedFamilies: MutableSet<String> = linkedSetOf()
    private val archivedFamilies: MutableSet<String> = linkedSetOf()
    private val cloneSources: MutableMap<String, String> = linkedMapOf()

    /**
     * Duplicate zone/condition names between the shipped library and the user's
     * own scripts. Empty in the normal case; a non-empty list means those names
     * are undefined and any protocol referencing one will not resolve.
     *
     * Declared BEFORE `init`: Kotlin runs property initializers and init blocks
     * in declaration order, so below the init block `= emptyList()` would run
     * after [publishZoneNamespace] and wipe what it had just stored.
     */
    var zoneNamespaceErrors: List<String> = emptyList()
        private set

    init {
        loadFromStore()
        loadFamilyState()
        // Unconditional: loadFromStore() returns early when nothing is stored,
        // and the registry must be pointed at a namespace either way.
        publishZoneNamespace()
    }

    // MARK: Availability

    fun availability(entry: NPProtocolEntry): ProtocolAvailability {
        // BIPA gate (ISC-90) runs before hardware/tier — parity with iOS.
        if (!eegConsentGranted && ProtocolConsentGate.isEEGDependent(entry, ::findSingle)) {
            return ProtocolAvailability.EegConsentRequired
        }
        if (connectedDeviceTier == DeviceTier.NONE) {
            return ProtocolAvailability.Unavailable(emptyList())
        }
        return when (entry) {
            is NPProtocolEntry.Composite -> {
                val missing = linkedSetOf<NPModalityType>()
                for (layer in entry.composite.layers) {
                    val def = findSingle(layer.protocolName) ?: continue
                    val sub = availability(NPProtocolEntry.Single(def))
                    if (sub is ProtocolAvailability.Unavailable) missing.addAll(sub.missingModalities)
                }
                if (missing.isEmpty()) ProtocolAvailability.Available
                else ProtocolAvailability.Unavailable(missing.toList())
            }
            is NPProtocolEntry.Single -> {
                val missing = mutableListOf<NPModalityType>()
                val def = entry.protocol
                if (def.requires1064SmartModule() && !has1064SmartModules) {
                    missing.add(NPModalityType.PBM_TRANSCRANIAL)
                }
                for (modality in def.requiredModalityTypes) {
                    if (!isModalityAvailable(modality) && !missing.contains(modality)) missing.add(modality)
                }
                if (missing.isEmpty()) ProtocolAvailability.Available
                else ProtocolAvailability.Unavailable(missing)
            }
            // A definition needs no hardware — it is referenced by protocols,
            // never run.
            is NPProtocolEntry.Limits,
            is NPProtocolEntry.Zone,
            is NPProtocolEntry.Condition -> ProtocolAvailability.Available
        }
    }

    private fun isModalityAvailable(modality: NPModalityType): Boolean = when (modality.tier) {
        NPModalityTier.T1 -> connectedDeviceTier == DeviceTier.T1 || connectedDeviceTier == DeviceTier.T2
        NPModalityTier.T2 -> connectedDeviceTier == DeviceTier.T2
        NPModalityTier.ACCESSORY -> true
    }

    fun updateAvailability(tier: DeviceTier, smartModules: Boolean) {
        connectedDeviceTier = tier
        has1064SmartModules = smartModules
        availableModalities = when (tier) {
            DeviceTier.NONE -> emptySet()
            DeviceTier.T1 -> NP_T1_MODALITY_SET
            DeviceTier.T2 -> NP_T2_MODALITY_SET
        }
    }

    /** Push the BIPA consent state from onboarding (ISC-90), parity with iOS updateEEGConsent. */
    fun updateEEGConsent(granted: Boolean) {
        eegConsentGranted = granted
    }

    // MARK: Validation integration

    fun validationResult(
        entry: NPProtocolEntry,
        limits: NPLimitsSet = NPLimitsSet(name = "unlimited"),
    ): NPValidationResult {
        validationCache[entry.id]?.let { return it }
        val result = NPProtocolValidator(limits).validate(entry, ::findSingle)
        validationCache[entry.id] = result
        return result
    }

    fun issueCount(entry: NPProtocolEntry): Pair<Int, Int> {
        val result = validationCache[entry.id] ?: return 0 to 0
        return result.errors.size to result.warnings.size
    }

    fun revalidateAll(limits: NPLimitsSet = NPLimitsSet(name = "unlimited")) {
        validationCache.clear()
        val validator = NPProtocolValidator(limits)
        for (entry in allProtocols) validationCache[entry.id] = validator.validate(entry, ::findSingle)
    }

    // MARK: Read-only guards

    fun canEdit(entry: NPProtocolEntry): Boolean = !entry.isReadOnly
    fun canDelete(entry: NPProtocolEntry): Boolean = !entry.isReadOnly

    // MARK: Mutations

    /**
     * Save [entry]. The rules are the core's (SWR-PL-02/03, SWR-FM-03/06): a predefined protocol is never saved over; a
     * protocol of a used family is refused, nothing is written, and a clone of the whole family is offered; an unused one
     * saves in place. A new protocol joining a used family is a new variant, allowed only for a hardware version the
     * family has no variant for.
     */
    fun save(entry: NPProtocolEntry): SaveResult {
        if (bundledProtocols.any { it.id == entry.id }) return SaveResult.ReadOnly // never overwrite a bundled entry
        val idx = _userProtocols.indexOfFirst { it.id == entry.id }
        if (idx >= 0) {
            val check = NPLibraryRules.saveCheck(familySummaries(withVersions = false), entry.id.toString())
            if (check["allowed"]?.jsonPrimitive?.boolean != true) {
                return SaveResult.RefusedFamilyUsed(
                    familyId = NPLibraryRules.string(check, "familyId") ?: entry.familyKey(),
                    memberIds = check["members"]?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList(),
                )
            }
            _userProtocols[idx] = entry
        } else {
            if (entry.isRunnable && entry.familyId != null && entry.familyKey() in usedFamilies) {
                val version = NPLibraryRules.supportedVersions(entry).firstOrNull() ?: 1
                val check = NPLibraryRules.addVariantCheck(familySummaries(withVersions = true), entry.familyKey(), version)
                if (check["allowed"]?.jsonPrimitive?.boolean != true) return SaveResult.RefusedVariantExists
            }
            _userProtocols.add(entry)
        }
        saveToStore()
        publishZoneNamespace()
        return SaveResult.Saved
    }

    /** Delete a user protocol. A used family is archived, never deleted (SWR-FM-05). */
    fun delete(id: UUID): DeleteResult {
        if (bundledProtocols.any { it.id == id }) return DeleteResult.ReadOnly
        if (_userProtocols.none { it.id == id }) return DeleteResult.NotFound
        val check = NPLibraryRules.deleteCheck(familySummaries(withVersions = false), id.toString())
        if (check["allowed"]?.jsonPrimitive?.boolean != true) return DeleteResult.RefusedFamilyUsed
        _userProtocols.removeAll { it.id == id }
        cloneSources.remove(id.toString())
        saveToStore()
        saveFamilyState()
        publishZoneNamespace()
        return DeleteResult.Deleted
    }

    // MARK: Families (SWR-FM-03…07, SWR-PL-02)

    /** Whether [entry]'s family has been used. The use of any variant marks the family (SWR-FM-03). */
    fun isUsed(entry: NPProtocolEntry): Boolean = entry.familyKey() in usedFamilies

    /**
     * Clone the whole family [id] belongs to: a new family from the moment it is made, a copy of every variant, none of
     * them used, fully editable, the source family recorded for provenance only (SWR-FM-03). Each copy's name is the
     * source's with " (copy)" added, made unique, because a protocol is referred to by name.
     */
    fun cloneFamily(id: UUID): List<NPProtocolEntry> {
        val entry = allProtocols.firstOrNull { it.id == id && it.isRunnable } ?: return emptyList()
        val key = entry.familyKey()
        val members = allProtocols.filter { it.isRunnable && it.familyKey() == key }
        val newFamily = UUID.randomUUID().toString()
        val newIds = members.map { UUID.randomUUID() }
        // The core checks the count, that the family is new and that every id is new, before anything is made.
        NPLibraryRules.cloneFamily(familySummaries(withVersions = false), id.toString(), newFamily, newIds.map { it.toString() })
        val taken = allProtocols.map { it.name }.toMutableSet()
        val copies = members.zip(newIds).map { (m, newId) ->
            val name = uniqueCopyName(m.name, taken)
            taken.add(name)
            when (m) {
                is NPProtocolEntry.Single -> NPProtocolEntry.Single(
                    m.protocol.copy(id = newId, name = name, isPredefined = false, isReadOnly = false, familyId = newFamily))
                is NPProtocolEntry.Composite -> NPProtocolEntry.Composite(
                    m.composite.copy(id = newId, name = name, isPredefined = false, isReadOnly = false, familyId = newFamily))
                else -> m
            }
        }
        for ((m, copy) in members.zip(copies)) {
            _userProtocols.add(copy)
            cloneSources[copy.id.toString()] = m.id.toString()
        }
        saveToStore()
        saveFamilyState()
        publishZoneNamespace()
        return copies
    }

    private fun uniqueCopyName(base: String, taken: Set<String>): String {
        var candidate = "$base (copy)"
        var n = 2
        while (candidate in taken) candidate = "$base (copy $n)".also { n += 1 }
        return candidate
    }

    /** The variants of [entry]'s family that fit a helmet of [helmetVersion], latest first; empty means the person decides whether to create one (SWR-HV-04). */
    fun variantsFor(entry: NPProtocolEntry, helmetVersion: Int): List<NPProtocolEntry> {
        val ids = NPLibraryRules.variantFor(familySummaries(withVersions = true), entry.familyKey(), helmetVersion)["variants"]!!
            .jsonArray.map { it.jsonPrimitive.content }
        return ids.mapNotNull { id -> allProtocols.firstOrNull { it.id.toString() == id } }
    }

    // MARK: Archive and the offered list (SWR-WO-01, SWR-FM-05)

    /** Archive the family of [entry]: it drops out of the offered list and every variant is kept. */
    fun archiveFamily(entry: NPProtocolEntry) {
        if (archivedFamilies.add(entry.familyKey())) saveFamilyState()
    }

    fun unarchiveFamily(entry: NPProtocolEntry) {
        if (archivedFamilies.remove(entry.familyKey())) saveFamilyState()
    }

    fun isArchived(entry: NPProtocolEntry): Boolean = entry.familyKey() in archivedFamilies

    /** The warranty owner's set: every protocol whose family is not archived. */
    val offeredProtocols: List<NPProtocolEntry>
        get() {
            val offered = NPLibraryRules.offered(familySummaries(withVersions = false), archivedFamilies).toSet()
            return allProtocols.filter { !it.isRunnable || it.id.toString() in offered }
        }

    // MARK: Upload (SWR-PL-06, SWR-HV-04, SWR-UP-02)

    /** [entry]'s identity as the core defines it, or null for a kind that has none. */
    fun contentHash(entry: NPProtocolEntry): String? = NPLibraryRules.contentHash(entry)

    /**
     * What an upload of [entry] to a helmet of [helmetHardwareVersion] meets before any byte is sent. The hardware
     * refusal comes first, then the unmodified clone; an allowed upload is then sent and its result passed to
     * [recordUploadResult].
     */
    fun uploadGate(entry: NPProtocolEntry, helmetHardwareVersion: Int): UploadGate {
        val hash = contentHash(entry) ?: return UploadGate.Allow
        val sourceId = cloneSources[entry.id.toString()]
        val source = sourceId?.let { sid -> allProtocols.firstOrNull { it.id.toString() == sid } }
        val answer = NPLibraryRules.uploadGate(
            helmetHardwareVersion,
            NPLibraryRules.supportedVersions(entry),
            hash,
            source?.let { contentHash(it) },
        )
        return when (NPLibraryRules.string(answer, "reason")) {
            "hardware_mismatch" -> UploadGate.HardwareMismatch(
                helmetVersion = helmetHardwareVersion,
                protocolVersions = answer["protocolVersions"]!!.jsonArray.map { it.jsonPrimitive.content.toInt() },
                details = answer,
            )
            "unmodified_clone" -> UploadGate.UnmodifiedClone(source!!.id)
            else -> UploadGate.Allow
        }
    }

    /**
     * Record how an upload that was sent ended. Confirmed and unknown mark [entry]'s family used; a confirmed failure, a
     * refusal before sending and a helmet refusal for hardware version, unmodified clone or tier do not (SWR-UP-02).
     * [helmetStatus] is the hub's `UPLOAD_CHECK` verdict, `"accepted"` or `"refused"`, or null when it never answered.
     */
    fun recordUploadResult(
        entry: NPProtocolEntry,
        sentHash: String,
        helmetStatus: String?,
        helmetHash: String?,
        helmetRefusal: String? = null,
        refusedBeforeSend: Boolean = false,
    ): UploadOutcome {
        val answer = NPLibraryRules.uploadResult(sentHash, helmetStatus, helmetHash, helmetRefusal, refusedBeforeSend)
        if (answer["markUsed"]?.jsonPrimitive?.boolean == true) markUsed(entry)
        val rule = NPLibraryRules.string(answer, "outcome")
        return UploadOutcome.values().first { it.rule == rule }
    }

    /** The used flag is one bit per family and is never cleared (SWR-WO-02). */
    private fun markUsed(entry: NPProtocolEntry) {
        if (usedFamilies.add(entry.familyKey())) saveFamilyState()
    }

    // MARK: Script export / import

    /** The script for [entry], carrying the used flag when its family is used (SWR-WO-03). The flag is not part of the content hash. */
    fun exportScript(entry: NPProtocolEntry): String {
        val item = entry.toNppsCoreItem()
        val flagged = if (entry is NPProtocolEntry.Single && isUsed(entry)) {
            val protocol = JsonObject(item["protocol"]!!.jsonObject + ("isUsed" to JsonPrimitive(true)))
            JsonObject(item + ("protocol" to protocol))
        } else if (entry is NPProtocolEntry.Composite && isUsed(entry)) {
            val composite = JsonObject(item["composite"]!!.jsonObject + ("isUsed" to JsonPrimitive(true)))
            JsonObject(item + ("composite" to composite))
        } else item
        return NppsCore.serialize(listOf(flagged))
    }

    /**
     * Read the first entry of [text]. An entry that arrives flagged used makes its family used, and one that arrives unflagged
     * but whose content is already used here stays used; an import never clears the flag (SWR-WO-03).
     */
    fun importScript(text: String): NPProtocolEntry {
        val entries = NPPSParser.parse(text)
        val entry = entries.firstOrNull() ?: throw NPPSError("No protocol found in script", 0)
        val raw = NppsCore.parse(text)["entries"]?.jsonArray?.firstOrNull()?.jsonObject
        val body = raw?.get("protocol") ?: raw?.get("composite")
        val flagged = body?.jsonObject?.get("isUsed")?.jsonPrimitive?.boolean == true
        val usedHashes = allProtocols.filter { isUsed(it) }.mapNotNull { contentHash(it) }
        val merged = NPLibraryRules.mergeImport(usedHashes, listOf(Triple(entry.id.toString(), contentHash(entry), flagged)))
        if (merged[entry.id.toString()] == true) markUsed(entry)
        return entry
    }

    // MARK: Lookup

    private fun findSingle(name: String): NPProtocolDefinition? =
        allProtocols.firstNotNullOfOrNull { e ->
            (e as? NPProtocolEntry.Single)?.protocol?.takeIf { it.name == name }
        }

    // MARK: Bundled loading

    /**
     * The shipped protocols and composites. Zone and condition definitions load
     * into the same namespace (NP-NPPS-REF-001 §1.6) but are not library items:
     * they are referenced by name, never run, so they are filtered out here
     * rather than shown in a protocol list.
     */
    private fun loadBundledProtocols(): List<NPProtocolEntry> =
        NPBundledProtocols.namespace.runnableEntries

    // MARK: Persistence (NPPS text via KeyValueStore)

    private fun loadFromStore() {
        val text = kv.getString(USER_PROTOCOLS_KEY) ?: return
        if (text.isBlank()) return
        val parsed = runCatching { NPPSParser.parse(text) }.getOrDefault(emptyList())
        _userProtocols.clear()
        _userProtocols.addAll(parsed)
    }

    /**
     * Fold the user's own entries into the namespace zones resolve against.
     *
     * A user may define a `zone` block in their own script (NP-NPPS-REF-001 §8
     * *User-defined zones*), and it has to resolve exactly as a shipped zone
     * does. It reaches [NPZoneRegistry] the same way a shipped one does: parsed
     * out of NPPS text, never authored anywhere else.
     *
     * A user zone colliding with a shipped name is a duplicate definition, so
     * §1.6 applies unchanged — the name binds to neither and the collision is an
     * error. That is deliberate: silently letting either one win would make
     * which sockets get dosed depend on load order.
     */
    private fun publishZoneNamespace() {
        // `bundledProtocols` is deliberately runnable entries only, so the zone
        // and condition blocks are not in it. Take the bundled namespace's full
        // entry list instead — the definitions are exactly what is needed here.
        val build = buildNamespace(NPBundledProtocols.namespace.entries + _userProtocols)
        NPZoneRegistry.use(build.namespace)
        zoneNamespaceErrors = build.errors
    }


    /** The summaries the core decides over: every protocol and composite, the used families, and (when asked) the versions each fits. */
    private fun familySummaries(withVersions: Boolean): JsonArray =
        NPLibraryRules.summaries(allProtocols, usedFamilies, withVersions)

    private fun loadFamilyState() {
        usedFamilies.clear()
        kv.getString(USED_FAMILIES_KEY)?.let { text ->
            runCatching { Json.parseToJsonElement(text).jsonArray }.getOrNull()?.forEach { record ->
                // A stored record with any field but the family id is not one this library wrote: it is dropped, not trusted.
                if (NPLibraryRules.isValidUsedRecord(record)) usedFamilies.add(record.jsonObject["familyId"]!!.jsonPrimitive.content)
            }
        }
        archivedFamilies.clear()
        kv.getString(ARCHIVED_FAMILIES_KEY)?.let { text ->
            runCatching { Json.parseToJsonElement(text).jsonArray }.getOrNull()?.forEach { archivedFamilies.add(it.jsonPrimitive.content) }
        }
        cloneSources.clear()
        kv.getString(CLONE_SOURCES_KEY)?.let { text ->
            runCatching { Json.parseToJsonElement(text).jsonObject }.getOrNull()?.forEach { (k, v) -> cloneSources[k] = v.jsonPrimitive.content }
        }
    }

    private fun saveFamilyState() {
        kv.putString(USED_FAMILIES_KEY, buildJsonArray { usedFamilies.forEach { add(NPLibraryRules.usedRecord(it)) } }.toString())
        kv.putString(ARCHIVED_FAMILIES_KEY, buildJsonArray { archivedFamilies.forEach { add(JsonPrimitive(it)) } }.toString())
        kv.putString(CLONE_SOURCES_KEY, JsonObject(cloneSources.mapValues { JsonPrimitive(it.value) }).toString())
    }

    private fun saveToStore() {
        val serializer = NPPSSerializer()
        // The core refuses a model it cannot write (a PBM block that names no zone) rather than write a file its own
        // parser rejects. One such draft must not take the rest of the library with it: it stays in memory for this
        // session and is left out of the stored text, where before it would have made the whole store unreadable on
        // the next launch.
        val text = _userProtocols.mapNotNull { entry ->
            try {
                serializer.serialize(entry)
            } catch (e: IllegalArgumentException) {
                null
            }
        }.joinToString("\n\n")
        kv.putString(USER_PROTOCOLS_KEY, text)
    }
}
