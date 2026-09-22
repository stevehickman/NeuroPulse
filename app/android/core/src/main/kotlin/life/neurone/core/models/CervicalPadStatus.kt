package life.neurone.core.models

// Port of iOS CervicalPadStatus (app/ios/NeurOne/Models/CervicalPadStatus.swift).
//
// Cervical VNS gel pad contact result — the hub's CVNS_PAD_STATUS characteristic.
//
// The pad is replaced when it FAILS, not after a count (docs/reference/commercial-model.md §2.3,
// OI-ACC-03): it is guaranteed for one use and may be reused while it still makes contact.
// Failure is detected on the hub — the pre-enable per-electrode impedance check
// (NP-HW-CVNS-001 REQ-CVNS-06, ≤ 5.0 kΩ at 1 kHz, on which the safety MCU refuses enable) or
// mid-session open detection (REQ-CVNS-08, 500 ms). This type only WORDS that refusal for the
// wearer, naming WHERE the failing pad is (OI-ACC-07). It gates nothing: the safety MCU already
// has, and no app path may stand in for it (CLAUDE.md §4.2).
//
// UHDR-class — derived from tissue impedance (docs/reference/data-architecture-detail.md §5.1).
// Held in memory for display only: never persisted, never uploaded, cleared on disconnect.
//
// Wire (4 bytes):
//   byte 0 — failed-electrode bitmask: bit 0 = electrode 1, bit 1 = electrode 2; bits 2–7
//            reserved, must be 0. 0x00 = both pads passed, which clears any alert.
//   byte 1 — which check: 0x00 = pre-enable (REQ-CVNS-06), 0x01 = mid-session (REQ-CVNS-08).
//   byte 2 — where electrode 1's pad is: 0x01 = left side of the neck, 0x02 = right side.
//   byte 3 — where electrode 2's pad is, same codes.
// The HUB supplies the location, from the session's montage (np_cvns_session_config_t
// electrode_config: bilateral puts one pad on each side, unilateral puts both on one side) and
// its own electrode-to-side wiring. The app never maps an electrode number to a side itself.
// The hub notifies when the result changes within a session attempt and at each new attempt.
data class CervicalPadStatus(
    /**
     * Where each failing pad is, one entry per failing electrode, in electrode order.
     * Empty = both passed. Two entries may name the same side (a unilateral montage).
     */
    val failedPadSides: List<NeckSide>,
    val check: Check,
) {
    enum class Check(val rawValue: Int) {
        PRE_ENABLE(0x00),
        MID_SESSION(0x01);

        companion object {
            fun from(rawValue: Int): Check? = entries.firstOrNull { it.rawValue == rawValue }
        }
    }

    enum class NeckSide(val rawValue: Int) {
        LEFT(0x01),
        RIGHT(0x02);

        companion object {
            fun from(rawValue: Int): NeckSide? = entries.firstOrNull { it.rawValue == rawValue }
        }
    }

    /**
     * Which locale key words this status. The app resolves it — `:core` holds no text
     * (CLAUDE.md §17). A single failure names its side — "a gel pad on the left side", which
     * stays true when a unilateral montage puts both pads there.
     */
    enum class Message { PRE_LEFT, PRE_RIGHT, PRE_BOTH, MID_LEFT, MID_RIGHT, MID_BOTH }

    val hasFailure: Boolean get() = failedPadSides.isNotEmpty()

    /** The sides the neck diagram lights. */
    val failingSides: Set<NeckSide> get() = failedPadSides.toSet()

    /** null when both pads passed. */
    val message: Message?
        get() {
            val first = failedPadSides.firstOrNull() ?: return null
            val both = failedPadSides.size >= PAD_COUNT
            return when (check) {
                Check.PRE_ENABLE -> when {
                    both -> Message.PRE_BOTH
                    first == NeckSide.LEFT -> Message.PRE_LEFT
                    else -> Message.PRE_RIGHT
                }
                Check.MID_SESSION -> when {
                    both -> Message.MID_BOTH
                    first == NeckSide.LEFT -> Message.MID_LEFT
                    else -> Message.MID_RIGHT
                }
            }
        }

    companion object {
        /** NP_CVNS_ELECTRODE_COUNT — the assembly checks two pads every session. */
        const val PAD_COUNT = 2

        /**
         * Decodes the 4-byte frame. Returns null for anything not exactly well-formed — a
         * reserved bit, an unknown check or an unknown location — so a malformed frame is
         * discarded, never half-believed. A location the app cannot name is malformed, not
         * "somewhere".
         */
        fun fromWire(data: ByteArray): CervicalPadStatus? {
            if (data.size < 4) return null
            val mask = data[0].toInt() and 0xFF
            if (mask and ((1 shl PAD_COUNT) - 1).inv() and 0xFF != 0) return null
            val check = Check.from(data[1].toInt() and 0xFF) ?: return null
            val side1 = NeckSide.from(data[2].toInt() and 0xFF) ?: return null
            val side2 = NeckSide.from(data[3].toInt() and 0xFF) ?: return null
            val sides = listOf(side1, side2)
            val failed = (0 until PAD_COUNT).filter { mask and (1 shl it) != 0 }.map { sides[it] }
            return CervicalPadStatus(failed, check)
        }
    }
}
