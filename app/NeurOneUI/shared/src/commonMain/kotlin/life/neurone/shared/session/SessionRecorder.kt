package life.neurone.shared.session

import kotlin.time.TimeSource
import kotlinx.datetime.Clock
import kotlinx.datetime.TimeZone
import kotlinx.datetime.todayIn
import life.neurone.core.models.SessionState
import life.neurone.core.models.SessionStatus
import life.neurone.core.session.CompletedSessionSummary
import life.neurone.core.session.SessionHistoryStore

/**
 * Puts a finished session into the history when the hub says it is complete (the Swift app's `handleStatusChange`). It follows the hub's
 * reports and never decides a session is over itself (ISC-34): it remembers when a session started running and what the live figures
 * last were, and on running to completed it writes one row, with the day and not the time (`REQ-SW03-06`) and the hub's session id only
 * in memory (`REQ-SW03-13`). A pause and a resume are the same session.
 */
class SessionRecorder(
    private val history: SessionHistoryStore,
    private val clock: TimeSource = TimeSource.Monotonic,
    private val today: () -> kotlinx.datetime.LocalDate = { Clock.System.todayIn(TimeZone.currentSystemDefault()) },
) {
    private var startedAt: kotlin.time.TimeMark? = null
    private var protocolLabel: String = ""
    private var previous: SessionStatus = SessionStatus.IDLE
    private var lastCoherence: Float? = null
    private var lastRmssd: Int? = null
    private var lastImpedancePass: Int = 0

    /**
     * Take the hub's latest report. [protocolLabel] words the running protocol for the history row; it is asked for once, when a session
     * starts. Returns the summary when this report completed a session, so the screen can offer its download.
     */
    suspend fun observe(session: SessionState, protocolLabel: suspend (protocolId: Int) -> String): CompletedSessionSummary? {
        val now = session.status
        session.hrv?.let { lastCoherence = it.coherenceScore; lastRmssd = it.rmssdMilliseconds }
        lastImpedancePass = (0 until 8).count { (session.impedancePassFlags and (1 shl it)) != 0 }
        var finished: CompletedSessionSummary? = null
        when {
            now == SessionStatus.RUNNING && previous == SessionStatus.IDLE -> {
                startedAt = clock.markNow()
                this.protocolLabel = protocolLabel(session.protocolId)
                lastCoherence = session.hrv?.coherenceScore
                lastRmssd = session.hrv?.rmssdMilliseconds
            }
            now == SessionStatus.RUNNING && previous == SessionStatus.PAUSED -> Unit
            now == SessionStatus.COMPLETED && previous == SessionStatus.RUNNING -> {
                val seconds = startedAt?.elapsedNow()?.inWholeSeconds?.coerceAtLeast(0)?.toInt() ?: 0
                finished = CompletedSessionSummary(
                    protocolName = this.protocolLabel,
                    completedAt = today(),
                    durationSeconds = seconds,
                    averageCoherenceScore = lastCoherence,
                    rmssdMilliseconds = lastRmssd,
                    impedancePassCount = lastImpedancePass,
                    edfSessionId = edfSessionIdFrom(session.epoch),
                )
                history.record(finished)
                startedAt = null
            }
        }
        previous = now
        return finished
    }
}
