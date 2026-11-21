package life.neurone.core.consumable

import life.neurone.core.common.PresenceInstall
import life.neurone.core.protocol.PersistedKeys
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The consumable reminder engine under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-19`). The session counts are live from the hub;
 * the snooze counts are stored and sealed. A store built locked read 0 snoozes and its first persist replaced the sealed counts, so
 * the snooze limits of `CLAUDE.md` §5.2 reset on every launch.
 */
class ConsumableTrackerPresenceTests {

    private class Hub(private val counts: List<Int>) : ConsumableCountsProviding {
        override fun observe(listener: (List<Int>) -> Unit): AutoCloseable {
            listener(counts)
            return AutoCloseable { }
        }
    }

    private val sealed = setOf(PersistedKeys.SNOOZE_KEY)
    private val hubCounts = listOf(0, 37, 0, 0) // the hydrogel tip: 8 remaining, so low and snoozable

    /** An install whose sealed snooze count for the hydrogel is 2, left as an earlier run would. */
    private fun installWithSnoozes(): PresenceInstall = PresenceInstall { sealed }.also { install ->
        val run = install.Run()
        run.unlock()
        val tracker = ConsumableTracker(Hub(hubCounts), run.kv, lock = run.lock)
        tracker.snooze(1)
        tracker.snooze(1)
        check(tracker.states[1].snoozeCount == 2)
        run.lock.lock()
    }

    // @verifies REQ-PRES-01#d
    @Test fun the_sealed_snooze_counts_are_not_replaced_by_the_zeros_of_a_locked_read() {
        val install = installWithSnoozes()
        val run = install.Run()
        val shown = mutableListOf<ConsumableReminder>()
        val tracker = ConsumableTracker(Hub(hubCounts), run.kv, onReminderNotification = { shown += it }, lock = run.lock)

        assertFalse(tracker.snoozeIsReadable)
        assertEquals(0, tracker.states[1].snoozeCount, "locked, the snooze count is not readable")
        assertTrue(tracker.activeReminders.isNotEmpty(), "a locked reminder can repeat but never goes missing")

        run.unlock()

        assertTrue(tracker.snoozeIsReadable)
        assertEquals(2, tracker.states[1].snoozeCount, "the sealed count came back, not a reset")

        // And what was stored is still 2: the next run reads it.
        run.lock.lock()
        val next = install.Run()
        val again = ConsumableTracker(Hub(hubCounts), next.kv, lock = next.lock)
        next.unlock()
        assertEquals(2, again.states[1].snoozeCount)
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_snooze_made_while_locked_is_added_to_the_sealed_count_at_the_unlock() {
        val install = installWithSnoozes()
        val run = install.Run()
        val tracker = ConsumableTracker(Hub(hubCounts), run.kv, lock = run.lock)

        tracker.snooze(1)
        assertEquals(0, tracker.states[1].snoozeCount)

        run.unlock()

        assertEquals(3, tracker.states[1].snoozeCount, "2 sealed and 1 made while locked")
    }

    // The hub owns the count, so it is told at once; the stored snooze count is zeroed on what is read at the unlock.
    // @verifies REQ-PRES-01#d
    @Test fun a_replacement_made_while_locked_tells_the_hub_at_once_and_clears_the_sealed_snoozes_at_the_unlock() {
        val install = installWithSnoozes()
        val run = install.Run()
        val told = mutableListOf<Int>()
        val tracker = ConsumableTracker(Hub(hubCounts), run.kv, onReplaced = { told += it }, lock = run.lock)

        tracker.markReplaced(1)
        assertEquals(listOf(1), told, "the hub is told now")

        run.unlock()

        assertEquals(0, tracker.states[1].snoozeCount)
        run.lock.lock()
        val next = install.Run()
        val again = ConsumableTracker(Hub(hubCounts), next.kv, lock = next.lock)
        next.unlock()
        assertEquals(0, again.states[1].snoozeCount, "the cleared count is what was stored")
    }
}
