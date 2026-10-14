package life.neurone.core.migration

import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.common.KeyValueStore

// What a bundle carries, and where each item lives on this phone (SWR-MG-01).
//
// The core decides which ITEMS a kind carries (neurone_npps_core::migration). This file decides which stored KEYS make up
// an item, and says so for every key the app writes, so a new key cannot move by accident or be forgotten (the
// "definition that survives new keys" of OI-UI-KMP-11 (d)): MigrationKeyMapTests fails when a key literal in the sources
// is in none of the lists below.

/** One item of a bundle, as this phone stores it. */
interface MigrationItemStore {
    /** The core's item name: `protocols`, `sessionHistory`, `consentGrants`, `withdrawalState`, `characterisationEnrolment`, `limitsProfiles`. */
    val item: String

    /** The item's current value, or null when this phone holds none (it is then left out of the bundle). */
    fun export(): JsonElement?

    /** Replace the item with a value from a bundle. */
    fun import(value: JsonElement)

    /** Forget the item (SWR-MG-06, SWR-MG-09: what the old phone does when it clears). */
    fun clear()
}

object MigrationKeyMap {
    /** Items stored as plain keys in the [KeyValueStore]. */
    val carried: Map<String, List<String>> = mapOf(
        "protocols" to listOf("np.user-protocols.npps", "np.used-families", "np.archived-families", "np.clone-sources"),
        "sessionHistory" to listOf("np.session.history"),
        // `np.limits.global` is the user's own limits, set beside their profiles. UC-088 records that this is a reading.
        "limitsProfiles" to listOf("np.limits.global", "np.limits.profiles", "np.limits.active-profile"),
        "characterisationEnrolment" to listOf(
            "np.characterisation.enrolled",
            "np.characterisation.consent-epoch",
            "np.characterisation.programme-id",
        ),
    )

    /**
     * Consent state. `consentGrants` and `withdrawalState` are not whole keys: both live inside the ConsentStore's JSON
     * (`blanketConsentWithdrawnOnDay` sits in `np.consent.research` beside the grants), so a [ConsentMigrationAdapter]
     * splits and joins them, and these keys are cleared with the consent items and moved by no one else.
     */
    val consent: List<String> = listOf(
        "np.consent.research",
        "np.consent.clinician-grants",
        "np.consent.clinician-expansions",
        "np.consent.study-invitations",
        "np.consent.study-participations",
        "np.research.consent-granted",
        "np.research.suggestions",
    )

    /** Tied to this install, a device or a warranty registration, and never moved (SWR-MG-01). */
    val perInstall: List<String> = listOf(
        "np.research.device-token",
        "np.shdr.device-token",
        "np.warranty.consent-granted",
        "np.analytics.launch-count",
        "ble.lastHubId",
    )

    /** Not yet sorted by the requirement (SWR-MG-01): neither moved nor dropped by this build. */
    val unsorted: List<String> = listOf(
        "np.consumable.snooze-counts",
        "np.consumable.pending-resets",
        "np.cvns.fault-ledger.last-acknowledged-session",
        "np.onboarding.age-confirmed",
        "np.onboarding.bipa-accepted",
        "np.onboarding.consent-shown",
        "np.setup.first-complete",
    )

    val all: Set<String> = carried.values.flatten().toSet() + consent + perInstall + unsorted
}

/**
 * An item that is one or more plain keys. Its value is a JSON object of key to stored text, so a restore writes back
 * exactly what was read, whatever each key's own format is; nothing here parses the text.
 */
class KeyValueItemStore(
    override val item: String,
    private val keys: List<String>,
    private val store: KeyValueStore,
) : MigrationItemStore {
    override fun export(): JsonElement? {
        val present = keys.mapNotNull { k -> store.getString(k)?.let { k to JsonPrimitive(it) } }
        return if (present.isEmpty()) null else buildJsonObject { present.forEach { (k, v) -> put(k, v) } }
    }

    override fun import(value: JsonElement) {
        val o = value as? JsonObject ?: throw IllegalArgumentException("item '$item' is not an object")
        // Keys outside this item are refused whole: a forged bundle cannot write another item's keys through this one.
        require(o.keys.all { it in keys }) { "item '$item' names a key it does not own" }
        keys.forEach { store.remove(it) }
        o.forEach { (k, v) -> store.putString(k, v.jsonPrimitive.content) }
    }

    override fun clear() {
        keys.forEach { store.remove(it) }
    }

    companion object {
        /** The four plain-key items over one store. */
        fun allOver(store: KeyValueStore): List<MigrationItemStore> =
            MigrationKeyMap.carried.map { (item, keys) -> KeyValueItemStore(item, keys, store) }
    }
}

/**
 * Consent in a bundle. The shapes are the core's: `withdrawalState` is
 * `{"blanketConsentWithdrawnAt": string|null, "studyWithdrawals": [id], "categoryWithdrawals": [id]}` (mergeWithdrawal).
 * Reading the ConsentStore into these shapes, and writing a merged state back without resetting a withdrawal, is a
 * change to the consent engine (`CLAUDE.md` §6) that is waiting on a ruling: the app stores `blanketConsentWithdrawnOnDay`
 * (a day, not a time) and records no per-category withdrawal at all, so a withdrawn category cannot yet be told from one
 * never granted. Until then no adapter is wired, and a bundle's consent items are reported as not applied.
 */
interface ConsentMigrationAdapter {
    val grants: MigrationItemStore
    val withdrawal: MigrationItemStore
}
