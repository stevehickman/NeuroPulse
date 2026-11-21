package life.neurone.core.analytics

import life.neurone.core.common.PresenceInstall
import kotlin.test.Test
import kotlin.test.assertEquals

/** The launch count under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-19`): a locked read of 0 must not become a write of 1. */
class EngagementTierPresenceTests {

    private val sealed = setOf(EngagementTier.LAUNCH_COUNT_KEY)

    private fun installWithLaunches(n: Int): PresenceInstall = PresenceInstall { sealed }.also { install ->
        val run = install.Run()
        run.unlock()
        run.kv.putInt(EngagementTier.LAUNCH_COUNT_KEY, n)
        run.lock.lock()
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_launch_is_counted_at_the_unlock_on_the_sealed_count_not_on_the_zero_of_a_locked_read() {
        val run = installWithLaunches(10).Run()

        EngagementTier.incrementLaunchCount(run.kv, run.lock) // at launch: locked

        run.unlock()

        assertEquals(11, run.kv.getInt(EngagementTier.LAUNCH_COUNT_KEY), "10 sealed and this launch, not a reset to 1")
        assertEquals(EngagementTier.ACTIVE, EngagementTier.current(run.kv))
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_launch_is_counted_once_however_many_times_the_lock_opens() {
        val run = installWithLaunches(10).Run()
        EngagementTier.incrementLaunchCount(run.kv, run.lock)

        run.unlock()
        run.lock.lock()
        run.unlock()

        assertEquals(11, run.kv.getInt(EngagementTier.LAUNCH_COUNT_KEY))
    }
}
