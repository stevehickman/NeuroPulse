package life.neurone.shared.migration

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.BundleSecretResult
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.LinkReceiveSession
import life.neurone.core.migration.LinkTransport
import life.neurone.core.migration.MigrationItemStore

// The new phone's side of a direct move, with no Compose in it (SWR-MG-10; issue #569 package 6u): the code typed, the other
// phone found, the secret asked for once the bundle's header has been read from the other phone, and what was restored. The
// pairing, the proof, the sealed link and the restore are the core's (`LinkReceiveSession`); this holds the order of the
// screens and nothing the core decides.

sealed interface LinkReceiveStep {
    /** Asking for the code shown on the other phone. [reason] is the core's code for why the last try failed, if it did. */
    class EnterCode(val reason: String? = null) : LinkReceiveStep

    /** Finding the other phone, pairing, or restoring. */
    data object Working : LinkReceiveStep

    /** The other phone's bundle is found and paired: ask for what it is locked with. [problem] is why the last try failed. */
    class EnterSecret(val method: BundleKeyMethod, val problem: SecretProblem?) : LinkReceiveStep

    class Done(val report: RestoreReport) : LinkReceiveStep

    /** The core's reason code; the screen words it with [MigrationBackupModel.messageKey]. */
    class Failed(val reason: String) : LinkReceiveStep
}

class LinkReceiveModel(
    private val transport: LinkTransport?,
    private val keys: KeyValueStore,
    private val items: List<MigrationItemStore> = emptyList(),
    private val random: RandomSource = SystemRandomSource,
) {
    private val state = MutableStateFlow<LinkReceiveStep>(LinkReceiveStep.EnterCode())
    val step: StateFlow<LinkReceiveStep> = state.asStateFlow()

    /** The secret the person typed, handed to the restore that is waiting for it; null if they gave up. */
    private var typed: CompletableDeferred<String?>? = null

    /** Whether this host can reach another phone at all. */
    val available: Boolean get() = transport != null

    /** The code the person typed. Runs the whole move; returns once it has ended. */
    suspend fun submitCode(text: String) {
        val transport = transport ?: return
        state.value = LinkReceiveStep.Working
        val done = LinkReceiveSession(transport, keys, items, random).run(text) { kdf -> askSecret(kdf) }
        state.value = if (done["ok"]!!.jsonPrimitive.boolean) {
            LinkReceiveStep.Done(restoreReportOf(done))
        } else {
            LinkReceiveStep.Failed(done["reason"]!!.jsonPrimitive.content)
        }
    }

    /** The recovery code or passphrase, once the screen is asking for it. */
    fun submitSecret(text: String) {
        typed?.complete(text)
    }

    /** The person gave up asking for the secret. */
    fun cancel() {
        typed?.complete(null)
    }

    /** Back to the code, after a failure. */
    fun again() {
        state.value = LinkReceiveStep.EnterCode()
    }

    private suspend fun askSecret(kdf: JsonObject): BundleSecret? {
        val method = BundleKeyMethod.fromWire(kdf["method"]!!.jsonPrimitive.content) ?: return null
        var problem: SecretProblem? = null
        while (true) {
            val answer = CompletableDeferred<String?>().also { typed = it }
            state.value = LinkReceiveStep.EnterSecret(method, problem)
            val text = answer.await() ?: return null
            val parsed = when (method) {
                BundleKeyMethod.RECOVERY_CODE -> BundleKeyDerivation.recoveryCodeSecret(text)
                BundleKeyMethod.PASSPHRASE -> BundleKeyDerivation.passphraseSecret(text)
            }
            when (parsed) {
                is BundleSecretResult.Rejected -> problem = SecretProblem(parsed.reason, parsed.minChars)
                is BundleSecretResult.Ok -> {
                    state.value = LinkReceiveStep.Working
                    return parsed.secret
                }
            }
        }
    }
}
