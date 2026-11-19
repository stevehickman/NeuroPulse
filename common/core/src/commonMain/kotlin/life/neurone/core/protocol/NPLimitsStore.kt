package life.neurone.core.protocol

import life.neurone.core.common.KeyValueStore
import life.neurone.core.npps.NppsCore
import life.neurone.core.npps.toNppsCoreLimitsJson
import life.neurone.core.npps.toNppsCoreItem
import life.neurone.core.platform.secureRandomBytes
import kotlinx.serialization.Serializable
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.builtins.MapSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import life.neurone.core.common.UUID

// Port of iOS NPLimitsStore + NPLimitsSet.resolve (app/ios/NeurOne/Protocol/NPLimitsStore.swift,
// NPDosageLimits.swift). Manages dosage limit tiers — global / per-helmet / per-individual —
// and resolves them field-by-field (individual > helmet > global) into the effective NPLimitsSet
// the validator enforces, and the tier each value came from, which the validator names in an issue. The resolution
// is the shared NPPS core's; see resolveLimits below.

// What the store keeps about a person, and nothing else (NP-SW-001 REQ-SW03-32; the per-field rulings are in
// docs/reference/data-architecture-detail.md §5.1). `id` is random and names the individual tier. `name` is how the person
// choosing a profile recognises it. `notes` is a short label that tells two profiles apart; it is not a record about the
// person, nothing reads it but the profile list, and it holds at most ProfileLimits.PROFILE_NOTES_MAX_CHARS UTF-16 code
// units. There is no date of birth: no limit or protocol rule ever read one (OI-PRIV-11).
@Serializable
data class NPIndividualProfile(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val notes: String = "",
)

/**
 * Field-by-field resolution of the three tiers into one effective NPLimitsSet: individual ?? helmet ?? global.
 * A modality block is null (unlimited) only when all three tiers omit it.
 *
 * The rule is the shared NPPS core's (common/npps-core/src/resolve.rs, OI-NPPS-CORE-01), the same one the web and iOS
 * call; this maps the models to its shape and back.
 */
fun resolveLimits(global: NPLimitsSet?, helmet: NPLimitsSet?, individual: NPLimitsSet?): NPLimitsSet =
    resolveLimitsWithSources(global, helmet, individual).first

/**
 * As [resolveLimits], with the tier each value came from (`{"besTacs":{"maxFrequencyHz":"helmet"}}`), which the validator
 * attributes each configured limit by.
 */
fun resolveLimitsWithSources(global: NPLimitsSet?, helmet: NPLimitsSet?, individual: NPLimitsSet?): Pair<NPLimitsSet, JsonObject> {
    val out = NppsCore.resolveLimits(global?.toNppsCoreLimitsJson(), helmet?.toNppsCoreLimitsJson(), individual?.toNppsCoreLimitsJson())
    val limits = out["limits"] as JsonObject
    val resolved = NPPSParser.limitsOf(JsonObject(limits + ("name" to JsonPrimitive("Resolved"))))
    return resolved.copy(level = NPLimitsSet.LimitLevel.GLOBAL) to (out["sources"] as JsonObject)
}

