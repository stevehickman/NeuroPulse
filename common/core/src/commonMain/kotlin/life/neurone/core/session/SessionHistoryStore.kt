package life.neurone.core.session

import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.PresenceState
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.protocol.PersistedKeys
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.datetime.Clock
import kotlinx.datetime.DateTimeUnit
import kotlinx.datetime.LocalDate
import kotlinx.datetime.TimeZone
import kotlinx.datetime.minus
import kotlinx.datetime.todayIn
import kotlinx.serialization.Serializable
import kotlinx.serialization.Transient
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.json.Json
import kotlin.math.floor
import kotlin.math.round

// Local, display-only record of completed sessions. Port of iOS
// SessionHistoryStore.swift.
//
// PRIVACY (UHDR boundary): this store NEVER persists raw EEG/HRV waveform
// data — only display-aggregated metrics already shown post-session.
//
// Exact completion timestamps are UHDR-class data (CLAUDE.md §5.1 — "session
// timestamps → UHDR") and are NOT stored here. Only the calendar day is
// persisted (sessionDay, "yyyy-MM-dd" local timezone) plus a monotonic
// insertionIndex for same-day ordering (NP-PRIV-ANALYSIS-002 MEDIUM-07).
//
// edfSessionId is in-memory only (@Transient) — the hub session epoch is a
// Unix-ms timestamp and must not reach persisted storage.
//
// What a record keeps is what the History screen shows and no finer (OI-PRIV-10 (2)): the
// duration in whole minutes and the coherence score to one decimal place. protocolName is kept
// as text because it is the row's title; a protocol the person named themselves is their own
// wording, which is one reason this key is UHDR-tier and sealed.
//
// PRESENCE (NP-APP-PRESENCE-001, REQ-PRES-01 (d)): under the gate this store is built while
// the app is locked, when a read of the sealed history answers "absent". It therefore never
// treats a locked read as an empty history: it holds nothing while locked, reads again at
// each unlock, and a session that finishes while locked waits and is merged then.

@Serializable
data class SessionRecord(
    val id: String,
    val protocolName: String,
    // Calendar day the session completed — "yyyy-MM-dd", device local timezone.
    val sessionDay: String,
    // Monotonic insertion counter; tiebreaks same-day records newest-first.
    val insertionIndex: Int,
    val durationSeconds: Double,
    val averageCoherenceScore: Float? = null, // 0.0–10.0; null when protocol had no HRV
    val rmssdMilliseconds: Int? = null,       // integer only — never a raw RR series
    val impedancePassCount: Int,              // 0–8 EEG electrodes that passed
    // Deliberately excluded from serialization — UHDR-class (see header comment).
    @Transient val edfSessionId: Long? = null,
) {
    /**
     * Names the record and nothing in it (`SWR-MG-28` (f)). Every field is about the person (what they ran, when, their
     * coherence and RMSSD), so the generated `toString` would write UHDR into any log line or exception message a record
     * is formatted into. Serialization is unaffected: it does not go through `toString`.
     */
    override fun toString(): String = "SessionRecord(<redacted>)"
}

// Post-session summary handed in by the session flow (in-memory only).
data class CompletedSessionSummary(
    val protocolName: String,
    val completedAt: LocalDate,
    val durationSeconds: Int,
    val averageCoherenceScore: Float?,
    val rmssdMilliseconds: Int?,
    val impedancePassCount: Int,
    val edfSessionId: Long? = null,
    // Closed-loop adaptation events (UHDR-class). Populated by the live post-session path;
    // NEVER persisted (see store header), so a summary reconstructed from a history record has
    // an empty list — the Adaptive Adjustments card then shows its empty state, as on iOS.
    val adaptationEvents: List<AdaptationEvent> = emptyList(),
) {
    /** Names the summary and nothing in it, for the reason [SessionRecord.toString] gives (`SWR-MG-28` (f)). */
    override fun toString(): String = "CompletedSessionSummary(<redacted>)"

    companion object {
        /** Reconstruct a display summary from a persisted history row (no adaptation events). */
        fun fromRecord(record: SessionRecord): CompletedSessionSummary = CompletedSessionSummary(
            protocolName = record.protocolName,
            completedAt = runCatching { LocalDate.parse(record.sessionDay) }.getOrDefault(LocalDate(-100000, 1, 1)),
            durationSeconds = record.durationSeconds.toInt(),
            averageCoherenceScore = record.averageCoherenceScore,
            rmssdMilliseconds = record.rmssdMilliseconds,
            impedancePassCount = record.impedancePassCount,
            edfSessionId = record.edfSessionId,
            adaptationEvents = emptyList(),
        )
    }
}

