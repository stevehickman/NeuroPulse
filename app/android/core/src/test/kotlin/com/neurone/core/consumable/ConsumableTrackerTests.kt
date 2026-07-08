package com.neurone.core.consumable

import com.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

// Synchronous fake provider — delivers current value on subscribe, matching
// the iOS CurrentValueSubject contract (this is exactly what exposed the
// init-order bug in the iOS implementation).
private class FakeCountsProvider(initial: List<Int>) : ConsumableCountsProviding {
    private var current = initial
    private var listener: ((List<Int>) -> Unit)? = null

    override fun observe(listener: (List<Int>) -> Unit): AutoCloseable {
        this.listener = listener
        listener(current) // synchronous delivery of current value
        return AutoCloseable { this.listener = null }
    }

    fun emit(counts: List<Int>) {
        current = counts
        listener?.invoke(counts)
    }
}

class ConsumableTrackerTests {

    // ISC-35: DI provider; isLow semantics incl. intranasal single-use threshold 0.
    @Test
    fun intranasalIsLowOnlyAtOrPastLimit() {
        val provider = FakeCountsProvider(listOf(0, 0, 0, 0))
        val tracker = ConsumableTracker(provider, InMemoryKeyValueStore())

        // Fresh sleeve (0 sessions): NOT low — lowThreshold is 0, remaining is 1.
        assertFalse(tracker.states[0].isLow)

        provider.emit(listOf(1, 0, 0, 0)) // used once — at limit
        assertTrue(tracker.states[0].isLow)
        assertTrue(tracker.states[0].isExceeded)
        assertTrue(tracker.sessionIsBlocked) // safety-blocking gate
    }

    @Test
    fun hydrogelLowAtEightRemaining() {
        val provider = FakeCountsProvider(listOf(0, 37, 0, 0)) // 45-37 = 8 remaining
        val tracker = ConsumableTracker(provider, InMemoryKeyValueStore())
        assertTrue(tracker.states[1].isLow)
        assertFalse(tracker.states[1].isExceeded)
        assertFalse(tracker.sessionIsBlocked) // performance-critical, not blocking
    }

    @Test
    fun activeRemindersFiltersOnlyLowStates() {
        val provider = FakeCountsProvider(listOf(0, 37, 5, 10))
        val tracker = ConsumableTracker(provider, InMemoryKeyValueStore())
        // Only hydrogel (8 remaining ≤ 8) is low; VNS has 25 remaining, foam 140.
        assertEquals(
            listOf(ConsumableKind.ELECTRODE_HYDROGEL),
            tracker.activeReminders.map { it.state.kind },
        )
    }

    // ISC-36: snooze loads BEFORE observation — the synchronous first delivery
    // must not wipe persisted snooze state (iOS init-order bug).
    @Test
    fun persistedSnoozeSurvivesTrackerRestart() {
        val kv = InMemoryKeyValueStore()
        val provider = FakeCountsProvider(listOf(0, 40, 0, 0))
        val tracker1 = ConsumableTracker(provider, kv)
        tracker1.snooze(1)
        tracker1.snooze(1)
        assertEquals(2, tracker1.states[1].snoozeCount)

        // Restart: new tracker, same store — provider delivers synchronously in init.
        val tracker2 = ConsumableTracker(FakeCountsProvider(listOf(0, 40, 0, 0)), kv)
        assertEquals(2, tracker2.states[1].snoozeCount)
    }

    @Test
    fun snoozeCapsAtMaxAndSafetyBlockingCannotSnooze() {
        val kv = InMemoryKeyValueStore()
        val provider = FakeCountsProvider(listOf(1, 40, 0, 0))
        val tracker = ConsumableTracker(provider, kv)

        // Intranasal (index 0) is safety-blocking — snooze is refused.
        tracker.snooze(0)
        assertEquals(0, tracker.states[0].snoozeCount)

        // Hydrogel: performance-critical, max 3.
        repeat(5) { tracker.snooze(1) }
        assertEquals(3, tracker.states[1].snoozeCount)
    }

    @Test
    fun markReplacedResetsCountsAndSnooze() {
        val kv = InMemoryKeyValueStore()
        val tracker = ConsumableTracker(FakeCountsProvider(listOf(0, 40, 0, 0)), kv)
        tracker.snooze(1)
        tracker.markReplaced(1)
        assertEquals(0, tracker.states[1].sessionCount)
        assertEquals(0, tracker.states[1].snoozeCount)
        assertFalse(tracker.states[1].isLow)
    }

    @Test
    fun duplicateCountUpdatesAreIgnored() {
        val provider = FakeCountsProvider(listOf(0, 37, 0, 0))
        var notifications = 0
        ConsumableTracker(provider, InMemoryKeyValueStore(), onReminderNotification = { notifications++ })
        val after = notifications
        provider.emit(listOf(0, 37, 0, 0)) // identical — removeDuplicates parity
        assertEquals(after, notifications)
    }
}
