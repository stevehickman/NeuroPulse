package life.neurone.core.migration

import kotlinx.serialization.json.JsonElement
import life.neurone.core.consent.ConsentStore

// What a bundle carries, and where each item lives on this phone (SWR-MG-01).
//
// The core decides which ITEMS a kind carries and which stored KEYS make up each (common/npps/migration-keys.json, read by
// the core and by scripts/check-migration-keys.ts for every platform), so there is no Kotlin copy of the list. An item that
// is plain keys needs no code here: MigrationFlow reads and writes those through the app's KeyValueStore. An item an
// app stores another way (the withdrawal records) is a MigrationItemStore.

/** One item of a bundle that this phone stores some way other than plain keys: read and written whole. */
interface MigrationItemStore {
    /** The core's item name: `protocols`, `sessionHistory`, `withdrawalState`, `characterisationEnrolment`, `limitsProfiles`. */
    val item: String

    /** The item's current value, or null when this phone holds none (it is then left out of the bundle). */
    fun export(): JsonElement?

    /** Replace the item with a value from a bundle. */
    fun import(value: JsonElement)

    /** Forget the item (SWR-MG-06, SWR-MG-09: what the old phone does when it clears). */
    fun clear()
}

/**
 * Consent in a bundle: the withdrawal records, and nothing else. **Grants are never carried** (ruling, issue #569): a grant
 * is stated afresh on each device, so an out-of-date one can never be restored, and a restore can only reduce what a phone
 * is consented to. The shape is the core's `withdrawalState` (`mergeWithdrawal`): the withdrawal times (UTC instants) and
 * `categoryWithdrawals` / `studyWithdrawals` as id → time. A withdrawal is sticky: the later time wins and none is erased.
 */
class StoreConsentMigrationAdapter(private val consent: ConsentStore) : MigrationItemStore {
    override val item: String = "withdrawalState"

    override fun export(): JsonElement = consent.exportWithdrawals()

    override fun import(value: JsonElement) = consent.applyWithdrawals(value)

    /** The clear plan wipes the consent keys (`migration-keys.json`), so there is nothing more to forget here. */
    override fun clear() = Unit
}
