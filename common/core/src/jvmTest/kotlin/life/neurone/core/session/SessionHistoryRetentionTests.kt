package life.neurone.core.session

import kotlinx.datetime.LocalDate
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.PersistedKeys
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** How long the session history keeps a session is the person's choice, with no age limit by default (`REQ-SW03-32`, `OI-PRIV-10` (2)). */
class SessionHistoryRetentionTests {
    private var today = LocalDate(2026, 11, 19)

    private fun store(kv: InMemoryKeyValueStore) = SessionHistoryStore(kv, today = { today })

    private fun session(day: LocalDate, name: String = day.toString()) = CompletedSessionSummary(
        protocolName = name, completedAt = day, durationSeconds = 600,
        averageCoherenceScore = null, rmssdMilliseconds = null, impedancePassCount = 8,
    )

    // @verifies REQ-SW03-32#a
    @Test fun the_default_keeps_a_session_whatever_its_age() {
        val kv = InMemoryKeyValueStore()
        val history = store(kv)
        assertEquals(HistoryRetention.NO_LIMIT, history.retention.value)
        history.record(session(LocalDate(2020, 1, 1)))
        today = LocalDate(2030, 1, 1)
        assertEquals(1, store(kv).records.size)
    }

    // @verifies REQ-SW03-32#a
    @Test fun choosing_a_period_removes_older_sessions_at_once_and_on_disk() {
        val kv = InMemoryKeyValueStore()
        val history = store(kv)
        history.record(session(LocalDate(2026, 11, 19)))
        history.record(session(LocalDate(2026, 10, 20))) // exactly 30 days ago: kept
        history.record(session(LocalDate(2026, 10, 19))) // 31 days ago: removed
        assertTrue(history.setRetention(HistoryRetention.DAYS_30))
        assertEquals(listOf("2026-11-19", "2026-10-20"), history.records.map { it.sessionDay })
        assertEquals("DAYS_30", kv.getString(PersistedKeys.SESSION_HISTORY_RETENTION_KEY))

        val reloaded = store(kv)
        assertEquals(HistoryRetention.DAYS_30, reloaded.retention.value)
        assertEquals(2, reloaded.records.size)
    }

    // @verifies REQ-SW03-32#a
    @Test fun a_session_that_ages_past_the_choice_leaves_when_the_history_is_next_read() {
        val kv = InMemoryKeyValueStore()
        store(kv).apply { setRetention(HistoryRetention.DAYS_90); record(session(LocalDate(2026, 11, 1))) }
        today = LocalDate(2027, 3, 1)
        val later = store(kv)
        assertTrue(later.records.isEmpty())
        assertTrue(kv.getString(PersistedKeys.SESSION_HISTORY_KEY)!!.trim() == "[]", "and it is gone from the stored history too")
    }

    // @verifies REQ-SW03-32#a
    @Test fun an_unreadable_stored_choice_is_the_default_and_removes_nothing() {
        val kv = InMemoryKeyValueStore()
        store(kv).record(session(LocalDate(2019, 5, 5)))
        kv.putString(PersistedKeys.SESSION_HISTORY_RETENTION_KEY, "FOREVER-ISH")
        val history = store(kv)
        assertEquals(HistoryRetention.NO_LIMIT, history.retention.value)
        assertEquals(1, history.records.size)
    }

    // @verifies REQ-SW03-32#b
    @Test fun clearing_removes_the_history_and_keeps_the_choice() {
        val kv = InMemoryKeyValueStore()
        val history = store(kv)
        history.setRetention(HistoryRetention.DAYS_365)
        history.record(session(today))
        history.clearHistory()
        assertTrue(history.records.isEmpty())
        assertNull(kv.getString(PersistedKeys.SESSION_HISTORY_KEY))
        assertEquals(HistoryRetention.DAYS_365, store(kv).retention.value)
    }
}
