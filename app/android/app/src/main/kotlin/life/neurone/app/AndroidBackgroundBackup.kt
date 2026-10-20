package life.neurone.app

import android.content.Context
import androidx.work.Constraints
import androidx.work.CoroutineWorker
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import life.neurone.shared.migration.BackgroundBackupScheduler
import java.util.concurrent.TimeUnit

/**
 * The automatic backup's wake-up request to Android (WorkManager, a periodic job): the system runs it when it chooses, within the
 * device's battery and Doze rules, and runs nothing while the phone has not been unlocked since it started (the job's own storage is
 * not open until then). It waits for a battery that is not low and storage that is not low, which the system's own defaults treat
 * as the courtesy a background job owes. A job the system never runs is not an error: the backup is still made whenever the app runs.
 *
 * The request is a periodic job under one unique name. [request] keeps a job already queued or running, and changes only its
 * period, so asking again on every launch does not push the next run out, and the run itself can ask for the next without cancelling
 * itself. [cancel] withdraws it.
 */
class AndroidBackgroundBackupScheduler(private val context: Context) : BackgroundBackupScheduler {
    override fun request(afterSeconds: Long) {
        val work = PeriodicWorkRequestBuilder<BackupWorker>(afterSeconds, TimeUnit.SECONDS)
            .setConstraints(Constraints.Builder().setRequiresBatteryNotLow(true).setRequiresStorageNotLow(true).build())
            .build()
        WorkManager.getInstance(context).enqueueUniquePeriodicWork(WORK_NAME, ExistingPeriodicWorkPolicy.UPDATE, work)
    }

    override fun cancel() {
        WorkManager.getInstance(context).cancelUniqueWork(WORK_NAME)
    }

    private companion object {
        const val WORK_NAME = "neurone-automatic-backup"
    }
}

/**
 * What the system runs: the backup, if it is due, through the app's one [life.neurone.shared.AppServices]. The process was started
 * for this job, so [NeurOneApplication] has already built them. A run that could not open the saved data, or whose backup failed,
 * is recorded by the services themselves (`ScheduledBackup`, `AppServices.runBackgroundBackup`); the job succeeds either way, because
 * the next period is the retry.
 */
class BackupWorker(context: Context, params: WorkerParameters) : CoroutineWorker(context, params) {
    override suspend fun doWork(): Result {
        (applicationContext as NeurOneApplication).services.runBackgroundBackup()
        return Result.success()
    }
}
