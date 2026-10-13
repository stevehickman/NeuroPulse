package life.neurone.core.models

// SESSION_PROGRESS (SWR-SS-07, OI-UI-KMP-09 R6): where the helmet says a session is. The figures are the helmet's own
// clock, never the app's, so they are right across a pause, a ramp and a reconnect. Wire layout: firmware/hub_control/
// include/np_session_progress.h, 22 bytes little-endian. Session timing is UHDR-class once it is tied to a person
// (CLAUDE.md §5.1): display only, never persisted or sent to analytics.

/** The hub's session state (`np_session_state_t`). */
enum class HubSessionState(val rawValue: Int) {
    IDLE(0), LOADING(1), VERIFYING(2), RUNNING(3), PAUSED(4), STOPPING(5), COMPLETE(6), FAULT(7);

    companion object {
        fun from(rawValue: Int): HubSessionState? = entries.firstOrNull { it.rawValue == rawValue }
    }
}

/** The gating ramp of a pause or a resume (`np_ramp_phase_t`). */
enum class RampPhase(val rawValue: Int) {
    NONE(0),

    /** A pause: stimulation is still on and falling. */
    DOWN(1),

    /** Paused, nothing ramping. */
    HELD(2),

    /** Cervical VNS impedance and cardiac baseline checks, before its ramp. */
    PRECHECK(3),

    /** Ramping up after a start or a resume. */
    UP(4);

    companion object {
        fun from(rawValue: Int): RampPhase? = entries.firstOrNull { it.rawValue == rawValue }
    }
}

data class SessionProgress(
    val state: HubSessionState,
    /** Index of the highest-numbered running command of the signed descriptor, or null when none runs. */
    val activePart: Int?,
    /** Bit n set = a running command drives modality type n (`np_hub_mod_type_t`). */
    val activeModalityMask: Int,
    val elapsedMs: Long,
    val remainingMs: Long,
    val totalMs: Long,
    val rampPhase: RampPhase,
    val rampPercent: Int,
    /** Null while it cannot be known (the cervical VNS checks). */
    val rampRemainingMs: Long?,
) {
    /** A session is in progress, paused or not: the controls apply (SWR-SS-05). */
    val isActive: Boolean
        get() = state == HubSessionState.RUNNING || state == HubSessionState.PAUSED || state == HubSessionState.STOPPING

    val isPaused: Boolean get() = state == HubSessionState.PAUSED

    companion object {
        /** A duration as a clock: `M:SS`, or `H:MM:SS` from an hour up. Digits only, so it needs no locale key. */
        fun formatClock(ms: Long): String {
            val total = (ms.coerceAtLeast(0L)) / 1000L
            val h = total / 3600L
            val m = (total % 3600L) / 60L
            val sec = total % 60L
            val ss = sec.toString().padStart(2, '0')
            return if (h > 0L) "$h:${m.toString().padStart(2, '0')}:$ss" else "$m:$ss"
        }

        const val LENGTH = 22
        private const val NO_PART = 0xFF
        private const val UNKNOWN_MS = 0xFFFFFFFFL

        private fun ByteArray.u16(o: Int) = (this[o].toInt() and 0xFF) or ((this[o + 1].toInt() and 0xFF) shl 8)
        private fun ByteArray.u32(o: Int) =
            (this[o].toLong() and 0xFF) or ((this[o + 1].toLong() and 0xFF) shl 8) or
                ((this[o + 2].toLong() and 0xFF) shl 16) or ((this[o + 3].toLong() and 0xFF) shl 24)

        /** Null for a frame of the wrong length or with a state or ramp phase it does not know: no part of it is believed. */
        fun fromWire(data: ByteArray): SessionProgress? {
            if (data.size != LENGTH) return null
            val state = HubSessionState.from(data[0].toInt() and 0xFF) ?: return null
            val ramp = RampPhase.from(data[16].toInt() and 0xFF) ?: return null
            val part = data[1].toInt() and 0xFF
            val rampLeft = data.u32(18)
            return SessionProgress(
                state = state,
                activePart = if (part == NO_PART) null else part,
                activeModalityMask = data.u16(2),
                elapsedMs = data.u32(4),
                remainingMs = data.u32(8),
                totalMs = data.u32(12),
                rampPhase = ramp,
                rampPercent = (data[17].toInt() and 0xFF).coerceAtMost(100),
                rampRemainingMs = if (rampLeft == UNKNOWN_MS) null else rampLeft,
            )
        }
    }
}
