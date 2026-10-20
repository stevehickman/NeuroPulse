package life.neurone.shared.migration

import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.data.storedSecret
import life.neurone.core.data.toStoredText
import life.neurone.core.migration.MigrationItemStore
import life.neurone.core.platform.epochSeconds
import life.neurone.shared.MigrationPlace

/**
 * The automatic health-data backup (SWR-MG-03: the nightly backup stays UHDR only). Once the person has made a health-data backup
 * and chosen to keep backing up, the same backup is made again whenever the app runs and the last one is over 20 hours old, into the
 * same folder, under the same code or passphrase, without asking.
 *
 * **The secret is kept on this phone, by the person's choice** (`np.backup.auto-secret`): in the sealed per-install store, under the
 * device-only data key, so it is readable only on this phone while it is unlocked, never goes into a bundle or to another phone, and
 * is destroyed by [turnOff] and by the clear. Restoring still needs the code or passphrase the person holds. A run only ever reads
 * the secret to seal the next bundle; no screen shows it.
 *
 * Only a place the host can write without a dialog is used (a folder). A host whose only place is a file the person must save
 * ([PortableFilePlace]) cannot run unattended and has no automatic backup. The schedule is "when the app runs", one rule for every
 * platform, with no operating-system background scheduler.
 */
class ScheduledBackup(
    /** The host's own folder place now, or null when it has none. Read at each run, because the person may change the folder. */
    private val place: () -> MigrationPlace?,
    private val keys: KeyValueStore,
    private val items: List<MigrationItemStore> = emptyList(),
    private val nowSeconds: () -> Long = { epochSeconds() },
    private val random: RandomSource = SystemRandomSource,
) {
    /** How a [runIfDue] ended. */
    sealed interface Outcome {
        data object Off : Outcome
        data object NoPlace : Outcome
        data object NotDue : Outcome
        data class Made(val atSeconds: Long) : Outcome
        /** The core's reason code; the screen words it from the core's table. */
        data class Failed(val reason: String) : Outcome
    }

    /** Whether the person has turned the automatic backup on. */
    val isOn: Boolean get() = keys.getString(SECRET_KEY) != null

    /** When the last backup was made, in seconds since 1970, or null if none has been. */
    val lastBackupSeconds: Long? get() = keys.getString(LAST_KEY)?.toLongOrNull()

    /** The core's reason code for the last automatic run if it failed, or null. */
    val lastFailure: String? get() = keys.getString(FAILURE_KEY)

    /** Keep backing up under [secret], which the caller made for this backup. The secret is copied; the caller still destroys its own. */
    fun turnOn(method: BundleKeyMethod, secret: BundleSecret) {
        keys.putString(SECRET_KEY, method.wire + ":" + secret.toStoredText())
        keys.remove(FAILURE_KEY)
    }

    /** Stop, and destroy the kept secret. */
    fun turnOff() {
        keys.remove(SECRET_KEY)
        keys.remove(FAILURE_KEY)
    }

    /** Record that a backup was made now (the manual one counts, so the first automatic run follows it by a day). */
    fun noteMade() {
        keys.putString(LAST_KEY, nowSeconds().toString())
    }

    /** Make the backup if it is on, there is a folder, and the last one is over [INTERVAL_SECONDS] old. */
    suspend fun runIfDue(): Outcome {
        val stored = keys.getString(SECRET_KEY) ?: return Outcome.Off
        val place = place() ?: return Outcome.NoPlace
        val now = nowSeconds()
        val last = lastBackupSeconds
        if (last != null && now - last in 0 until INTERVAL_SECONDS) return Outcome.NotDue

        val method = BundleKeyMethod.fromWire(stored.substringBefore(':'))
        val secret = BundleKeyDerivation.storedSecret(stored.substringAfter(':'))
        if (method == null || secret == null) return fail("secret_unreadable")
        val outcome = try {
            runBackup(place, keys, items, BackupKind.UHDR, method, secret, random)
        } finally {
            secret.destroy()
        }
        return when (outcome) {
            is BackupStep.Done -> {
                keys.putString(LAST_KEY, now.toString())
                keys.remove(FAILURE_KEY)
                Outcome.Made(now)
            }
            is BackupStep.Failed -> fail(outcome.reason)
            else -> fail("unknown")
        }
    }

    private fun fail(reason: String): Outcome {
        keys.putString(FAILURE_KEY, reason)
        return Outcome.Failed(reason)
    }

    companion object {
        const val SECRET_KEY = "np.backup.auto-secret"
        const val LAST_KEY = "np.backup.last-epoch"
        const val FAILURE_KEY = "np.backup.auto-failure"

        /** The Swift app's interval: a backup that ran within 20 hours is not repeated, so a day's several launches make one. */
        const val INTERVAL_SECONDS = 20L * 3600L
    }
}
