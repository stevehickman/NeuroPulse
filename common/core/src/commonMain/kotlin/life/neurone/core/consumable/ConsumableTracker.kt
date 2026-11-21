package life.neurone.core.consumable

import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.PresenceState
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.protocol.PersistedKeys

/**
 * Source of consumable session counts. Port of iOS ConsumableCountsProviding.
 * The production implementation adapts the GATT manager's CONSUMABLE_STATUS
 * notifications; tests provide a synchronous fake.
 *
 * Contract: [observe] delivers the CURRENT value synchronously on
 * subscription (matching iOS CurrentValueSubject semantics), then every
 * subsequent distinct update.
 */
fun interface ConsumableCountsProviding {
    fun observe(listener: (List<Int>) -> Unit): AutoCloseable
}

/**
 * Measurement-triggered consumable reminder engine. Port of iOS
 * ConsumableTracker.swift. Per CLAUDE.md §5.2: all reminders are
 * measurement-triggered, not calendar-triggered. Reminder levels:
 * safety-blocking (cannot dismiss), performance-critical (snooze ≤3×),
 * comfort (≤5×).
 *
 * Every trigger here is a SESSION COUNT from the hub's SHDR-class CONSUMABLE_STATUS —
 * an *exposure count* in CLAUDE.md §2.3's sense, not a condition measurement of the
 * part. Two consequences, stated because the bare phrase "measurement-triggered" hides
 * both: a count cannot see a part that failed early, was damaged, or degraded off the
 * device; and a count whose limit was back-derived from a calendar is a calendar prompt
 * in disguise, which is why AUDIO_CUP_FOAM's limit is labelled an unvalidated
 * placeholder (OI-ACC-02 / OI-ACC-04, CLAUDE.md Rev 48). No trigger in this engine is
 * time-denominated, and none can be: the headset has no VBAT rail.
 *
 * Notification delivery is platform-side (:app) via [onReminderNotification];
 * lock-screen content must use a generic "NeurOne" title with consumable
 * detail only in the body (NP-PRIV-ANALYSIS-003 LOW-1).
 */
