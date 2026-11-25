package life.neurone.shared.migration

import kotlinx.coroutines.CancellationException
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.KeptKeySealer
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.MigrationItemStore
import life.neurone.core.platform.epochSeconds
import life.neurone.shared.MigrationPlace

/**
 * The automatic health-data backup (SWR-MG-03: the nightly backup stays UHDR only). Once the person has made a health-data backup
 * and chosen to keep backing up, the same backup is made again whenever the app runs and the last one is over 20 hours old, into the
 * same folder, under the same code or passphrase, without asking.
 *
 * **A key derived from the secret is kept on this phone, by the person's choice, and the secret is not** (`np.backup.auto-secret`;
 * `OI-PRIV-65` (2), the principal, 2026-11-24). When the backup is turned on, the core derives the bundle key from the code or
 * passphrase under one fresh salt and keeps it; [sealer] has the core seal it under the data key of the store that holds it, and the
 * record kept is `{v, method, kdf, key}` with the sealed key. Every automatic bundle and its recordings are sealed under that salt, so
 * each still restores with the code or passphrase alone. A passphrase a person may reuse elsewhere is therefore never stored. The record
 * never goes into a bundle or to another phone and is destroyed by [turnOff] and by the clear. A run where nothing can be sealed (no
 * [sealer]: the storage is not sealed on this run) keeps nothing, and the backup stays off.
 *
 * **Only a run whose storage is sealed keeps it** (`SWR-MG-13` (e), `OI-PRIV-61`). On any other run ([secretKeptSealed] false: the host
 * has no usable keystore and its store is plain, or the sealed data cannot be opened) [turnOn] keeps nothing and says so, and a secret an
 * earlier run left in the clear is removed when this object is made: otherwise the key would sit beside the backups it opens, and anyone
 * who copied the folder would hold both. Removing it is an erase, not a weakening: the backup made by hand still runs under what the
 * person types, and a value sealed by an earlier run is left alone for a run that can open it.
 *
 * Only a place the host can write without a dialog is used (a folder). A host whose only place is a file the person must save
 * ([PortableFilePlace]) cannot run unattended and has no automatic backup. The schedule is "when the app runs", one rule for every
 * platform; a host that has an operating-system scheduler also asks it to wake the app (`BackgroundBackupScheduler`), and the app
 * says when the schedule changed through [onChange].
 */
