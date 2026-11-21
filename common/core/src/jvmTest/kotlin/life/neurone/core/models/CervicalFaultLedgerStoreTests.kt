package life.neurone.core.models

import life.neurone.core.common.PresenceInstall
import life.neurone.core.protocol.PersistedKeys
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/**
 * Where each person's cardiac-fault acknowledgement point is kept (`OI-PRIV-19`): one sealed value, not one key per person that no
 * protected list names, and never replaced by a store built locked.
 */
class CervicalFaultLedgerStoreTests {

    private val key = PersistedKeys.CERVICAL_FAULT_LEDGER_KEY
    private val sealed = setOf(key)

    private fun installWith(vararg points: Pair<Long?, Long>): PresenceInstall = PresenceInstall { sealed }.also { install ->
        val run = install.Run()
        run.unlock()
        val store = CervicalFaultLedgerStore(run.kv, run.lock)
        points.forEach { (tag, n) -> store.set(tag, CervicalFaultLedger(n)) }
        run.lock.lock()
    }

    // @verifies REQ-PRES-01#d
    @Test fun each_persons_point_is_kept_under_the_one_sealed_key_and_not_under_a_key_per_person() {
        val install = installWith(7L to 12L, null to 3L)
        val run = install.Run()
        val store = CervicalFaultLedgerStore(run.kv, run.lock)
        run.unlock()

        assertEquals(12L, store.ledgerFor(7L).lastAcknowledgedSession)
        assertEquals(3L, store.ledgerFor(null).lastAcknowledgedSession)
        assertNull(install.host.getString("$key.7"), "no per-person key is written in the clear")
        assertNull(install.host.getString("$key.unnamed"))
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_ledger_built_locked_acknowledges_nothing_and_an_acknowledgement_made_then_joins_the_sealed_points() {
        val run = installWith(7L to 12L, 8L to 5L).Run()
        var told = 0
        val store = CervicalFaultLedgerStore(run.kv, run.lock) { told++ }

        assertNull(store.ledgerFor(7L).lastAcknowledgedSession, "locked, every fault shows and a cervical restart stays blocked")
        store.set(9L, CervicalFaultLedger(20L))
        assertEquals(20L, store.ledgerFor(9L).lastAcknowledgedSession, "an acknowledgement made now is not forgotten while locked")

        run.unlock()

        assertEquals(12L, store.ledgerFor(7L).lastAcknowledgedSession, "the sealed points were not replaced")
        assertEquals(5L, store.ledgerFor(8L).lastAcknowledgedSession)
        assertEquals(20L, store.ledgerFor(9L).lastAcknowledgedSession)
        assertEquals(1, told, "the caller re-reads its faults against the real points")
    }

    // @verifies REQ-PRES-01#d
    @Test fun a_per_person_key_an_earlier_build_left_in_the_clear_is_moved_into_the_sealed_value_and_removed() {
        val install = PresenceInstall { sealed }
        val run = install.Run()
        install.host.putString("$key.7", "41") // as the old code wrote it: unsealed, one key per person
        val store = CervicalFaultLedgerStore(run.kv, run.lock)
        run.unlock()

        assertEquals(41L, store.ledgerFor(7L).lastAcknowledgedSession)
        assertNull(install.host.getString("$key.7"), "the unsealed copy is gone")
        run.lock.lock()
        val next = install.Run()
        val again = CervicalFaultLedgerStore(next.kv, next.lock)
        next.unlock()
        assertEquals(41L, again.ledgerFor(7L).lastAcknowledgedSession, "and the value is what was sealed")
    }
}
