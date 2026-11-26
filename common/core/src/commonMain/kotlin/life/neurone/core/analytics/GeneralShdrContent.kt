package life.neurone.core.analytics

/**
 * One version of what general SHDR consent covers (`REQ-SHDR-12`; `NP-FW-NVRAM-001` D-39; `NP-MOD-ID-001` §7.4).
 *
 * [fields] names every item the set covers beyond the baseline, by an id the device and the app can both name. Version 0 is the
 * baseline itself (the device-condition fields general consent covers today) and names nothing beyond it. A set says nothing
 * about any owner, helmet or person: it is the same for everyone.
 */
data class GeneralShdrContentSet(val version: Int, val fields: List<String>)

/**
 * The general-consent content sets this app carries (`OI-NVRAM-28` (1), (2)).
 *
 * WHY IN THE APP. The principal ruled (2026-11-26) that the analysis-done notice is "broadcast through an app update mechanism to
 * all warranty owners": the same to every owner and never targeted (the pattern of `CLAUDE.md` §6.3). An app build is that: every
 * install of a release carries the same sets, nothing is fetched, no server learns who received it, and nothing names an owner.
 * The review gate's result arrives as a new entry in [history] in an app update, together with the plain words for each item it
 * adds (in `locales/<bcp47>.json`, `CLAUDE.md` §17). Until the gate rules, the only set is the baseline.
 *
 * NOT BUILT HERE: the helmet's side. The device must also know the set and send no field above the version its owner agreed to
 * (`OI-NVRAM-28` (1), (3)), and how it learns the version is not designed. The app records the version ([WarrantyOwnerRecord.contentLevel])
 * and never raises it without the owner's yes.
 */
object GeneralShdrContent {

    /** The baseline: what general consent covers until the `NP-MOD-ID-001` §7.4 review gate rules. */
    val BASELINE = GeneralShdrContentSet(0, emptyList())

    /**
     * Every set this app knows, oldest first. The last is the one in force. A set the gate later fixes is appended here, never
     * written over an earlier one, so the app can still name what an owner agreed to before.
     */
    val history: List<GeneralShdrContentSet> = listOf(BASELINE)

    /** The set in force in this build. */
    val shipped: GeneralShdrContentSet get() = history.last()

    private val FIELD_ID = Regex("^[a-z][a-z0-9_]{0,63}$")

    /**
     * True when [sets] is a history the app may act on: it starts at the baseline, versions rise by one, every id is well formed
     * and named once in a set. A history that is not is never shown and changes nothing (see [GeneralShdrConsentPrompts]); what the
     * owner should be told then is the principal's question (`docs/human-input/oi-nvram-26-28-general-consent-prompts.md`).
     */
    fun isWellFormed(sets: List<GeneralShdrContentSet>): Boolean {
        if (sets.isEmpty() || sets.first() != BASELINE) return false
        return sets.withIndex().all { (i, set) ->
            set.version == i && set.fields.all(FIELD_ID::matches) && set.fields.toSet().size == set.fields.size
        }
    }

    /**
     * What [to] covers that [from] does not, in [to]'s order: the items an owner who agreed to [from] has not agreed to. A [from]
     * the history does not hold (an owner's level from a newer build, then downgraded) is read as the baseline, so nothing [to]
     * covers is passed over unnamed.
     */
    fun added(sets: List<GeneralShdrContentSet>, from: Int, to: GeneralShdrContentSet): List<String> {
        val had = sets.firstOrNull { it.version == from }?.fields.orEmpty().toSet()
        return to.fields.filter { it !in had }
    }
}
