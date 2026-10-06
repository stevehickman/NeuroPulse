package life.neurone.core.protocol

import life.neurone.core.common.KeyValueStore
import life.neurone.core.npps.NppsCore
import life.neurone.core.npps.toNppsCoreLimitsJson
import kotlinx.serialization.Serializable
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import java.util.UUID

// Port of iOS NPLimitsStore + NPLimitsSet.resolve (app/ios/NeurOne/Protocol/NPLimitsStore.swift,
// NPDosageLimits.swift). Manages dosage limit tiers — global / per-helmet / per-individual —
// and resolves them field-by-field (individual > helmet > global) into the effective NPLimitsSet
// the validator enforces, and the tier each value came from, which the validator names in an issue. The resolution
// is the shared NPPS core's; see resolveLimits below.

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

class NPLimitsStore(private val kv: KeyValueStore) {

    companion object {
        const val GLOBAL_KEY = "np.limits.global"
        const val PROFILES_KEY = "np.limits.profiles"
        const val ACTIVE_PROFILE_KEY = "np.limits.active-profile"
        private val profilesSerializer = ListSerializer(NPIndividualProfile.serializer())
        private val json = Json { ignoreUnknownKeys = true }
    }

    var globalLimits: NPLimitsSet? = null
        private set

    // Helmet/individual limit tiers are in-memory for now (global is the editable/persisted tier);
    // persisting those maps is a follow-up (OI-AND-LIMITS-01). Profiles and the active profile ARE
    // persisted: the active profile names the person on the device (per-user cardiac scope,
    // NP-SW-FAULTMSG-001), and losing it on restart would make every relaunch "nobody named".
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
    }

    fun saveIndividualLimits(limits: NPLimitsSet, profileId: String) {
        individualLimits = individualLimits + (profileId to limits.copy(level = NPLimitsSet.LimitLevel.INDIVIDUAL))
    }

    fun saveProfile(profile: NPIndividualProfile) {
        profiles = if (profiles.any { it.id == profile.id }) {
            profiles.map { if (it.id == profile.id) profile else it }
        } else {
            profiles + profile
        }
        persistProfiles()
    }

    fun deleteProfile(id: String) {
        profiles = profiles.filterNot { it.id == id }
        individualLimits = individualLimits - id
        if (activeProfileId == id) activeProfileId = null
        persistProfiles()
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

    fun exportLimitsAsNPPS(limits: NPLimitsSet): String = NPPSSerializer().serializeLimits(limits)

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

    private fun loadProfiles() {
        profiles = kv.getString(PROFILES_KEY)
            ?.let { runCatching { json.decodeFromString(profilesSerializer, it) }.getOrNull() }
            ?: emptyList()
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
}
