package life.neurone.shared.migration

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.SecretKeyStore
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.ClearActions
import life.neurone.core.migration.FileStore
import life.neurone.core.migration.MigrationFlow
import life.neurone.core.migration.StandardMigrationHost
import life.neurone.shared.ble.NeurOneGattManager

// The person's own "erase this device" (desktop audit of the privacy review, item 5, SWR-MG-17). The device-replacement clear needs
// a backup the new phone confirmed; an erase has none. It is the same ordered plan from the core (the data key first, the files
// overwritten and deleted, every stored key forgotten), asked for with `erase`, so no platform writes the order again.

sealed interface EraseStep {
    data object Idle : EraseStep

    data object Running : EraseStep

    /**
     * The erase finished. [keyKept] is true when the data key could not be removed from the system's credential store (a locked
     * keychain, a tool that failed): everything it protected has been overwritten and deleted, so what is left is a key for
     * nothing, and the person is told it can be removed by hand.
     */
    class Done(val keyKept: Boolean) : EraseStep

    /** The core's reason code (`step_failed`, ...). The erase stops at the step and carries on at the next try (`SWR-MG-08`). */
    class Failed(val reason: String) : EraseStep
}

/**
 * The two platform steps of the plan, for an erase. They differ from [SharedClearActions] in one way, and it matters: **an erase
 * must work when the keystore does not**, because a run that cannot read its key (`SWR-MG-13`, `SWR-MG-15`) is the one in which
 * the person has no other way out. A key that cannot be destroyed is reported ([keyKept]) and the erase goes on, since the files
 * the key opened are overwritten and deleted by the next step. The pairing record is likewise best effort.
 */
class EraseActions(
    private val secrets: SecretKeyStore?,
    private val gatt: NeurOneGattManager,
) : ClearActions {
    var keyKept: Boolean = false
        private set

    override suspend fun run(step: String): Boolean {
        when (step) {
            "destroyEncryptionKey" -> try {
                secrets?.destroy()
            } catch (ignored: Exception) {
                keyKept = true
            }
            "forgetPairingRecord" -> try {
                gatt.forgetHub()
            } catch (ignored: Exception) {
                // The link to the helmet is forgotten with the stored keys; the central's own record is not worth stopping for.
            }
            else -> return false
        }
        return true
    }
}

/**
 * Runs the erase against the host's **raw** [keys]: in a locked run the protected store holds the sealed values in memory and
 * would leave them on disk, and an erase has to remove what is on disk.
 */
class EraseDeviceModel(
    private val keys: KeyValueStore,
    private val files: FileStore?,
    private val secrets: SecretKeyStore?,
    private val gatt: NeurOneGattManager,
    private val random: RandomSource = SystemRandomSource,
) {
    private val state = MutableStateFlow<EraseStep>(EraseStep.Idle)
    val step: StateFlow<EraseStep> = state.asStateFlow()

    suspend fun erase() {
        if (state.value is EraseStep.Running) return
        state.value = EraseStep.Running
        val actions = EraseActions(secrets, gatt)
        val params = buildJsonObject {
            put("erase", true)
            put("canRemoveOsBond", false)
        }
        state.value = try {
            val done = MigrationFlow.run("clear", params, StandardMigrationHost(emptyList(), null, actions), null, keys, null, files, random = random)
            if (done["ok"]!!.jsonPrimitive.boolean) EraseStep.Done(actions.keyKept) else EraseStep.Failed(done["reason"]!!.jsonPrimitive.content)
        } catch (ex: Exception) {
            EraseStep.Failed("step_failed")
        }
    }
}
