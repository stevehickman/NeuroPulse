package com.neuropulse.core.models

// Shared session state model — port of NeuroPulseShared/SessionState.swift.
// Raw values must remain byte-identical to the hub firmware and the iOS app.

enum class SessionStatus(val rawValue: Int) {
    IDLE(0),
    RUNNING(1),
    PAUSED(2),
    COMPLETED(3);

    companion object {
        fun from(rawValue: Int): SessionStatus? = entries.firstOrNull { it.rawValue == rawValue }
    }
}

enum class PacerPhase(val rawValue: Int) {
    INHALE(0),
    EXHALE(1);

    companion object {
        fun from(rawValue: Int): PacerPhase? = entries.firstOrNull { it.rawValue == rawValue }
    }
}

data class HRVData(
    // Derived from GATT HRV_COHERENCE characteristic (uint16 / 100). 0.0–10.0
    val coherenceScore: Float,
    val rmssdMilliseconds: Int,
)

data class SessionState(
    // Hub session clock (truncated uint32, used for offset sync only).
    // UHDR-class data (session timestamp, CLAUDE.md §5.1) — must never be
    // persisted to backed-up storage or transmitted to analytics.
    val epoch: Long,
    val protocolId: Int,
    val status: SessionStatus,
    val hrv: HRVData?,
    val pacerPhase: PacerPhase,
    val pacerElapsedPercent: Int,       // 0–100, fraction of current breath phase elapsed
    // UHDR-class: raw per-electrode bitmask at named scalp positions
    // (CLAUDE.md §5.1 "Raw EEG impedance → UHDR"). Display only.
    val impedancePassFlags: Int,
    val consumableSessionCounts: List<Int>, // 4 values: intranasal/hydrogel/VNS/audio
) {
    companion object {
        val EMPTY = SessionState(
            epoch = 0L,
            protocolId = 0,
            status = SessionStatus.IDLE,
            hrv = null,
            pacerPhase = PacerPhase.INHALE,
            pacerElapsedPercent = 0,
            impedancePassFlags = 0,
            consumableSessionCounts = listOf(0, 0, 0, 0),
        )
    }
}