/**
 * How long the session history keeps a session, counted from the day it finished (`REQ-SW03-33` (a); the principal, 2026-11-19,
 * `OI-PRIV-10` (2)). The person chooses; [NO_LIMIT] is the default. The periods offered are a menu, not limits anything derives
 * (`UC-196`).
 */
enum class HistoryRetention(val days: Int?) {
    NO_LIMIT(null),
    DAYS_30(30),
    DAYS_90(90),
    DAYS_365(365);

    companion object {
        val DEFAULT = NO_LIMIT

        /** A stored choice; anything unreadable is the default, which removes nothing. */
        fun parse(stored: String?): HistoryRetention = entries.firstOrNull { it.name == stored } ?: DEFAULT
    }
}

class SessionHistoryStore(
    private val store: KeyValueStore,
    private val json: Json = Json { ignoreUnknownKeys = true },
    /**
     * The presence lock when the host gates UHDR (`NP-APP-PRESENCE-001`), or null when it does not. With a lock, the history is in
     * memory only while the person's presence stands (`REQ-PRES-01` (d), `OI-PRIV-10` (1)).
     */
    private val lock: PresenceLock? = null,
    /** Today's date on the device, by which a session's age is counted for [retention]. Injected so the rule is testable. */
    private val today: () -> LocalDate = { Clock.System.todayIn(TimeZone.currentSystemDefault()) },
) {
    companion object {
        // Companion-scoped so it exists before the instance init block runs
        // (a body-declared val would still be null during init { load() }).
        private val newestFirst = Comparator<SessionRecord> { a, b ->
            if (a.sessionDay != b.sessionDay) {
                b.sessionDay.compareTo(a.sessionDay)
            } else {
                b.insertionIndex.compareTo(a.insertionIndex)
            }
        }

        /**
         * A record holds no more than the history shows (`OI-PRIV-10` (2)): the duration in whole minutes (kept in seconds, so a
         * record written by an earlier build or another platform still reads) and the coherence score to one decimal place.
         */
        internal fun wholeMinutes(seconds: Double): Double = if (seconds.isFinite() && seconds > 0) floor(seconds / 60.0) * 60.0 else 0.0
        internal fun oneDecimal(score: Float?): Float? = score?.takeIf { it.isFinite() }?.let { round(it * 10f) / 10f }

        private fun asShown(r: SessionRecord): SessionRecord =
            r.copy(durationSeconds = wholeMinutes(r.durationSeconds), averageCoherenceScore = oneDecimal(r.averageCoherenceScore))
    }

    private val _records = MutableStateFlow<List<SessionRecord>>(emptyList())

    /** The history, newest first. **Empty while locked**, whatever is sealed and whatever finished since the lock closed. */
    var records: List<SessionRecord>
        get() = _records.value
        private set(value) { _records.value = value }

    /** [records], for a screen that redraws when the history changes (a clear, a new session, the lock). */
    val recordsFlow: StateFlow<List<SessionRecord>> = _records.asStateFlow()

    private val _retention = MutableStateFlow(HistoryRetention.DEFAULT)

    /**
     * How long the person chose to keep the history (`REQ-SW03-33` (a); the principal, 2026-11-19). It is the person's record, so the
     * period is theirs to set and none is set for them: the default keeps a session with no age limit. Read with the history, so it is
     * the default while locked.
     */
    val retention: StateFlow<HistoryRetention> = _retention.asStateFlow()

    private var nextInsertionIndex = 0

    /**
     * Whether [records] is what the store holds. False while locked: a locked read answers "absent", which is not "empty", and a
     * list built on it would replace the sealed history when it was written (`OI-PRIV-10` (1)).
     */
    private var loaded = false

    /** Sessions that finished while the history could not be read, oldest first. They join it at the next unlock, in this order. */
    private val waiting = mutableListOf<CompletedSessionSummary>()

    init {
        if (lock == null || lock.isUnlocked) load()
        lock?.onStateChanged { state -> if (state == PresenceState.LOCKED) forget() else reload() }
    }

    /**
     * Append a completed session, persist, trim, and keep newest-first. While locked nothing is read or written: the session waits
     * in memory and is added to the sealed history at the next unlock, so it neither replaces that history nor is lost.
     */
    fun record(session: CompletedSessionSummary) {
        if (!loaded) {
            waiting += session
            return
        }
        append(session)
        if (!persist()) {
            // The lock closed between the check and the write: nothing reached the disk, so the session waits like any other.
            forget()
            waiting += session
        }
    }

    /**
     * Keep the history for [choice] from now on, and remove at once every session older than it (`REQ-SW03-33` (a)). False, changing
     * nothing, while the history cannot be read: the choice is stored beside the sealed history and a period applied to a list read
     * while locked would apply to nothing.
     */
    fun setRetention(choice: HistoryRetention): Boolean {
        if (!loaded || (lock != null && !lock.isUnlocked)) return false
        val before = records
        val pruned = withinRetention(before, choice)
        try {
            store.putString(PersistedKeys.SESSION_HISTORY_RETENTION_KEY, choice.name)
        } catch (e: SecretKeyUnavailableException) {
            return false
        }
        _retention.value = choice
        records = pruned
        if (pruned.size != before.size) persist()
        return true
    }

    /**
     * Destroy the history. It works locked: a removal discloses nothing and needs no presence (`REQ-PRES-01` (b)), and a session
     * that was waiting to join the history is dropped with it.
     */
    fun clearHistory() {
        records = emptyList()
        nextInsertionIndex = 0
        waiting.clear()
        store.remove(PersistedKeys.SESSION_HISTORY_KEY)
    }

    // ── Presence ─────────────────────────────────────────────────────────

    /**
     * Read the stored history again, for after a restore wrote it under this store (`OI-PRIV-19`): the store reads once, so without
     * this the restored history would not show, and the next session would be written over it. Does nothing while locked.
     */
    fun reloadFromStorage() {
        if (lock != null && !lock.isUnlocked) return
        reload()
    }

    /** The lock closed: nothing of the person's history stays decrypted in this store. */
    private fun forget() {
        records = emptyList()
        nextInsertionIndex = 0
        _retention.value = HistoryRetention.DEFAULT
        loaded = false
    }

    /** The lock opened: read the sealed history, then add what finished while it was closed and write the two together. */
    private fun reload() {
        load()
        if (!loaded || waiting.isEmpty()) return
        val joining = waiting.toList()
        joining.forEach(::append)
        if (persist()) waiting.clear() else forget()
    }

    private fun append(session: CompletedSessionSummary) {
        val new = SessionRecord(
            id = life.neurone.core.common.UUID.randomUUID().toString(),
            protocolName = session.protocolName,
            sessionDay = session.completedAt.toString(), // ISO yyyy-MM-dd
            insertionIndex = nextInsertionIndex,
            durationSeconds = wholeMinutes(session.durationSeconds.toDouble()),
            averageCoherenceScore = oneDecimal(session.averageCoherenceScore),
            rmssdMilliseconds = session.rmssdMilliseconds,
            impedancePassCount = session.impedancePassCount,
            edfSessionId = session.edfSessionId,
        )
        nextInsertionIndex += 1
        records = withinRetention((records + new).sortedWith(newestFirst), _retention.value)
    }

    /** The records whose day is within [choice] of today; all of them when the choice has no age limit. */
    private fun withinRetention(list: List<SessionRecord>, choice: HistoryRetention): List<SessionRecord> {
        val days = choice.days ?: return list
        val oldest = today().minus(days, DateTimeUnit.DAY).toString() // ISO dates compare as text
        return list.filter { it.sessionDay >= oldest }
    }

    // ── Persistence ──────────────────────────────────────────────────────

    private fun load() {
        if (lock != null && !lock.isUnlocked) return
        val blob = store.getString(PersistedKeys.SESSION_HISTORY_KEY)
        // Corrupt blob is non-fatal — start clean rather than crash on launch.
        records = if (blob == null) emptyList() else runCatching {
            json.decodeFromString(ListSerializer(SessionRecord.serializer()), blob)
                .map(::asShown).sortedWith(newestFirst)
        }.getOrDefault(emptyList())
        nextInsertionIndex = (records.maxOfOrNull { it.insertionIndex } ?: -1) + 1
        _retention.value = HistoryRetention.parse(store.getString(PersistedKeys.SESSION_HISTORY_RETENTION_KEY))
        loaded = true
        // A session that has aged past the person's choice since the history was last written leaves it now, on disk as well.
        val kept = withinRetention(records, _retention.value)
        if (kept.size != records.size) {
            records = kept
            persist()
        }
    }

    /** False when the store could not take the write because the key is not readable; nothing was written. */
    private fun persist(): Boolean {
        if (lock != null && !lock.isUnlocked) return false
        return try {
            store.putString(
                PersistedKeys.SESSION_HISTORY_KEY,
                json.encodeToString(ListSerializer(SessionRecord.serializer()), records),
            )
            true
        } catch (e: SecretKeyUnavailableException) {
            false
        }
    }
}