class ScheduledBackup(
    /** The host's own folder place now, or null when it has none. Read at each run, because the person may change the folder. */
    private val place: () -> MigrationPlace?,
    private val keys: KeyValueStore,
    private val items: List<MigrationItemStore> = emptyList(),
    private val nowSeconds: () -> Long = { epochSeconds() },
    private val random: RandomSource = SystemRandomSource,
    /** The recordings sealed beside each backup, or null where the host keeps none. */
    private val waveforms: WaveformBackup? = null,
    /** Called after the backup is turned on or off, so the host's scheduler can follow. */
    private val onChange: () -> Unit = {},
    /**
     * Whether a value written under [SECRET_KEY] through [keys] is sealed at rest on this run. False unless the caller knows it is (the app
     * passes `StorageProtection.SEALED`), so a store nobody vouched for never receives the secret.
     */
    private val secretKeptSealed: () -> Boolean = { false },
    /** Seals the kept key under the data key of [keys]' sealed tier; null on a run whose storage is not sealed, where nothing is kept. */
    private val sealer: KeptKeySealer? = null,
) {
    init {
        if (!secretKeptSealed()) removePlainSecret()
    }

    /** The key the automatic backup will keep, derived and sealed but not yet stored ([prepare], then [turnOn]). Holds no secret and no key. */
    class Kept internal constructor(internal val method: BundleKeyMethod, internal val kdf: JsonObject, internal val sealed: String)

    /** How a [runIfDue] ended. */
    sealed interface Outcome {
        data object Off : Outcome
        data object NoPlace : Outcome
        data object NotDue : Outcome
        data class Made(val atSeconds: Long) : Outcome
        /** The core's reason code; the screen words it from the core's table. */
        data class Failed(val reason: String) : Outcome
    }

    /**
     * Whether the automatic backup can be turned on on this run: only where what it keeps would be sealed at rest (`OI-PRIV-61`) and there
     * is a key to seal its kept key under (`OI-PRIV-65` (2)).
     */
    val isAvailable: Boolean get() = secretKeptSealed() && sealer != null

    /** Whether the person has turned the automatic backup on. Never on a run that cannot keep its secret sealed. */
    val isOn: Boolean get() = isAvailable && keys.getString(SECRET_KEY) != null

    /** When the last backup was made, in seconds since 1970, or null if none has been. */
    val lastBackupSeconds: Long? get() = keys.getString(LAST_KEY)?.toLongOrNull()

    /** The core's reason code for the last automatic run if it failed, or null. */
    val lastFailure: String? get() = keys.getString(FAILURE_KEY)

    /**
     * The key to keep for [secret]: derived by the core under a fresh salt, sealed under the data key and released. Null where this run
     * cannot seal (no [sealer], or its keystore cannot be asked). Nothing is stored until [turnOn]. The caller still destroys [secret].
     */
    fun prepare(method: BundleKeyMethod, secret: BundleSecret): Kept? {
        if (!isAvailable) return null
        val s = sealer ?: return null
        val kdf = BundleKeyDerivation.newKdf(method, random)
        val key = runCatching { BundleKeyDerivation.deriveKey(secret, kdf) }.getOrNull() ?: return null
        val sealed = try { s.sealKept(SECRET_KEY, key) } finally { key.destroy() } ?: return null
        return Kept(method, kdf, sealed)
    }

    /** [kept] as the key a backup is made under: unsealed into a new handle for each use. */
    fun backupKey(kept: Kept): BackupKey? {
        val s = sealer ?: return null
        return KeptBackupKey(kept.method, kept.kdf) { s.openKept(SECRET_KEY, kept.sealed) }
    }

    /** Keep backing up under [kept]. */
    fun turnOn(kept: Kept) {
        keys.putString(SECRET_KEY, buildJsonObject {
            put("v", RECORD_VERSION); put("method", kept.method.wire); put("kdf", kept.kdf); put("key", kept.sealed)
        }.toString())
        keys.remove(FAILURE_KEY)
        onChange()
        return true
    }

    /** Keep backing up under a key derived from [secret] ([prepare], then [turnOn]). False, and nothing kept, where this run cannot seal one. The caller still destroys [secret]. */
    fun turnOn(method: BundleKeyMethod, secret: BundleSecret): Boolean {
        turnOn(prepare(method, secret) ?: return false)
        return true
    }

    /** The record [stored], or null when it is not one: an unreadable value, or the earlier form that held the secret itself. */
    private fun readKept(stored: String): Kept? = runCatching {
        val o = Json.parseToJsonElement(stored).jsonObject
        if (o["v"]!!.jsonPrimitive.int != RECORD_VERSION) return null
        Kept(BundleKeyMethod.fromWire(o["method"]!!.jsonPrimitive.content)!!, o["kdf"]!!.jsonObject, o["key"]!!.jsonPrimitive.content)
    }.getOrNull()

    /** Stop, and destroy the kept secret. */
    fun turnOff() {
        keys.remove(SECRET_KEY)
        keys.remove(FAILURE_KEY)
        onChange()
    }

    /** Record that a backup was made now (the manual one counts, so the first automatic run follows it by a day). */
    fun noteMade() {
        keys.putString(LAST_KEY, nowSeconds().toString())
    }

    /** Make the backup if it is on, there is a folder, and the last one is over [INTERVAL_SECONDS] old. */
    suspend fun runIfDue(): Outcome {
        if (!isAvailable) return Outcome.Off
        val stored = keys.getString(SECRET_KEY) ?: return Outcome.Off
        val place = place() ?: return Outcome.NoPlace
        val now = nowSeconds()
        val last = lastBackupSeconds
        if (last != null && now - last in 0 until INTERVAL_SECONDS) return Outcome.NotDue

        val kept = readKept(stored)
        if (kept == null) {
            // Not a kept key. The earlier form held the code or passphrase itself (`method:hex`): it is destroyed, not read, and the
            // person turns the backup on again with their next backup (`OI-PRIV-65` (2); nothing of that form was released).
            keys.remove(SECRET_KEY)
            onChange()
            return fail("secret_unreadable")
        }
        val key = backupKey(kept) ?: return fail("secret_unreadable")
        // The key is unsealed once here so a key that no longer opens (the data key was destroyed or replaced) is said as such.
        (key.keyFor(kept.kdf) ?: return fail("secret_unreadable")).destroy()
        val outcome = try {
            runBackup(place, keys, items, BackupKind.UHDR, key, random, waveforms)
        } catch (ex: CancellationException) {
            throw ex
        } catch (ex: IllegalStateException) {
            return fail("secret_unreadable")
        }
        return when (outcome) {
            is BackupStep.Done -> {
                keys.putString(LAST_KEY, now.toString())
                // The settings backup was made either way; recordings that could not be sealed are said, and tried again at the next run.
                if (outcome.waveforms is WaveformBackupResult.Failed) keys.putString(FAILURE_KEY, WAVEFORMS_FAILED) else keys.remove(FAILURE_KEY)
                Outcome.Made(now)
            }
            is BackupStep.Failed -> fail(outcome.reason)
            else -> fail("unknown")
        }
    }

    /** A secret in the clear, left by a run before this rule; one that carries the sealing prefix was sealed and is not this run's to touch. */
    private fun removePlainSecret() {
        val stored = try { keys.getString(SECRET_KEY) } catch (ignored: ClassCastException) { null } ?: return
        if (!stored.startsWith(EncryptedKeyValueStore.PREFIX)) keys.remove(SECRET_KEY)
    }

    private fun fail(reason: String): Outcome {
        keys.putString(FAILURE_KEY, reason)
        return Outcome.Failed(reason)
    }

    companion object {
        const val SECRET_KEY = "np.backup.auto-secret"

        /** The form of the record under [SECRET_KEY]: a kept key, sealed (`OI-PRIV-65` (2)). Before it the value was `method:hex` of the secret. */
        private const val RECORD_VERSION = 2
        const val LAST_KEY = "np.backup.last-epoch"
        const val FAILURE_KEY = "np.backup.auto-failure"

        /** The failure code when the backup was made but its recordings were not; not one of the core's reasons, so the screen words it itself. */
        const val WAVEFORMS_FAILED = "waveforms_failed"

        /** The Swift app's interval: a backup that ran within 20 hours is not repeated, so a day's several launches make one. */
        const val INTERVAL_SECONDS = 20L * 3600L
    }
}
