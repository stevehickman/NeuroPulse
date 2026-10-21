package life.neurone.shared

/**
 * The host's local notifications, for the consumable reminders (CLAUDE.md §2.3, NP-PRIV-ANALYSIS-003). The reminder engine decides
 * when one is due and the shared app words it; the host only shows it.
 *
 * **What the lock screen shows is the host's to keep generic:** the title and subtitle carry no consumable and no device type, and
 * the detail is only in the body (NP-PRIV-ANALYSIS-003 LOW-1). The shared app passes exactly that split, so a host cannot put the
 * detail in the title by accident.
 */
interface ReminderNotifier {
    /**
     * Ask the person for permission to notify, if the system has not been asked. Called when the Consumables screen first appears
     * and never at launch, so the dialog comes with the reason for it (NP-PRIV-ANALYSIS-003 LOW-2).
     */
    fun requestPermission()

    /** Show one notification now. A second with the same [id] replaces the first, so a reminder is never shown twice. */
    fun show(id: String, title: String, subtitle: String, body: String)
}

/**
 * The host's read-only view of the person's heart rate and heart rate variability from the phone's health store, for one session at
 * a time (iOS HealthKit). The values are for the live display only: held in memory, never written to the app's storage, never sent
 * anywhere, and cleared when the session ends (NP-APP-ROADMAP-001 Rev B §9.1; iOS ISC-94 to ISC-96). The host never writes to the
 * health store.
 */
interface HealthSignals {
    /** Latest HRV (SDNN) in milliseconds, or null when there is none. */
    val hrvSdnnMilliseconds: kotlinx.coroutines.flow.StateFlow<Double?>

    /** Latest heart rate in beats per minute, or null when there is none. */
    val heartRateBpm: kotlinx.coroutines.flow.StateFlow<Double?>

    /**
     * Ask for read permission and begin. Called only while an HRV biofeedback session is running, never at launch or at consent, so
     * the system's dialog comes in the context that explains it (ISC-94).
     */
    suspend fun start()

    /** Stop reading and clear both values at once. */
    fun stopAndClear()
}
