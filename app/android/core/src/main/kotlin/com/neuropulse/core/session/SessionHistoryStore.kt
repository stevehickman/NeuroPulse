package com.neuropulse.core.session

import com.neuropulse.core.common.KeyValueStore
import java.time.LocalDate
import kotlinx.serialization.Serializable
import kotlinx.serialization.Transient
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.json.Json

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
)

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
    companion object {
        /** Reconstruct a display summary from a persisted history row (no adaptation events). */
        fun fromRecord(record: SessionRecord): CompletedSessionSummary = CompletedSessionSummary(
            protocolName = record.protocolName,
            completedAt = runCatching { LocalDate.parse(record.sessionDay) }.getOrDefault(LocalDate.MIN),
            durationSeconds = record.durationSeconds.toInt(),
            averageCoherenceScore = record.averageCoherenceScore,
            rmssdMilliseconds = record.rmssdMilliseconds,
            impedancePassCount = record.impedancePassCount,
            edfSessionId = record.edfSessionId,
            adaptationEvents = emptyList(),
        )
    }
}

class SessionHistoryStore(
    private val store: KeyValueStore,
    private val json: Json = Json { ignoreUnknownKeys = true },
) {
    companion object {
        const val STORAGE_KEY = "np.session.history"
        const val MAX_RECORDS = 100

        // Companion-scoped so it exists before the instance init block runs
        // (a body-declared val would still be null during init { load() }).
        private val newestFirst = Comparator<SessionRecord> { a, b ->
            if (a.sessionDay != b.sessionDay) {
                b.sessionDay.compareTo(a.sessionDay)
            } else {
                b.insertionIndex.compareTo(a.insertionIndex)
            }
        }
    }

    var records: List<SessionRecord> = emptyList()
        private set

    private var nextInsertionIndex = 0

    init {
        load()
    }

    /** Append a completed session, persist, trim, and keep newest-first. */
    fun record(session: CompletedSessionSummary) {
        val new = SessionRecord(
            id = java.util.UUID.randomUUID().toString(),
            protocolName = session.protocolName,
            sessionDay = session.completedAt.toString(), // ISO yyyy-MM-dd
            insertionIndex = nextInsertionIndex,
            durationSeconds = session.durationSeconds.toDouble(),
            averageCoherenceScore = session.averageCoherenceScore,
            rmssdMilliseconds = session.rmssdMilliseconds,
            impedancePassCount = session.impedancePassCount,
            edfSessionId = session.edfSessionId,
        )
        nextInsertionIndex += 1
        records = (records + new).sortedWith(newestFirst).take(MAX_RECORDS)
        persist()
    }

    fun clearHistory() {
        records = emptyList()
        nextInsertionIndex = 0
        persist()
    }

    // ── Persistence ──────────────────────────────────────────────────────

    private fun load() {
        val blob = store.getString(STORAGE_KEY) ?: return
        // Corrupt blob is non-fatal — start clean rather than crash on launch.
        records = runCatching {
            json.decodeFromString(ListSerializer(SessionRecord.serializer()), blob)
                .sortedWith(newestFirst)
        }.getOrDefault(emptyList())
        nextInsertionIndex = (records.maxOfOrNull { it.insertionIndex } ?: -1) + 1
    }

    private fun persist() {
        store.putString(
            STORAGE_KEY,
            json.encodeToString(ListSerializer(SessionRecord.serializer()), records),
        )
    }
}
