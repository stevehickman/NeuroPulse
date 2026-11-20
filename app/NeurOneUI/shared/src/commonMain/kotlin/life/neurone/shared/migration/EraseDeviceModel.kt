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
     *
     * [cloudAccessMayRemain] is true when the person had cloud sign-ins here and not every provider confirmed it ended the app's access (it has
     * no call for that, the call failed, or the sign-ins could not be read): the credentials are erased from this device all the same, and the
     * person is told a service may still list the app, and that a key or password they typed still works there (SWR-MG-30 (d)).
     *
     * [analyticsIdsNotErased] is how many pseudonymous analytics IDs the vendor's erasure relay had not accepted when the wipe
     * deleted them (the relay could not be reached, or refused): the vendor's events under them can no longer be erased by this
     * device (`OI-PRIV-08` (1), `OI-ANALYTICS-01` (5)). Zero in the ordinary case, where there was no ID or the relay took it.
     */
    class Done(val keyKept: Boolean, val analyticsIdsNotErased: Int = 0, val cloudAccessMayRemain: Boolean = false) : EraseStep

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
 * What a locked run cannot send to the vendor before an erase (`OI-ANALYTICS-01` (5) (b)). A locked run reads its sealed keys as
 * absent, so the analytics gate sees no ID and no queue and reports nothing left, while the [raw] store still holds them sealed
 * and the wipe is about to delete them. Each sealed value found counts as one: how many IDs the queue holds cannot be read, and
 * "not all erased" is what the person is told. Zero in any other run, where the gate could read them and its own count stands.
 */
internal fun unreadableAnalyticsIds(protection: life.neurone.core.common.StorageProtection, raw: KeyValueStore): Int {
    if (protection != life.neurone.core.common.StorageProtection.LOCKED) return 0
    return listOf(
        life.neurone.core.analytics.PostHogAnalyticsBackend.ID_KEY,
        life.neurone.core.analytics.AnalyticsErasureQueue.KEY,
    ).count { raw.getString(it) != null }
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
    /**
     * Run before anything is wiped: what has to be said to someone else while the data it needs still exists. The composition
     * root gives it `ResearchAnalyticsGate.resetForErase`, which asks the vendor to erase this install's analytics ID and returns
     * how many IDs are left unanswered. It never stops the erase: a failure here counts as nothing erased and the wipe goes on
     * (`SWR-MG-17` (b), `REQ-SW03-19` (d)).
     */
    private val beforeWipe: (suspend () -> Int)? = null,
    /**
     * Ends the cloud sign-ins before the erase (`CloudAuth.signOutAll`), so a refresh token is not left working at a provider after the device
     * that held it is wiped; answers true when a provider's access may remain. Null on a host with no cloud sign-in. It is best effort and
     * **never stops the erase**: a throw reads as "may remain", and the stored credentials go with every other key in the plan below.
     */
    private val endCloudAccess: (suspend () -> Boolean)? = null,
) {
    private val state = MutableStateFlow<EraseStep>(EraseStep.Idle)
    val step: StateFlow<EraseStep> = state.asStateFlow()

    suspend fun erase() {
        if (state.value is EraseStep.Running) return
        state.value = EraseStep.Running
        val notErased = try {
            beforeWipe?.invoke() ?: 0
        } catch (cancelled: kotlinx.coroutines.CancellationException) {
            state.value = EraseStep.Idle
            throw cancelled
        } catch (ignored: Exception) {
            // A locked or unreadable store, a transport that threw: the person's erase is not held back by the vendor's.
            UNKNOWN_NOT_ERASED
        }
        val cloudMayRemain = try {
            endCloudAccess?.invoke() ?: false
        } catch (cancelled: kotlinx.coroutines.CancellationException) {
            state.value = EraseStep.Idle
            throw cancelled
        } catch (ignored: Exception) {
            true
        }
        val actions = EraseActions(secrets, gatt)
        val params = buildJsonObject {
            put("erase", true)
            put("canRemoveOsBond", false)
        }
        state.value = try {
            val done = MigrationFlow.run("clear", params, StandardMigrationHost(emptyList(), null, actions), null, keys, null, files, random = random)
            if (done["ok"]!!.jsonPrimitive.boolean) EraseStep.Done(actions.keyKept, notErased, cloudMayRemain) else EraseStep.Failed(done["reason"]!!.jsonPrimitive.content)
        } catch (ex: Exception) {
            EraseStep.Failed("step_failed")
        }
    }

    private companion object {
        /** The step before the wipe failed, so how many IDs were left is not known; reported as one, which is "not all erased". */
        const val UNKNOWN_NOT_ERASED = 1
    }
}
