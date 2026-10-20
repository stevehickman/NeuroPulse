package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.ObjCObjectVar
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import life.neurone.shared.migration.BackgroundBackupScheduler
import platform.BackgroundTasks.BGProcessingTask
import platform.BackgroundTasks.BGProcessingTaskRequest
import platform.BackgroundTasks.BGTaskScheduler
import platform.Foundation.NSDate
import platform.Foundation.NSError
import platform.Foundation.dateWithTimeIntervalSinceNow

/** The task identifier the host declares in `BGTaskSchedulerPermittedIdentifiers` (`iosApp/project.yml`). */
const val BACKUP_TASK_ID = "life.neurone.ui.backup"

/**
 * The automatic backup's wake-up request to iOS (`BGProcessingTask`: the system runs it when it chooses, usually while the phone
 * charges and is idle, and runs nothing if the person has switched Background App Refresh off). It needs no network and no power.
 * A request the system refuses (the simulator has no scheduler) is not an error: the backup is still made whenever the app runs.
 */
@OptIn(ExperimentalForeignApi::class)
class IosBackgroundBackupScheduler : BackgroundBackupScheduler {
    override fun request(afterSeconds: Long) {
        val request = BGProcessingTaskRequest(identifier = BACKUP_TASK_ID)
        request.earliestBeginDate = NSDate.dateWithTimeIntervalSinceNow(afterSeconds.toDouble())
        request.requiresNetworkConnectivity = false
        request.requiresExternalPower = false
        memScoped {
            val error = alloc<ObjCObjectVar<NSError?>>()
            BGTaskScheduler.sharedScheduler.submitTaskRequest(request, error.ptr)
        }
    }

    override fun cancel() {
        BGTaskScheduler.sharedScheduler.cancelTaskRequestWithIdentifier(BACKUP_TASK_ID)
    }
}

/**
 * The one [AppServices] of this process. The system may launch the app only to run the backup, with no window, so the services cannot
 * belong to the view controller: the view controller and the background task both take them from here, and the app is built once.
 */
object IosApp {
    private val scope: CoroutineScope = createAppScope()
    val services: AppServices by lazy { AppServices(IosPlatformServices(), scope) }

    internal fun launch(block: suspend () -> Unit) = scope.launch { block() }
}

/**
 * Called by the host before the app finishes launching (iOS refuses a handler registered later). When the system wakes the app, the
 * backup is made if due, the next wake-up is requested, and the task is completed; if the system runs out of time the work is
 * cancelled and the task completed as unsuccessful.
 */
@OptIn(ExperimentalForeignApi::class)
fun registerBackgroundBackup() {
    BGTaskScheduler.sharedScheduler.registerForTaskWithIdentifier(BACKUP_TASK_ID, null) { task ->
        val processing = task as? BGProcessingTask ?: return@registerForTaskWithIdentifier
        val job = IosApp.launch {
            IosApp.services.runBackgroundBackup()
            processing.setTaskCompletedWithSuccess(true)
        }
        processing.expirationHandler = {
            job.cancel()
            processing.setTaskCompletedWithSuccess(false)
        }
    }
}
