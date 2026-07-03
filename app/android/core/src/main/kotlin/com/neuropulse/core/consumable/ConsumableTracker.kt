package com.neuropulse.core.consumable

import com.neuropulse.core.common.KeyValueStore

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
 * Notification delivery is platform-side (:app) via [onReminderNotification];
 * lock-screen content must use a generic "NeuroPulse" title with consumable
 * detail only in the body (NP-PRIV-ANALYSIS-003 LOW-1).
 */
class ConsumableTracker(
    countsProvider: ConsumableCountsProviding,
    private val store: KeyValueStore,
    private val onReminderNotification: (ConsumableReminder) -> Unit = {},
) {
    companion object {
        const val SNOOZE_KEY = "np.consumable.snooze-counts"
    }

    var states: List<ConsumableState> =
        ConsumableKind.entries.map { ConsumableState(kind = it, sessionCount = 0) }
        private set

    var activeReminders: List<ConsumableReminder> = emptyList()
        private set
    var blockingReminders: List<ConsumableReminder> = emptyList()
        private set

    private val subscription: AutoCloseable

    init {
        // Snooze state MUST load before count observation begins — the provider
        // delivers its current value synchronously on subscription, which
        // persists snooze; loading first ensures snooze counts survive restart.
        // (iOS init-order bug fix preserved — NP-PRIV-ANALYSIS-003 session.)
        loadPersistedSnooze()
        subscription = countsProvider.observe { counts -> handleUpdatedCounts(counts) }
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
        states = states.mapIndexed { i, s ->
            if (i == consumableIndex) s.copy(sessionCount = 0, snoozeCount = 0) else s
        }
        persistSnooze()
        recomputeReminders()
    }

    // ── Session start gate ───────────────────────────────────────────────

    val sessionIsBlocked: Boolean get() = blockingReminders.isNotEmpty()

    // ── Persistence (snooze counts survive app restart) ─────────────────

    private fun persistSnooze() {
        store.putString(SNOOZE_KEY, states.joinToString(",") { it.snoozeCount.toString() })
    }

    private fun loadPersistedSnooze() {
        val counts = store.getString(SNOOZE_KEY)
            ?.split(",")
            ?.mapNotNull { it.toIntOrNull() }
            ?: return
        states = states.mapIndexed { i, s ->
            if (i < counts.size) s.copy(snoozeCount = counts[i]) else s
        }
    }

    fun close() = subscription.close()
}