class NPLimitsStore(
    private val kv: KeyValueStore,
    private val randomBytes: (Int) -> ByteArray = ::secureRandomBytes,
) {

    companion object {
        const val GLOBAL_KEY = "np.limits.global"
        const val PROFILES_KEY = "np.limits.profiles"
        const val ACTIVE_PROFILE_KEY = "np.limits.active-profile"
        const val HELMETS_KEY = "np.limits.helmets"
        const val INDIVIDUALS_KEY = "np.limits.individuals"
        private val tierSerializer = MapSerializer(String.serializer(), String.serializer())
        private val profilesSerializer = ListSerializer(NPIndividualProfile.serializer())
        private val json = Json { ignoreUnknownKeys = true }
    }

    var globalLimits: NPLimitsSet? = null
        private set

    // Every tier is persisted. A tier a person configured and the app then forgot would make a cap
    // silently stop applying, so a restart must not drop one. The active profile names the person on
    // the device (per-user cardiac scope, NP-SW-FAULTMSG-001); losing it would make every relaunch
    // "nobody named".
    var helmetLimits: Map<String, NPLimitsSet> = emptyMap()
        private set
    var individualLimits: Map<String, NPLimitsSet> = emptyMap()
        private set
    var profiles: List<NPIndividualProfile> = emptyList()
        private set
    var activeHelmetSerial: String? = null
    var activeProfileId: String? = null
        set(value) {
            if (field == value) return
            field = value
            onActiveProfileChanged?.invoke(value)
        }

    /**
     * Observes the active individual profile — the composition root forwards it to the hub as
     * the device's active user (per-user cardiac scope, NP-SW-FAULTMSG-001). iOS observes
     * NPLimitsStore.activeProfileId the same way.
     */
    var onActiveProfileChanged: ((String?) -> Unit)? = null

    init {
        loadGlobal()
        loadProfiles()
        loadTiers()
    }

    /** individual > helmet > global effective limits for the current active context. */
    val resolvedLimits: NPLimitsSet
        get() = resolve().first

    private fun resolve(): Pair<NPLimitsSet, JsonObject> = resolveLimitsWithSources(
        global = globalLimits,
        helmet = activeHelmetSerial?.let { helmetLimits[it] },
        individual = activeProfileId?.let { individualLimits[it] },
    )

    fun makeValidator(): NPProtocolValidator {
        val (limits, sources) = resolve()
        return NPProtocolValidator(limits, sources)
    }

    // MARK: Mutations

    fun saveGlobalLimits(limits: NPLimitsSet) {
        globalLimits = limits.copy(level = NPLimitsSet.LimitLevel.GLOBAL)
        persistGlobal()
    }

    fun clearGlobalLimits() {
        globalLimits = null
        kv.remove(GLOBAL_KEY)
    }

    fun saveHelmetLimits(limits: NPLimitsSet, serial: String) {
        helmetLimits = helmetLimits + (serial to limits.copy(level = NPLimitsSet.LimitLevel.HELMET, helmetId = serial))
        persistTiers()
    }

    fun clearHelmetLimits(serial: String) {
        helmetLimits = helmetLimits - serial
        persistTiers()
    }

    fun saveIndividualLimits(limits: NPLimitsSet, profileId: String) {
        individualLimits = individualLimits + (profileId to limits.copy(level = NPLimitsSet.LimitLevel.INDIVIDUAL))
        persistTiers()
    }

    fun clearIndividualLimits(profileId: String) {
        individualLimits = individualLimits - profileId
        persistTiers()
    }

    /** Keep [profile]. A note over the bound is refused (false) and nothing changes (REQ-SW03-32). */
    fun saveProfile(profile: NPIndividualProfile): Boolean {
        if (profile.notes.length > ProfileLimits.PROFILE_NOTES_MAX_CHARS) return false
        profiles = if (profiles.any { it.id == profile.id }) {
            profiles.map { if (it.id == profile.id) profile else it }
        } else {
            profiles + profile
        }
        persistProfiles()
        return true
    }

    fun deleteProfile(id: String) {
        profiles = profiles.filterNot { it.id == id }
        individualLimits = individualLimits - id
        if (activeProfileId == id) activeProfileId = null
        persistProfiles()
        persistTiers()
    }

    /**
     * Choose who is using the device (iOS NPLimitsStore.setActiveProfile parity). null =
     * nobody named: the device then keeps assuming whoever it last knew. An id that is not a
     * saved profile is ignored.
     */
    fun setActiveProfile(id: String?) {
        if (id != null && profiles.none { it.id == id }) return
        activeProfileId = id
        persistProfiles()
    }

    val activeProfile: NPIndividualProfile?
        get() = profiles.firstOrNull { it.id == activeProfileId }

    // MARK: NPPS import / export

    /** [limits] in full, with the helmet serial or individual id it belongs to: the form the store keeps and the tier editor edits. */
    fun exportLimitsAsNPPS(limits: NPLimitsSet): String = NPPSSerializer().serializeLimits(limits)

    /**
     * [limits] as a file for someone else (SWR-WO-07): the level is kept and the helmet serial and individual id are not
     * written, so the file names no helmet and no person. The recipient attaches it to the tier they open it in.
     */
    fun shareLimitsAsNPPS(limits: NPLimitsSet): String =
        NppsCore.serializeShare(listOf(NPProtocolEntry.Limits(limits).toNppsCoreItem()), randomBytes(32))

    fun importLimitsFromNPPS(text: String): NPLimitsSet {
        val entries = NPPSParser.parse(text)
        return entries.filterIsInstance<NPProtocolEntry.Limits>().firstOrNull()?.limits
            ?: throw NPPSError("No limits block found in script", 0)
    }

    // MARK: Persistence (global tier, NPPS text)

    private fun loadGlobal() {
        val text = kv.getString(GLOBAL_KEY) ?: return
        globalLimits = runCatching { importLimitsFromNPPS(text) }.getOrNull()
    }

    // What is stored is reduced to what a profile has (REQ-SW03-32): a field it does not have (the date of birth an earlier
    // build of another app kept) is not read, a note over the bound is cut to it, and the stored value is written back in
    // that form, so the dropped field does not stay on the device or travel in a migration bundle.
    private fun loadProfiles() {
        val stored = kv.getString(PROFILES_KEY)
        profiles = stored
            ?.let { runCatching { json.decodeFromString(profilesSerializer, it) }.getOrNull() }
            ?.map { it.copy(notes = it.notes.take(ProfileLimits.PROFILE_NOTES_MAX_CHARS)) }
            ?: emptyList()
        if (stored != null && profiles.isNotEmpty()) {
            val current = json.encodeToString(profilesSerializer, profiles)
            if (current != stored) kv.putString(PROFILES_KEY, current)
        }
        // Restore only an id that still names a saved profile.
        activeProfileId = kv.getString(ACTIVE_PROFILE_KEY)?.takeIf { id -> profiles.any { it.id == id } }
    }

    private fun persistProfiles() {
        kv.putString(PROFILES_KEY, json.encodeToString(profilesSerializer, profiles))
        activeProfileId?.let { kv.putString(ACTIVE_PROFILE_KEY, it) } ?: kv.remove(ACTIVE_PROFILE_KEY)
    }

    private fun persistGlobal() {
        globalLimits?.let { kv.putString(GLOBAL_KEY, exportLimitsAsNPPS(it)) } ?: kv.remove(GLOBAL_KEY)
    }

    // Helmet and individual tiers: one JSON map each, id -> NPPS limits text (the global tier's format).
    // The level and the id are stamped again on load, so a stored block cannot name another tier.
    private fun loadTiers() {
        helmetLimits = readTier(HELMETS_KEY).mapValues { (serial, l) ->
            l.copy(level = NPLimitsSet.LimitLevel.HELMET, helmetId = serial)
        }
        individualLimits = readTier(INDIVIDUALS_KEY)
            .filterKeys { id -> profiles.any { it.id == id } }
            .mapValues { (_, l) -> l.copy(level = NPLimitsSet.LimitLevel.INDIVIDUAL) }
    }

    private fun readTier(key: String): Map<String, NPLimitsSet> {
        val texts = kv.getString(key)
            ?.let { runCatching { json.decodeFromString(tierSerializer, it) }.getOrNull() }
            ?: return emptyMap()
        return texts.mapNotNull { (id, text) -> runCatching { importLimitsFromNPPS(text) }.getOrNull()?.let { id to it } }.toMap()
    }

    private fun persistTiers() {
        writeTier(HELMETS_KEY, helmetLimits)
        writeTier(INDIVIDUALS_KEY, individualLimits)
    }

    private fun writeTier(key: String, tier: Map<String, NPLimitsSet>) {
        if (tier.isEmpty()) { kv.remove(key); return }
        kv.putString(key, json.encodeToString(tierSerializer, tier.mapValues { exportLimitsAsNPPS(it.value) }))
    }
}
