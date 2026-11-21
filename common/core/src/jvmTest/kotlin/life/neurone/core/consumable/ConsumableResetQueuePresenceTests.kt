package life.neurone.core.consumable

import life.neurone.core.common.PresenceInstall
import life.neurone.core.protocol.PersistedKeys
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * The replacement queue under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-19`): a replacement marked while locked must not replace
 * the sealed queue (an earlier one the hub was never told about would be lost), and the queue is written to the hub at the unlock.
 */
class ConsumableResetQueuePresenceTests {

    private val sealed = setOf(PersistedKeys.CONSUMABLE_RESET_KEY)

    private fun installWithQueued(vararg kinds: Int): PresenceInstall = PresenceInstall { sealed }.also { install ->
        val run = install.Run()
        run.unlock()
        val q = ConsumableResetQueue(run.kv, run.lock)
        kinds.forEach(q::add)
        run.lock.lock()
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_replacement_marked_while_locked_joins_the_sealed_queue_instead_of_replacing_it() {
        val run = installWithQueued(1).Run()
        var unlockedCalls = 0
        val queue = ConsumableResetQueue(run.kv, run.lock) { unlockedCalls++ }

        queue.add(2)
        assertEquals(listOf(2), queue.pending, "locked, only what was marked since the lock closed can be seen")

        run.unlock()

        assertEquals(listOf(1, 2), queue.pending, "the sealed replacement is still queued, with the new one")
        assertEquals(1, unlockedCalls, "the caller is told, so it can write the queue to the hub")
    }

    // @verifies REQ-PRES-01#d
    @Test fun what_is_held_while_locked_can_be_written_at_once_and_the_sealed_kinds_wait_for_the_unlock() {
        val run = installWithQueued(1).Run()
        val queue = ConsumableResetQueue(run.kv, run.lock)
        queue.add(3)

        val written = mutableListOf<Int>()
        queue.drain { written += it[0].toInt() }

        assertEquals(listOf(3), written, "the held kind is handed over now")
        run.unlock()
        assertEquals(listOf(1), queue.pending, "the sealed kind was not read, not sent and not lost")
    }
}