class ConsumableTracker(
    countsProvider: ConsumableCountsProviding,
    private val store: KeyValueStore,
    private val onReminderNotification: (ConsumableReminder) -> Unit = {},
    /**
     * Tells the hub a part was replaced (OI-ACC-08): the hub owns the count, and without this
     * its next CONSUMABLE_STATUS notification would restore the old one. The :app wires it to
     * NeurOneGattManager.requestConsumableReset.
     */
    private val onReplaced: (Int) -> Unit = {},
    /**
     * The presence lock when the host gates UHDR (`NP-APP-PRESENCE-001`), or null when it does not. The session counts come live from
     * the hub and are never stored; the snooze counts are stored and sealed. While locked the snooze counts read as 0, so a reminder
     * can repeat but never goes missing (`NP-APP-PRESENCE-001` §3), and nothing is written: a write of those zeros would replace the
     * sealed snooze counts at the unlock and reset the limits of `CLAUDE.md` §5.2 every launch (`REQ-PRES-01` (d), `OI-PRIV-19`).
     * Used from one thread.
     */
    private val lock: PresenceLock? = null,
) {
    companion object {
    }

    var states: List<ConsumableState> =
        ConsumableKind.entries.map { ConsumableState(kind = it, sessionCount = 0) }
        private set

    var activeReminders: List<ConsumableReminder> = emptyList()
        private set
    var blockingReminders: List<ConsumableReminder> = emptyList()
        private set

    private val subscription: AutoCloseable

    /** Whether the snooze counts in [states] are the stored ones. False while locked. */
    val snoozeIsReadable: Boolean get() = snoozeLoaded

    private var snoozeLoaded = false

    /** Snoozes and replacements made while locked, oldest first, replayed on the snooze counts read at the unlock. */
    private val held = mutableListOf<() -> Unit>()

    init {
        // Snooze state MUST load before count observation begins — the provider
        // delivers its current value synchronously on subscription, which
        // persists snooze; loading first ensures snooze counts survive restart.
        // (iOS init-order bug fix preserved — NP-PRIV-ANALYSIS-003 session.)
        if (lock == null || lock.isUnlocked) {
            loadPersistedSnooze()
            snoozeLoaded = true
        }
        lock?.onStateChanged { state -> if (state == PresenceState.LOCKED) forgetSnooze() else readSnooze() }
        subscription = countsProvider.observe { counts -> handleUpdatedCounts(counts) }
    }

    /** The lock closed: no snooze count stays decrypted in this tracker. The hub's session counts are live and stay. */
    private fun forgetSnooze() {
        states = states.map { it.copy(snoozeCount = 0) }
        snoozeLoaded = false
        recomputeReminders()
    }

    /** The lock opened: read the sealed snooze counts, run what was held on them, and show the reminders they give. */
    private fun readSnooze() {
        loadPersistedSnooze()
        snoozeLoaded = true
        val operations = held.toList()
        held.clear()
        operations.forEach { it() }
        persistSnooze()
        recomputeReminders()
    }

    private var lastCounts: List<Int>? = null

    private fun handleUpdatedCounts(counts: List<Int>) {
        if (counts == lastCounts) return // removeDuplicates parity
        lastCounts = counts
        states = states.map { s ->
            if (s.kind.rawValue < counts.size) s.copy(sessionCount = counts[s.kind.rawValue]) else s
        }
        persistSnooze()
        recomputeReminders()
    }

    // ── Reminder computation ─────────────────────────────────────────────

    private fun recomputeReminders() {
        val all = mutableListOf<ConsumableReminder>()
        val blocking = mutableListOf<ConsumableReminder>()

        for (state in states.filter { it.isLow }) {
            val reminder = ConsumableReminder(state)
            all += reminder
            if (reminder.isBlocking) blocking += reminder
            if (!state.canSnooze || state.snoozeCount == 0) {
                onReminderNotification(reminder)
            }
        }

        activeReminders = all
        blockingReminders = blocking
    }

    // ── User actions ─────────────────────────────────────────────────────

    fun snooze(consumableIndex: Int) {
        if (!snoozeLoaded) {
            held += { snooze(consumableIndex) }
            return
        }
        val state = states.getOrNull(consumableIndex) ?: return
        if (!state.canSnooze) return
        states = states.mapIndexed { i, s ->
            if (i == consumableIndex) s.copy(snoozeCount = s.snoozeCount + 1) else s
        }
        persistSnooze()
        recomputeReminders()
    }

    fun markReplaced(consumableIndex: Int) {
        if (consumableIndex >= states.size) return
        if (!snoozeLoaded) {
            // The hub owns the count and is told now. The stored snooze count is zeroed at the unlock, on what is read then.
            held += { resetSnooze(consumableIndex) }
            states = states.mapIndexed { i, s -> if (i == consumableIndex) s.copy(sessionCount = 0, snoozeCount = 0) else s }
            recomputeReminders()
            onReplaced(consumableIndex)
            return
        }
        states = states.mapIndexed { i, s ->
            if (i == consumableIndex) s.copy(sessionCount = 0, snoozeCount = 0) else s
        }
        persistSnooze()
        recomputeReminders()
        onReplaced(consumableIndex)
    }

    // ── Session start gate ───────────────────────────────────────────────

    val sessionIsBlocked: Boolean get() = blockingReminders.isNotEmpty()

    // ── Persistence (snooze counts survive app restart) ─────────────────

    private fun resetSnooze(consumableIndex: Int) {
        states = states.mapIndexed { i, s -> if (i == consumableIndex) s.copy(snoozeCount = 0) else s }
    }

    private fun persistSnooze() {
        if (!snoozeLoaded || (lock != null && !lock.isUnlocked)) return
        try {
            store.putString(PersistedKeys.SNOOZE_KEY, states.joinToString(",") { it.snoozeCount.toString() })
        } catch (e: SecretKeyUnavailableException) {
            if (lock == null) throw e
        }
    }

    private fun loadPersistedSnooze() {
        val counts = store.getString(PersistedKeys.SNOOZE_KEY)
            ?.split(",")
            ?.mapNotNull { it.toIntOrNull() }
            ?: return
        states = states.mapIndexed { i, s ->
            if (i < counts.size) s.copy(snoozeCount = counts[i]) else s
        }
    }

    fun close() = subscription.close()
}
