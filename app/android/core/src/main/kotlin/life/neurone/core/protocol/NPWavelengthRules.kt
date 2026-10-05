package life.neurone.core.protocol

// Port of common/lib/wavelengthRules.ts (the reference), NP-NPPS-REF-001
// Rev 17 §4.1a and §7a.
//
// A protocol states the wavelength its source used (`wavelength: "810nm"`).
// These rules say which emitter channel, if any, may deliver it. They are
// configuration, not physics: the defaults ship in
// protocols/predefined/00-wavelength-rules.npps, and a user may loosen or
// tighten them. A wavelength no rule accepts maps to NOTHING — it is never
// moved to the nearest channel, and never driven on every channel.

enum class NPPBMChannelElement(val rawValue: String, val nominalOrder: Int) {
    LED_660("led_660", 0),
    LED_808("led_808", 1),
    LED_1064("led_1064", 2);

    companion object {
        fun fromRaw(raw: String): NPPBMChannelElement? = entries.firstOrNull { it.rawValue == raw }
    }
}

data class NPWavelengthChannelRule(
    val element: NPPBMChannelElement,
    /** Only breaks a tie between two accepting channels: nearest nominal wins. */
    val nominalNm: Double,
    val minNm: Double,
    val maxNm: Double,
)

data class NPWavelengthRules(
    val name: String,
    /** "global" (shipped) or "user" (an override). */
    val level: String = "global",
    val description: String? = null,
    val channels: List<NPWavelengthChannelRule>,
)

sealed class NPParsedWavelength {
    abstract val value: String
    /** A combined channel name that welded independent emitters into one block (Rev 18). */
    data class Retired(override val value: String, val replacement: List<String>) : NPParsedWavelength()
    data class Single(override val value: String, val nm: Double) : NPParsedWavelength()
    data class Invalid(override val value: String) : NPParsedWavelength()
}

sealed class NPPbmChannelResolution {
    data class Ok(val elements: List<NPPBMChannelElement>, val requestedNm: Double?) : NPPbmChannelResolution()
    data class Refused(val reason: String, val value: String, val requestedNm: Double?) : NPPbmChannelResolution()
}

object NPWavelengthRulesEngine {

    /**
     * The shipped defaults. Mirrors protocols/predefined/00-wavelength-rules.npps and
     * DEFAULT_WAVELENGTH_RULES in the web reference; NPWavelengthRulesTests fails if
     * this copy and the file differ. Each window is the channel band in CLAUDE.md §3
     * widened 10 nm each side — an UNVALIDATED DEFAULT, not a safety parameter.
     */
    val DEFAULT = NPWavelengthRules(
        name = "NeurOne default wavelength mapping",
        level = "global",
        channels = listOf(
            NPWavelengthChannelRule(NPPBMChannelElement.LED_660, 660.0, 650.0, 680.0),
            NPWavelengthChannelRule(NPPBMChannelElement.LED_808, 808.0, 798.0, 840.0),
            NPWavelengthChannelRule(NPPBMChannelElement.LED_1064, 1064.0, 1054.0, 1074.0),
        ),
    )

    /**
     * The two combined channel names the language used to accept (NP-NPPS-REF-001 Rev 18).
     * RETIRED: each wavelength is its own block. `"1064nm"` was never one of them in
     * substance; it is a single wavelength the default rules map to the 1064 nm channel.
     */
    val RETIRED: Map<String, List<String>> = mapOf(
        "660_808nm" to listOf("660nm", "808nm"),
        "660_808_1064nm" to listOf("660nm", "808nm", "1064nm"),
    )

    /** The refusal text for a retired name, naming the blocks that replace it. */
    fun retiredMessage(value: String): String {
        val blocks = (RETIRED[value] ?: emptyList()).joinToString(" and ") { "\"$it\"" }
        return "wavelength \"$value\" is retired: it welded independent emitters into one block. " +
            "Write one block per wavelength ($blocks), each with its own irradiance."
    }

    private val SINGLE_NM = Regex("^([0-9]+(?:\\.[0-9]+)?)nm$")

    fun parse(value: String): NPParsedWavelength {
        RETIRED[value]?.let { return NPParsedWavelength.Retired(value, it) }
        val m = SINGLE_NM.matchEntire(value) ?: return NPParsedWavelength.Invalid(value)
        val nm = m.groupValues[1].toDoubleOrNull()
        return if (nm != null && nm > 0.0) NPParsedWavelength.Single(value, nm) else NPParsedWavelength.Invalid(value)
    }

    /** User channels replace default channels one for one; untouched channels keep the default. */
    fun resolve(defaults: NPWavelengthRules, user: NPWavelengthRules?): NPWavelengthRules {
        if (user == null) return defaults
        val byElement = LinkedHashMap<NPPBMChannelElement, NPWavelengthChannelRule>()
        defaults.channels.forEach { byElement[it.element] = it }
        user.channels.forEach { byElement[it.element] = it }
        return NPWavelengthRules(
            name = user.name,
            level = "user",
            description = user.description,
            channels = NPPBMChannelElement.entries.mapNotNull { byElement[it] },
        )
    }

    fun validate(rules: NPWavelengthRules): List<String> {
        val errors = ArrayList<String>()
        val seen = HashSet<NPPBMChannelElement>()
        for (c in rules.channels) {
            if (!seen.add(c.element)) errors.add("channel '${c.element.rawValue}' has more than one rule")
            if (c.nominalNm <= 0.0 || c.minNm <= 0.0 || c.maxNm <= 0.0 ||
                c.nominalNm.isNaN() || c.minNm.isNaN() || c.maxNm.isNaN()
            ) {
                errors.add("channel '${c.element.rawValue}': nominal_nm, min_nm and max_nm must be positive numbers")
            }
            if (c.minNm > c.maxNm) {
                errors.add("channel '${c.element.rawValue}': min_nm ${c.minNm} is above max_nm ${c.maxNm}")
            }
        }
        return errors
    }

    /** Nearest nominal among the accepting windows; ties go to the earlier channel (660, 808, 1064). */
    fun map(nm: Double, rules: NPWavelengthRules): NPPBMChannelElement? {
        var best: NPWavelengthChannelRule? = null
        for (element in NPPBMChannelElement.entries) {
            val rule = rules.channels.firstOrNull { it.element == element } ?: continue
            if (nm < rule.minNm || nm > rule.maxNm) continue
            val b = best
            if (b == null || kotlin.math.abs(nm - rule.nominalNm) < kotlin.math.abs(nm - b.nominalNm)) best = rule
        }
        return best?.element
    }

    fun resolveChannels(value: String, rules: NPWavelengthRules = DEFAULT): NPPbmChannelResolution =
        when (val w = parse(value)) {
            is NPParsedWavelength.Retired -> NPPbmChannelResolution.Refused("retired", value, null)
            is NPParsedWavelength.Invalid -> NPPbmChannelResolution.Refused("invalid", value, null)
            is NPParsedWavelength.Single -> map(w.nm, rules)
                ?.let { NPPbmChannelResolution.Ok(listOf(it), w.nm) }
                ?: NPPbmChannelResolution.Refused("unmapped", value, w.nm)
        }

    /** True when the value drives CH_C, so the socket needs a 1064 nm (smart) module. */
    fun requiresSmartModule(value: String, rules: NPWavelengthRules = DEFAULT): Boolean =
        (resolveChannels(value, rules) as? NPPbmChannelResolution.Ok)
            ?.elements?.contains(NPPBMChannelElement.LED_1064) == true
}
