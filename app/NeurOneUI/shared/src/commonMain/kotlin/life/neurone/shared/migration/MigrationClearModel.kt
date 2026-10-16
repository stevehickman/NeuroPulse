package life.neurone.shared.migration

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.ClearActions
import life.neurone.core.migration.FileStore
import life.neurone.core.migration.MigrationFlow
import life.neurone.core.migration.MigrationItemStore
import life.neurone.core.migration.StandardMigrationHost
import life.neurone.shared.MigrationPlace

// The old phone's clear screen, with no Compose in it (SWR-MG-06, SWR-MG-08, SWR-MG-09; issue #569 package 6p). The core
// clears only against the new phone's confirmation of the very backup on the place, never on a timer, and an interrupted
// clear resumes at the step it reached. This holds the order of the screens: look at the backup, ask the person to confirm
// an erase that cannot be undone, run the clear.

sealed interface ClearStep {
    /** This host cannot clear itself: it has nowhere a backup could be, or does not supply the platform's two clear steps. */
    data object NotAvailable : ClearStep

    data object Looking : ClearStep

    /** The backup on file; the person must confirm before anything is erased. */
    class Confirm(val info: BundleInfo) : ClearStep

    data object Running : ClearStep

    /** The clear finished. [removeBluetoothBond] is true when the person must also remove the pairing in the system settings. */
    class Done(val removeBluetoothBond: Boolean) : ClearStep

    /** The core's reason code (`no_confirmation`, `other_bundle`, `step_failed`, ...); a clear that stopped part way carries on at the next try. */
    class Failed(val reason: String) : ClearStep
}

class MigrationClearModel(
    private val place: MigrationPlace?,
    private val keys: KeyValueStore,
    private val actions: ClearActions?,
    private val files: FileStore? = null,
    private val items: List<MigrationItemStore> = emptyList(),
    private val random: RandomSource = SystemRandomSource,
) {
    private val state = MutableStateFlow<ClearStep>(if (place == null || actions == null) ClearStep.NotAvailable else ClearStep.Looking)
    val step: StateFlow<ClearStep> = state.asStateFlow()

    private var info: BundleInfo? = null

    suspend fun look() {
        val place = place ?: return
        if (actions == null) return
        state.value = ClearStep.Looking
        state.value = when (val r = inspectBundle(place, keys, random)) {
            is Inspected.Found -> { info = r.info; ClearStep.Confirm(r.info) }
            is Inspected.Failed -> ClearStep.Failed(r.reason)
        }
    }

    /** The person has confirmed the erase. Clears against the backup that was shown: the core refuses if the new phone confirmed another. */
    suspend fun clear() {
        val place = place ?: return
        val actions = actions ?: return
        val shown = (state.value as? ClearStep.Confirm)?.info ?: info ?: return
        state.value = ClearStep.Running
        val host = StandardMigrationHost(items, null, actions)
        val params = buildJsonObject {
            put("bundleHash", shown.bundleHash)
            put("canRemoveOsBond", false)
        }
        val done = MigrationFlow.run("clear", params, host, null, keys, place.store, files, random = random)
        state.value = if (done["ok"]!!.jsonPrimitive.boolean) {
            ClearStep.Done(done["removeOsBond"]?.jsonPrimitive?.boolean == true)
        } else {
            ClearStep.Failed(done["reason"]!!.jsonPrimitive.content)
        }
    }

    /** After a failure the person may look again; a clear that stopped part way resumes at the step it reached (SWR-MG-08). */
    suspend fun retry() = look()
}
