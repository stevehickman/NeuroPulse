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
import life.neurone.core.npps.NppsCore

/**
 * The user's PBM wavelength rules (NP-NPPS-REF-001 §7a), port of the web `wavelengthRulesStore`. The shipped defaults
 * ([NPWavelengthRulesEngine.DEFAULT]) with the user's channel windows on top; [resolved] is what the descriptor compiler
 * uses, so what a person sees as runnable is what compiles. The override holds configuration only. A window that does not
 * validate is never saved, and a stored override that no longer validates is dropped on load rather than trusted, so an
 * invalid window cannot widen what a channel accepts by accident.
 */
class NPWavelengthRulesStore(private val kv: KeyValueStore) {

    companion object {
        const val USER_KEY = "np.wavelength-rules.user"
        private val json = Json { ignoreUnknownKeys = true }
    }

    var defaults: NPWavelengthRules = NPWavelengthRulesEngine.DEFAULT
        private set

    var userRules: NPWavelengthRules? = null
        private set

    init {
        userRules = kv.getString(USER_KEY)
            ?.let { runCatching { fromCoreJson(json.parseToJsonElement(it).jsonObject) }.getOrNull() }
            ?.takeIf { NPWavelengthRulesEngine.validate(it).isEmpty() }
            ?.copy(level = "user")
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
        val parsed = NppsCore.parse(text)["wavelengthRules"]?.jsonArray?.firstOrNull()?.jsonObject
            ?: throw IllegalArgumentException("No wavelength_rules block found.")
        val rules = fromCoreJson(parsed).copy(level = "user")
        val errors = NPWavelengthRulesEngine.validate(rules)
        require(errors.isEmpty()) { errors.joinToString("; ") }
        userRules = rules
        persist()
    }

    private fun persist() {
        userRules?.let { kv.putString(USER_KEY, toCoreJson(it).toString()) } ?: kv.remove(USER_KEY)
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
