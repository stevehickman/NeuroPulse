package life.neurone.core.common

import life.neurone.core.protocol.NPIndividualProfile
import life.neurone.core.protocol.NPLimitsSet
import life.neurone.core.protocol.NPLimitsStore
import life.neurone.core.protocol.NPTDCSLimits
import life.neurone.core.session.CompletedSessionSummary
import life.neurone.core.session.SessionHistoryStore
import kotlinx.datetime.LocalDate
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * A restore writes carried keys straight to the store, under every store that read them at launch (`OI-PRIV-19`): each reads them again
 * on request, so the restored values show and the next save does not write over them.
 */
class ReloadFromStorageTests {

    // @verifies REQ-PRES-01#d
    @Test fun the_limits_store_shows_what_a_restore_wrote_and_does_not_write_over_it() {
        val kv = InMemoryKeyValueStore()
        val store = NPLimitsStore(kv)
        // Another store over the same keys stands for the restore's writes.
        val restored = NPLimitsStore(kv)
        restored.saveGlobalLimits(NPLimitsSet(name = "restored", tdcs = NPTDCSLimits(maxIntensityMilliamps = 1.0)))
        restored.saveProfile(NPIndividualProfile(id = "alex", name = "Alex"))

        store.reloadFromStorage()

        assertEquals(1.0, store.globalLimits!!.tdcs!!.maxIntensityMilliamps)
        assertEquals(listOf("alex"), store.profiles.map { it.id })
    }

    // @verifies REQ-PRES-01#d
    @Test fun the_session_history_shows_what_a_restore_wrote_and_the_next_session_joins_it() {
        val kv = InMemoryKeyValueStore()
        val history = SessionHistoryStore(kv)
        val restored = SessionHistoryStore(kv)
        restored.record(summary("from the backup"))

        history.reloadFromStorage()
        history.record(summary("new"))

        assertEquals(listOf("new", "from the backup"), history.records.map { it.protocolName })
    }

    private fun summary(name: String) = CompletedSessionSummary(
        protocolName = name, completedAt = LocalDate(2026, 7, 1), durationSeconds = 1200,
        averageCoherenceScore = null, rmssdMilliseconds = null, impedancePassCount = 8,
    )
}
