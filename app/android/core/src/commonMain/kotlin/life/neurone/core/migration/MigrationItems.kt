package life.neurone.core.migration

import kotlinx.serialization.json.JsonElement

// What a bundle carries, and where each item lives on this phone (SWR-MG-01).
//
// The core decides which ITEMS a kind carries and which stored KEYS make up each (common/npps/migration-keys.json, read by
// the core and by scripts/check-migration-keys.ts for every platform), so there is no Kotlin copy of the list. An item that
// is plain keys needs no code here: MigrationService reads and writes those through the app's KeyValueStore. An item an
// app stores another way (consent, today) is a MigrationItemStore.

/** One item of a bundle that this phone stores some way other than plain keys: read and written whole. */
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
