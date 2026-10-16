package life.neurone.shared.migration

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.BundleSecretResult
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.MigrationFlow
import life.neurone.core.migration.MigrationItemStore
import life.neurone.core.migration.StandardMigrationHost
import life.neurone.core.migration.toJson
import life.neurone.shared.MigrationPlace

// The restore screen's flow, with no Compose in it (SWR-MG-02, SWR-MG-03, SWR-MG-04, SWR-MG-05, SWR-MG-07, SWR-MG-08; issue
// #569 package 6p). Like the backup model it holds the order of the screens and nothing the core decides: the core reads the
// header, refuses a backup it cannot read whole, takes the claim only after the backup has opened, applies each item and reads
// it back, and confirms last.

/** What a restore did, as item codes the screen words with the core's `items` keys, and the two notices the core asks for. */
class RestoreReport(
    val kind: BackupKind?,
    val bundleHash: String,
    val restored: List<String>,
    val missing: List<String>,
    val omitted: List<String>,
    val notApplied: List<String>,
    /** The earlier consent choices were not restored, so the person is told and asked again (SWR-MG-04). */
    val consentNotRestored: Boolean,
    /** The backup was marked as received, which is what lets the old phone clear (SWR-MG-08). Only a migration needs it. */
    val confirmed: Boolean,
)

sealed interface RestoreStep {
    data object NoPlace : RestoreStep

    /** Reading the header of the backup on the place. */
    data object Looking : RestoreStep

    /** The backup is there: ask for what it is locked with. [problem] is why the last try failed. */
    class EnterSecret(val info: BundleInfo, val problem: SecretProblem?) : RestoreStep

    data object Running : RestoreStep

    class Done(val report: RestoreReport) : RestoreStep

    /** The core's reason code; the screen words it with [MigrationBackupModel.messageKey]. */
    class Failed(val reason: String) : RestoreStep
}

class MigrationRestoreModel(
    private val place: MigrationPlace?,
    private val keys: KeyValueStore,
    private val items: List<MigrationItemStore> = emptyList(),
    private val random: RandomSource = SystemRandomSource,
) {
    private val state = MutableStateFlow<RestoreStep>(if (place == null) RestoreStep.NoPlace else RestoreStep.Looking)
    val step: StateFlow<RestoreStep> = state.asStateFlow()

    /** Read the header of the backup on the place. Called once when the screen opens, and again by [retry]. */
    suspend fun look() {
        val place = place ?: return
        state.value = RestoreStep.Looking
        state.value = when (val r = inspectBundle(place, keys, random)) {
            is Inspected.Found -> RestoreStep.EnterSecret(r.info, null)
            is Inspected.Failed -> RestoreStep.Failed(r.reason)
        }
    }

    suspend fun retry() = look()

    /**
     * The recovery code or passphrase the person typed. A secret the core refuses as mistyped, and one that does not open the
     * backup, both leave the person on the same step with the reason: a wrong secret leaves no claim behind, so they may try
     * again (SWR-MG-07).
     */
    suspend fun submitSecret(text: String) {
        val s = state.value as? RestoreStep.EnterSecret ?: return
        val method = s.info.method ?: return
        val parsed = when (method) {
            BundleKeyMethod.RECOVERY_CODE -> BundleKeyDerivation.recoveryCodeSecret(text)
            BundleKeyMethod.PASSPHRASE -> BundleKeyDerivation.passphraseSecret(text)
        }
        when (parsed) {
            is BundleSecretResult.Rejected -> state.value = RestoreStep.EnterSecret(s.info, SecretProblem(parsed.reason, parsed.minChars))
            is BundleSecretResult.Ok -> restore(s.info, parsed.secret)
        }
    }

    private suspend fun restore(info: BundleInfo, secret: BundleSecret) {
        val place = place ?: return
        state.value = RestoreStep.Running
        val host = StandardMigrationHost(items)
        val params = buildJsonObject {
            put("route", place.route.toJson())
            put("hostItems", JsonArray(host.hostItems.map { JsonPrimitive(it) }))
        }
        val done = try {
            MigrationFlow.run("restore", params, host, secret, keys, place.store, random = random)
        } finally {
            secret.destroy()
        }
        if (done["ok"]!!.jsonPrimitive.boolean) {
            fun list(key: String) = (done[key] as? JsonElement)?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList()
            state.value = RestoreStep.Done(
                RestoreReport(
                    kind = BackupKind.entries.firstOrNull { it.wire == done["kind"]?.jsonPrimitive?.content },
                    bundleHash = done["bundleHash"]!!.jsonPrimitive.content,
                    restored = list("restored"),
                    missing = list("missing"),
                    omitted = list("omitted"),
                    notApplied = list("notApplied"),
                    consentNotRestored = done["tellConsentNotRestored"]?.jsonPrimitive?.boolean == true,
                    confirmed = done["confirmed"]?.jsonPrimitive?.boolean == true,
                ),
            )
        } else {
            val reason = done["reason"]!!.jsonPrimitive.content
            // A secret that does not open the backup is the person's to retry; every other reason ends the restore.
            state.value = if (reason == "authentication_failed") {
                RestoreStep.EnterSecret(info, SecretProblem(reason, null, MessageGroup.REASONS))
            } else {
                RestoreStep.Failed(reason)
            }
        }
    }
}
