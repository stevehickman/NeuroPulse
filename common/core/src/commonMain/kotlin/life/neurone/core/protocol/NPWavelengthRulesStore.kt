package life.neurone.core.protocol

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.PresenceState
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.npps.NppsCore

/**
 * The user's PBM wavelength rules (NP-NPPS-REF-001 §7a), port of the web `wavelengthRulesStore`. The shipped defaults
 * ([NPWavelengthRulesEngine.DEFAULT]) with the user's channel windows on top; [resolved] is what the descriptor compiler
 * uses, so what a person sees as runnable is what compiles. The override holds configuration only. A window that does not
 * validate is never saved, and a stored override that no longer validates is dropped on load rather than trusted, so an
 * invalid window cannot widen what a channel accepts by accident.
 */
class NPWavelengthRulesStore(
    private val kv: KeyValueStore,
    /**
     * The presence lock when the host gates UHDR (`NP-APP-PRESENCE-001`), or null when it does not. With a lock this store holds no
     * override while locked, so the shipped windows apply, and it reads the override again at each unlock (`REQ-PRES-01` (d),
     * `OI-PRIV-19`). Used from one thread.
     */
    private val lock: PresenceLock? = null,
) {

    companion object {
        const val USER_KEY = "np.wavelength-rules.user"
        private val json = Json { ignoreUnknownKeys = true }
    }

    var defaults: NPWavelengthRules = NPWavelengthRulesEngine.DEFAULT
        private set

    var userRules: NPWavelengthRules? = null
        private set

    /** Whether [userRules] is what the store holds. False while locked, when a locked read of "absent" is not "no override". */
    val isReadable: Boolean get() = loaded

    private var loaded = false

    /** Changes made while locked, oldest first: each is one channel's window set or reset, replayed on the override read at the unlock. */
    private val held = mutableListOf<() -> Unit>()

    init {
        if (lock == null || lock.isUnlocked) {
            load()
            loaded = true
        }
        lock?.onStateChanged { state ->
            if (state == PresenceState.LOCKED) {
                userRules = null
                loaded = false
            } else {
                load()
                loaded = true
                val operations = held.toList()
                held.clear()
                operations.forEach { it() }
            }
        }
    }

    private fun load() {
        userRules = kv.getString(USER_KEY)
            ?.let { runCatching { fromCoreJson(json.parseToJsonElement(it).jsonObject) }.getOrNull() }
            ?.takeIf { NPWavelengthRulesEngine.validate(it).isEmpty() }
            ?.copy(level = "user")
    }

    /** Hold [op] for the next unlock if the override cannot be read now. True if it was held, and the caller then does nothing more. */
    private fun deferred(op: () -> Unit): Boolean {
        if (loaded) return false
        held += op
        return true
    }

    /** What eligibility and the compiler use: the defaults with the user's channels on top. */
    val resolved: NPWavelengthRules
        get() = NPWavelengthRulesEngine.resolve(defaults, userRules)

    /** The channels the user has changed. */
    val editedChannels: Set<NPPBMChannelElement>
        get() = userRules?.channels?.map { it.element }?.toSet() ?: emptySet()

    /** Called once the library has loaded `00-wavelength-rules.npps`. A set that does not validate is ignored. */
    fun setDefaults(rules: NPWavelengthRules) {
        if (NPWavelengthRulesEngine.validate(rules).isNotEmpty()) return
        defaults = rules
    }

    /** Replace one channel's window in the override. Returns the problems with the result, and saves nothing if there are any. */
    fun setChannel(rule: NPWavelengthChannelRule): List<String> {
        // Held and checked at the unlock, on the override read then: a window that does not validate is dropped there, as it is here.
        if (deferred { setChannel(rule) }) return emptyList()
        val current = userRules ?: NPWavelengthRules(name = "My wavelength rules", level = "user", channels = emptyList())
        val next = current.copy(channels = current.channels.filter { it.element != rule.element } + rule)
        val errors = NPWavelengthRulesEngine.validate(next)
        if (errors.isNotEmpty()) return errors
        userRules = next
        persist()
        return emptyList()
    }

    /** Drop the override for one channel, or for all channels. */
    fun resetChannel(element: NPPBMChannelElement? = null) {
        if (deferred { resetChannel(element) }) return
        val user = userRules ?: return
        val channels = if (element == null) emptyList() else user.channels.filter { it.element != element }
        userRules = if (channels.isEmpty()) null else user.copy(channels = channels)
        persist()
    }

    /** The override as a `wavelength_rules` block (all of the resolved rules when there is no override yet). */
    fun exportUserRules(): String =
        NppsCore.serialize(listOf(buildJsonObject {
            put("kind", "wavelengthRules")
            put("wavelengthRules", toCoreJson(userRules ?: resolved.copy(level = "user")))
        }))

    /** Import a `wavelength_rules` block as the override. Throws on anything invalid; nothing is saved then. */
    fun importUserRules(text: String) {
        if (!loaded) throw IllegalStateException("The wavelength rules cannot be imported while the presence lock is closed.")
        val parsed = NppsCore.parse(text)["wavelengthRules"]?.jsonArray?.firstOrNull()?.jsonObject
            ?: throw IllegalArgumentException("No wavelength_rules block found.")
        val rules = fromCoreJson(parsed).copy(level = "user")
        val errors = NPWavelengthRulesEngine.validate(rules)
        require(errors.isEmpty()) { errors.joinToString("; ") }
        userRules = rules
        persist()
    }

    private fun persist() {
        if (lock != null && !lock.isUnlocked) return
        try {
            userRules?.let { kv.putString(USER_KEY, toCoreJson(it).toString()) } ?: kv.remove(USER_KEY)
        } catch (e: SecretKeyUnavailableException) {
            if (lock == null) throw e
        }
    }
}

/** The shape the NPPS core takes and returns: `{name, level, description?, channels:[{element, nominalNm, minNm, maxNm}]}`. */
fun toCoreJson(rules: NPWavelengthRules): JsonObject = buildJsonObject {
    put("name", rules.name)
    put("level", rules.level)
    rules.description?.let { put("description", it) }
    put("channels", JsonArray(rules.channels.map { c ->
        buildJsonObject {
            put("element", c.element.rawValue)
            put("nominalNm", c.nominalNm)
            put("minNm", c.minNm)
            put("maxNm", c.maxNm)
        }
    }))
}

fun fromCoreJson(o: JsonObject): NPWavelengthRules = NPWavelengthRules(
    name = o["name"]?.jsonPrimitive?.contentOrNull ?: "user wavelength rules",
    level = o["level"]?.jsonPrimitive?.contentOrNull ?: "user",
    description = o["description"]?.jsonPrimitive?.contentOrNull,
    channels = o["channels"]!!.jsonArray.map { c ->
        val ch = c.jsonObject
        NPWavelengthChannelRule(
            element = NPPBMChannelElement.fromRaw(ch["element"]!!.jsonPrimitive.content) ?: error("unknown channel"),
            nominalNm = ch["nominalNm"]!!.jsonPrimitive.doubleOrNull ?: error("nominalNm"),
            minNm = ch["minNm"]!!.jsonPrimitive.doubleOrNull ?: error("minNm"),
            maxNm = ch["maxNm"]!!.jsonPrimitive.doubleOrNull ?: error("maxNm"),
        )
    },
)
