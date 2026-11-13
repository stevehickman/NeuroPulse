package life.neurone.shared.migration

import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.MigrationFlow
import life.neurone.core.migration.StandardMigrationHost
import life.neurone.shared.MigrationPlace

// The capability check of a place (SWR-MG-12, issue #569). The steps and the verdict are the core's (`flow: "probe"`, run on
// throwaway objects that hold no user data, always deleted); this asks it once and names the locale keys that word the answer,
// from the core's table, so no screen spells one its own way. It proves capability, not that a claim is atomic on a provider
// (SWR-MG-07, #566): a provider's guarantee is verified against its documentation, not by this call.

/** The check's three outcomes (SWR-MG-12). */
enum class StorageVerdict(val wire: String) {
    /** Migration and the health-data backup both work here. */
    SUFFICIENT("sufficient"),

    /** Only the health-data backup works; the place stays listed for it. */
    BACKUP_ONLY("backup_only"),

    /** Neither works. */
    INSUFFICIENT("insufficient"),
}

/**
 * What the check found. [messageKey] words the verdict; [operationKey] words the failed operation and what the person can do,
 * and is null when nothing failed. [failedOperation] is the core's name for it.
 */
class StorageCheck(val verdict: StorageVerdict, val failedOperation: String?, val messageKey: String, val operationKey: String?) {
    val canBackUp: Boolean get() = verdict != StorageVerdict.INSUFFICIENT
    val canMigrate: Boolean get() = verdict == StorageVerdict.SUFFICIENT
}

object StorageCheckModel {
    /**
     * Check [place] for a bundle of [bundleBytes]. A store that throws on a call is a failed operation, never an exception here
     * (the core ends the check with the operation named and removes what it made). The route's own rule still applies: a place
     * that cannot promise an atomic claim is withheld from a migration whatever this finds (`SWR-MG-10`), so a SUFFICIENT
     * verdict on such a place is lowered to BACKUP_ONLY here.
     */
    suspend fun check(place: MigrationPlace, bundleBytes: Long, random: RandomSource = SystemRandomSource): StorageCheck {
        val done = MigrationFlow.probe(place.store, bundleBytes, StandardMigrationHost(emptyList()), random)
        var verdict = StorageVerdict.entries.first { it.wire == done["result"]!!.jsonPrimitive.content }
        var failed = done["failedOperation"]?.jsonPrimitive?.content
        if (verdict == StorageVerdict.SUFFICIENT && !place.route.atomicClaim) {
            verdict = StorageVerdict.BACKUP_ONLY
            failed = "createIfAbsentSecondRefused"
        }
        return StorageCheck(
            verdict,
            failed,
            MigrationBackupModel.messageKey(MessageGroup.PROBE, verdict.wire)!!,
            failed?.let { MigrationBackupModel.messageKey(MessageGroup.PROBE_OPS, it)!! },
        )
    }
}
