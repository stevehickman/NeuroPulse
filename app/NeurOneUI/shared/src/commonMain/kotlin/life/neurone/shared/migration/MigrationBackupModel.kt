package life.neurone.shared.migration

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.BundleSecretResult
import life.neurone.core.data.NewRecoveryCode
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.MigrationFlow
import life.neurone.core.migration.MigrationItemStore
import life.neurone.core.migration.StandardMigrationHost
import life.neurone.core.migration.toJson
import life.neurone.shared.MigrationPlace

// The backup screen's flow, with no Compose in it (SWR-MG-02, SWR-MG-03, issue #569 package 6o). The screen draws a step and
// calls one of the functions here; every rule (what a kind carries, what a secret must be, whether a route may carry it, what
// ends a backup) is the shared core's, asked through MigrationFlow, so this holds the ORDER of the screens and nothing the
// core already decides. It is common to every platform because the screen is.

/** The two kinds of backup the person chooses between (SWR-MG-03). The nightly backup is the first and is not a choice here. */
enum class BackupKind(val wire: String) { UHDR("uhdr"), MIGRATE("migrate") }

/**
 * What a kind does, as item codes: the screen words each with [messageKey] (`items`). [carried] goes in the backup,
 * [omitted] is state that moves in a migration but not in this kind, [neverMoved] belongs to this install, and
 * [notYetSorted] is neither moved nor dropped yet (SWR-MG-01).
 */
class BackupPreview(
    val kind: BackupKind,
    val carried: List<String>,
    val omitted: List<String>,
    val neverMoved: List<String>,
    val notYetSorted: List<String>,
)

/**
 * Why a secret was refused: the core's code, the [group] of its table that words it (a typing mistake is `SECRET`; a secret that
 * did not open the backup is a `REASONS` code) and the minimum length if it has one.
 */
class SecretProblem(val reason: String, val minChars: Int?, val group: MessageGroup = MessageGroup.SECRET)

/** The step the screen draws. */
sealed interface BackupStep {
    /** This host has nowhere to put a backup yet. */
    data object NoPlace : BackupStep

    data object ChooseKind : BackupStep

    /** What the chosen kind carries and leaves out, before anything is made. */
    class Preview(val preview: BackupPreview) : BackupStep

    class ChooseMethod(val preview: BackupPreview) : BackupStep

    /** A new recovery code, shown once; the backup is made when the person says it is saved. */
    class ShowCode(val preview: BackupPreview, val code: String) : BackupStep

    class EnterPassphrase(val preview: BackupPreview, val problem: SecretProblem?) : BackupStep

    data object Running : BackupStep

    class Done(val bundleHash: String, val written: List<String>, val omitted: List<String>) : BackupStep

    /** The core's reason code; [messageKey] words it. */
    class Failed(val reason: String) : BackupStep
}

/** The groups of the core's table of locale keys (`messageKeys`). */
enum class MessageGroup(val wire: String) { REASONS("reasons"), SECRET("secret"), PROBE("probe"), ITEMS("items") }

/** The mark in front of a listed item. */
const val LIST_MARK = "\u2022"

