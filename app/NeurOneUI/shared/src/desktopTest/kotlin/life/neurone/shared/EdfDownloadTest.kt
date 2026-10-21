package life.neurone.shared

import kotlinx.coroutines.test.runTest
import life.neurone.core.models.SessionState
import life.neurone.core.models.SessionStatus
import life.neurone.core.models.HRVData
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.session.SessionHistoryStore
import life.neurone.core.migration.ObjectStore
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.session.EdfDownloadModel
import life.neurone.shared.session.EdfProblem
import life.neurone.shared.session.EdfState
import life.neurone.shared.session.SessionRecorder
import life.neurone.shared.session.UhdrFolder
import life.neurone.shared.session.edfFileName
import life.neurone.shared.session.edfSessionIdFrom
import life.neurone.shared.session.shouldShowSessionDownload
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** Mode 4: fetching and keeping a finished session's EDF+ recording, and writing the session into the history. */
class EdfDownloadTest {

    private class Folder : UhdrFolder {
        override val store: ObjectStore = MemoryObjectStore()
        val protectedNames = mutableListOf<String>()
        override fun protect(name: String) { protectedNames += name }
    }

    private fun edf(size: Int = 512): ByteArray = ByteArray(size) { ' '.code.toByte() }.also { it[0] = '0'.code.toByte() }

    private fun model(folder: Folder, connected: Boolean = true, asked: MutableList<Long> = mutableListOf(), onWait: suspend (Long) -> Unit = {}) =
        EdfDownloadModel(folder, connected = { connected }, ask = { asked += it }, wait = onWait)

    // @verifies REQ-SW03-26#a
    @Test fun a_hub_that_is_not_connected_is_not_asked_and_is_reported() = runTest {
        val asked = mutableListOf<Long>()
        val m = model(Folder(), connected = false, asked = asked)
        m.download(7)
        assertEquals(EdfState.Failed(EdfProblem.NOT_CONNECTED), m.state.value)
        assertTrue(asked.isEmpty())
    }

    // @verifies REQ-SW03-26#b
    // @verifies REQ-UHDRFP-01#a
    @Test fun the_download_ends_when_the_file_lands_and_the_file_is_protected_as_it_lands() = runTest {
        val folder = Folder()
        val asked = mutableListOf<Long>()
        var polls = 0
        val m = model(folder, asked = asked) {
            polls++
            if (polls == 3) folder.store.put(edfFileName(7), edf())
        }
        m.download(7)
        assertEquals(EdfState.Downloaded("session_7.edf"), m.state.value)
        assertEquals(listOf(7L), asked, "the hub is asked once")
        assertEquals(3, polls, "it stops looking as soon as the file is there")
        assertEquals(listOf("session_7.edf"), folder.protectedNames)
    }

    // @verifies REQ-SW03-26#c
    @Test fun a_file_that_never_arrives_is_reported_after_thirty_seconds() = runTest {
        var waitedMillis = 0L
        val m = model(Folder()) { waitedMillis += it }
        m.download(7)
        assertEquals(EdfState.Failed(EdfProblem.NO_DATA), m.state.value)
        assertEquals(30_000L, waitedMillis)
    }

    // @verifies REQ-SW03-27#a
    @Test fun a_chosen_recording_is_kept_byte_for_byte_and_protected() = runTest {
        val folder = Folder()
        val m = model(folder)
        val bytes = edf(1024).also { it[100] = 'X'.code.toByte() }
        m.adopt(9, bytes)
        assertEquals(EdfState.Downloaded("session_9.edf"), m.state.value)
        assertContentEquals(bytes, folder.store.get("session_9.edf"))
        assertEquals(listOf("session_9.edf"), folder.protectedNames)
    }

    // @verifies REQ-SW03-27#b
    @Test fun a_chosen_file_that_is_not_an_edf_recording_is_refused_and_nothing_is_stored() = runTest {
        val folder = Folder()
        val m = model(folder)
        m.adopt(9, ByteArray(100))
        assertEquals(EdfState.Failed(EdfProblem.NOT_EDF), m.state.value, "too short")
        m.adopt(9, ByteArray(512) { 'a'.code.toByte() })
        assertEquals(EdfState.Failed(EdfProblem.NOT_EDF), m.state.value, "wrong version field")
        assertNull(folder.store.get("session_9.edf"))
        assertTrue(folder.protectedNames.isEmpty())
    }

    // @verifies REQ-SW03-13
    @Test fun the_download_is_offered_only_for_a_completed_session_with_a_hub_id() {
        assertTrue(shouldShowSessionDownload(SessionStatus.COMPLETED, 1001))
        assertFalse(shouldShowSessionDownload(SessionStatus.COMPLETED, 0))
        assertFalse(shouldShowSessionDownload(SessionStatus.RUNNING, 1001))
        assertFalse(shouldShowSessionDownload(SessionStatus.PAUSED, 1001))
        assertFalse(shouldShowSessionDownload(SessionStatus.IDLE, 1001))
        assertNull(edfSessionIdFrom(0))
        assertEquals(1001L, edfSessionIdFrom(1001))
    }

    // @verifies REQ-SW03-06
    // @verifies REQ-SW03-13
    @Test fun a_session_is_recorded_when_the_hub_reports_it_complete_with_the_day_and_not_the_time() = runTest {
        val history = SessionHistoryStore(InMemoryKeyValueStore())
        val day = kotlinx.datetime.LocalDate(2026, 10, 21)
        val recorder = SessionRecorder(history, today = { day })
        val label: suspend (Int) -> String = { "Protocol $it" }
        val hrv = HRVData(coherenceScore = 6.5f, rmssdMilliseconds = 42)
        val idle = SessionState.EMPTY
        val running = idle.copy(status = SessionStatus.RUNNING, protocolId = 3, hrv = hrv, impedancePassFlags = 0b1111)
        assertNull(recorder.observe(idle, label))
        assertNull(recorder.observe(running, label))
        assertNull(recorder.observe(running.copy(status = SessionStatus.PAUSED), label), "a pause is not an end")
        assertNull(recorder.observe(running, label), "a resume is not a start")
        val done = recorder.observe(running.copy(status = SessionStatus.COMPLETED, epoch = 1001), label)
        assertIs<life.neurone.core.session.CompletedSessionSummary>(done)
        assertEquals(1, history.records.size)
        val row = history.records.single()
        assertEquals("Protocol 3", row.protocolName)
        assertEquals("2026-10-21", row.sessionDay)
        assertEquals(6.5f, row.averageCoherenceScore)
        assertEquals(42, row.rmssdMilliseconds)
        assertEquals(4, row.impedancePassCount)
        assertEquals(1001L, row.edfSessionId)
    }

    // @verifies REQ-SW03-13
    @Test fun a_completed_session_with_no_hub_id_is_recorded_with_no_recording_to_fetch() = runTest {
        val history = SessionHistoryStore(InMemoryKeyValueStore())
        val recorder = SessionRecorder(history)
        val label: suspend (Int) -> String = { "Protocol $it" }
        recorder.observe(SessionState.EMPTY.copy(status = SessionStatus.RUNNING), label)
        recorder.observe(SessionState.EMPTY.copy(status = SessionStatus.COMPLETED, epoch = 0), label)
        assertNull(history.records.single().edfSessionId)
    }
}
