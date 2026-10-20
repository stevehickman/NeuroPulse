package life.neurone.shared.migration

/**
 * The operating system's own scheduler, for a host that has one the app can ask to wake it (iOS `BGTaskScheduler`). The app asks
 * for a wake-up and the system decides when, if at all; a request is a wish, never a promise, which is why the app also checks
 * whenever it runs (`ScheduledBackup`). When the system wakes the app, the host calls `AppServices.runBackgroundBackup`.
 */
interface BackgroundBackupScheduler {
    /** Ask to be woken no sooner than [afterSeconds] from now. Replaces any earlier request. */
    fun request(afterSeconds: Long)

    /** Withdraw the request. */
    fun cancel()
}