class MigrationBackupModel(
    private val place: MigrationPlace?,
    private val keys: KeyValueStore,
    private val items: List<MigrationItemStore> = emptyList(),
    private val random: RandomSource = SystemRandomSource,
) {
    private val state = MutableStateFlow<BackupStep>(if (place == null) BackupStep.NoPlace else BackupStep.ChooseKind)
    val step: StateFlow<BackupStep> = state.asStateFlow()

    private var pendingCode: NewRecoveryCode? = null

    /** Whether the place can carry a migration. One that cannot promise an exclusive create is left to the health-data backup (SWR-MG-10). */
    val canMigrate: Boolean = place?.route?.atomicClaim == true

    fun chooseKind(kind: BackupKind) {
        if (place == null) return
        state.value = BackupStep.Preview(preview(kind))
    }

    fun continueToMethod() {
        val s = state.value
        if (s is BackupStep.Preview) state.value = BackupStep.ChooseMethod(s.preview)
    }

    /** Make a recovery code and show it. The secret it stands for is kept only until the backup is made or the person goes back. */
    fun useRecoveryCode() {
        val s = state.value as? BackupStep.ChooseMethod ?: return
        discardCode()
        val code = BundleKeyDerivation.newRecoveryCode(random)
        pendingCode = code
        state.value = BackupStep.ShowCode(s.preview, code.text)
    }

    fun usePassphrase() {
        val s = state.value as? BackupStep.ChooseMethod ?: return
        state.value = BackupStep.EnterPassphrase(s.preview, null)
    }

    /** The person says the recovery code is saved: make the backup under it. */
    suspend fun confirmCodeSaved() {
        val s = state.value as? BackupStep.ShowCode ?: return
        val code = pendingCode ?: return
        pendingCode = null
        run(s.preview, BundleKeyMethod.RECOVERY_CODE, code.secret)
    }

    /** A passphrase the person typed: refused with the core's reason, or the backup is made under it. */
    suspend fun submitPassphrase(text: String) {
        val s = state.value as? BackupStep.EnterPassphrase ?: return
        when (val r = BundleKeyDerivation.passphraseSecret(text)) {
            is BundleSecretResult.Rejected -> state.value = BackupStep.EnterPassphrase(s.preview, SecretProblem(r.reason, r.minChars))
            is BundleSecretResult.Ok -> run(s.preview, BundleKeyMethod.PASSPHRASE, r.secret)
        }
    }

    /** One step back. A recovery code not yet used is thrown away: the next one is a different code. */
    fun back() {
        when (val s = state.value) {
            is BackupStep.Preview -> state.value = BackupStep.ChooseKind
            is BackupStep.ChooseMethod -> state.value = BackupStep.Preview(s.preview)
            is BackupStep.ShowCode -> { discardCode(); state.value = BackupStep.ChooseMethod(s.preview) }
            is BackupStep.EnterPassphrase -> state.value = BackupStep.ChooseMethod(s.preview)
            is BackupStep.Failed, is BackupStep.Done -> state.value = if (place == null) BackupStep.NoPlace else BackupStep.ChooseKind
            else -> {}
        }
    }

    private fun discardCode() {
        pendingCode?.secret?.destroy()
        pendingCode = null
    }

    private fun preview(kind: BackupKind): BackupPreview {
        val inv = MigrationFlow.inventory(kind.wire)
        fun list(key: String) = inv[key]!!.jsonArray.map { it.jsonPrimitive.content }
        return BackupPreview(kind, list("carried"), list("omitted"), list("perInstall"), list("unsorted"))
    }

    private suspend fun run(preview: BackupPreview, method: BundleKeyMethod, secret: BundleSecret) {
        val place = place ?: return
        state.value = BackupStep.Running
        val host = StandardMigrationHost(items)
        val params = buildJsonObject {
            put("route", place.route.toJson())
            put("hostItems", JsonArray(host.hostItems.map { JsonPrimitive(it) }))
            put("kind", preview.kind.wire)
            put("method", method.wire)
        }
        val done = try {
            MigrationFlow.run("backup", params, host, secret, keys, place.store, random = random)
        } finally {
            secret.destroy()
        }
        state.value = if (done["ok"]!!.jsonPrimitive.boolean) {
            BackupStep.Done(
                done["bundleHash"]!!.jsonPrimitive.content,
                done["written"]!!.jsonArray.map { it.jsonPrimitive.content },
                done["omitted"]!!.jsonArray.map { it.jsonPrimitive.content },
            )
        } else {
            BackupStep.Failed(done["reason"]!!.jsonPrimitive.content)
        }
    }

    companion object {
        /** The locale key that words a core code in [group] ([MessageGroup]), from the core's table. */
        fun messageKey(group: MessageGroup, code: String): String? =
            MigrationFlow.messageKeys()[group.wire]?.jsonObject?.get(code)?.jsonPrimitive?.content
    }
}
