package life.neurone.core.models

import java.util.UUID

// Port of iOS CervicalFaultStatus (app/ios/NeurOne/Models/CervicalFaultStatus.swift).
//
// Cervical VNS faults that happened while the app was not connected, and the hub's re-enable
// state — the hub's CVNS_FAULT_STATUS characteristic (NP-SW-FAULTMSG-001 P3/P4).
//
// In Mode 3 the only fault signal is the red status LED (CLAUDE.md §4.7). When the phone next
// connects, this is what tells the wearer which fault it was and what to do — before the app
// offers another cervical session. It gates only what the app offers: the safety MCU owns the
// enable line and keeps a cardiac cutoff latched across power cycles on its own (P1, CLAUDE.md
// §4.2), so nothing here can release stimulation.
//
// UHDR-class — fault kinds from the user's own session records (docs/reference/
// data-architecture-detail.md §5.1). No heart-rate values and no timestamps cross: a device
// session counter orders the records. Held in memory; only the acknowledgement point
// (CervicalFaultLedger) is persisted, on the phone.
//
// Wire (4 + 8n bytes, n = 0…4):
//   byte 0 — version, 0x01
//   byte 1 — hub re-enable state (np_cvns_reenable_state_t): 0 idle, 1 lockout, 2 awaiting
//            confirmation, 3 impedance check, 4 re-enabling
//   byte 2 — n, the number of records that follow (newest last)
//   byte 3 — flags (from the safety MCU's np_safety_nv_report_t): bit 0 = the ACTIVE user's
//            cervical VNS is withheld; bit 1 = SOMEONE on this device has an outstanding
//            cardiac cutoff (the blanket warning); bits 2–7 reserved, 0. Which user is never sent.
//   per record: uint32 LE device session counter · uint8 fault kind (np_cvns_fault_reason_t,
//               1…5) · uint8 failing-pad side mask (bit 0 left, bit 1 right; pad faults only)
//               · uint16 reserved, 0
// The records are the ACTIVE user's own (per-user cardiac scope, principal 2026-09-22).
// Anything else — wrong version or length, a reserved bit, an unknown kind or state — is
// discarded whole, never half-believed.
data class CervicalFaultRecord(
    val sessionCounter: Long,
    val kind: Kind,
    /** Where the failing pad is, for a pad-contact fault; empty otherwise. */
    val padSides: Set<CervicalPadStatus.NeckSide> = emptySet(),
) {
    enum class Kind(val rawValue: Int) {
        HEART_RATE_CHANGE(1),   // NP_CVNS_FAULT_HR_CHANGE — the cardiac cutoff
        SIGNAL_LOST(2),         // NP_CVNS_FAULT_DATA_LOSS — ear-clip R-peak signal lost
        WATCHDOG(3),            // NP_CVNS_FAULT_WATCHDOG
        PAD_CONTACT(4),         // NP_CVNS_FAULT_IMPEDANCE
        SAFETY_MCU(5);          // NP_CVNS_FAULT_SAFETY_MCU

        companion object {
            fun from(rawValue: Int): Kind? = entries.firstOrNull { it.rawValue == rawValue }
        }
    }

    /** Which message explains this fault. The app resolves it — `:core` holds no text (§17). */
    enum class Message { HR_CHANGE, SIGNAL_LOST, DEVICE, PAD_LEFT, PAD_RIGHT, PAD_BOTH }

    val message: Message
        get() = when (kind) {
            Kind.HEART_RATE_CHANGE -> Message.HR_CHANGE
            Kind.SIGNAL_LOST -> Message.SIGNAL_LOST
            Kind.WATCHDOG, Kind.SAFETY_MCU -> Message.DEVICE
            Kind.PAD_CONTACT -> {
                val l = CervicalPadStatus.NeckSide.LEFT in padSides
                val r = CervicalPadStatus.NeckSide.RIGHT in padSides
                when {
                    l && !r -> Message.PAD_LEFT
                    r && !l -> Message.PAD_RIGHT
                    else -> Message.PAD_BOTH
                }
            }
        }
}

data class CervicalFaultStatus(
    val reenableState: ReenableState,
    val records: List<CervicalFaultRecord>,
    /** The active user's cervical VNS is withheld by the safety MCU. */
    val userBlocked: Boolean = false,
    /** Someone on this device — the active user or another — has an outstanding cardiac cutoff. */
    val outstanding: Boolean = false,
) {
    enum class ReenableState(val rawValue: Int) {
        IDLE(0), LOCKOUT(1), AWAIT_CONFIRM(2), IMPEDANCE(3), REENABLING(4);

        companion object {
            fun from(rawValue: Int): ReenableState? = entries.firstOrNull { it.rawValue == rawValue }
        }
    }

    /**
     * Another person has an outstanding cutoff and the active user does not: the case in which
     * switching profiles could otherwise be used to get around a block.
     */
    val outstandingForAnotherUser: Boolean get() = outstanding && !userBlocked

    companion object {
        const val VERSION = 0x01
        const val MAX_RECORDS = 4
        const val HEADER_LENGTH = 4
        const val RECORD_LENGTH = 8

        fun fromWire(data: ByteArray): CervicalFaultStatus? {
            if (data.size < HEADER_LENGTH) return null
            fun u8(i: Int) = data[i].toInt() and 0xFF
            if (u8(0) != VERSION) return null
            val state = ReenableState.from(u8(1)) ?: return null
            val n = u8(2)
            if (n > MAX_RECORDS) return null
            if (u8(3) and 0x03.inv() and 0xFF != 0) return null
            if (data.size != HEADER_LENGTH + n * RECORD_LENGTH) return null

            val records = (0 until n).map { i ->
                val o = HEADER_LENGTH + i * RECORD_LENGTH
                val counter = u8(o).toLong() or (u8(o + 1).toLong() shl 8) or
                    (u8(o + 2).toLong() shl 16) or (u8(o + 3).toLong() shl 24)
                val kind = CervicalFaultRecord.Kind.from(u8(o + 4)) ?: return null
                val mask = u8(o + 5)
                if (mask and 0x03.inv() != 0 || u8(o + 6) != 0 || u8(o + 7) != 0) return null
                val sides = buildSet {
                    if (mask and 0x01 != 0) add(CervicalPadStatus.NeckSide.LEFT)
                    if (mask and 0x02 != 0) add(CervicalPadStatus.NeckSide.RIGHT)
                }
                // A side mask belongs to a pad fault only; on any other kind it is malformed.
                if (kind != CervicalFaultRecord.Kind.PAD_CONTACT && sides.isNotEmpty()) return null
                CervicalFaultRecord(counter, kind, sides)
            }
            return CervicalFaultStatus(
                reenableState = state,
                records = records,
                userBlocked = u8(3) and 0x01 != 0,
                outstanding = u8(3) and 0x02 != 0,
            )
        }
    }
}

/**
 * The opaque tag that tells the device which person is using it (per-user cardiac scope,
 * principal 2026-09-22). Derived from the app's individual profile id — a random UUID — so it
 * carries no name or identity. The device's two reserved values are never produced. Same
 * derivation as iOS ActiveUserTag: the UUID's first four bytes, little-endian.
 */
object ActiveUserTag {
    fun from(profileId: String?): Long? {
        val id = profileId?.let { runCatching { UUID.fromString(it) }.getOrNull() } ?: return null
        val msb = id.mostSignificantBits
        var tag = ((msb ushr 56) and 0xFF) or (((msb ushr 48) and 0xFF) shl 8) or
            (((msb ushr 40) and 0xFF) shl 16) or (((msb ushr 32) and 0xFF) shl 24)
        if (tag == 0L) tag = 1L
        if (tag == 0xFFFF_FFFFL) tag = 0xFFFF_FFFEL
        return tag
    }

    fun toWire(tag: Long): ByteArray = ByteArray(4) { i -> ((tag shr (8 * i)) and 0xFF).toByte() }
}

/**
 * Which offline faults the wearer has already read. Persists one number — the device session
 * counter of the newest acknowledged fault — so a fault is explained once, not on every connect.
 *
 * A counter LOWER than the stored one means a different or reset device; the ledger then treats
 * every record as unread rather than hiding faults behind a stale number.
 */
data class CervicalFaultLedger(val lastAcknowledgedSession: Long? = null) {

    fun unacknowledged(records: List<CervicalFaultRecord>): List<CervicalFaultRecord> {
        val last = lastAcknowledgedSession ?: return records
        val newest = records.maxOfOrNull { it.sessionCounter }
        if (newest != null && newest < last) return records
        return records.filter { it.sessionCounter > last }
    }

    /**
     * True while an unread cardiac cutoff exists: the app then refuses to upload a protocol
     * containing cervical VNS until the wearer has read why stimulation stopped.
     */
    fun blocksCervicalRestart(records: List<CervicalFaultRecord>): Boolean =
        unacknowledged(records).any { it.kind == CervicalFaultRecord.Kind.HEART_RATE_CHANGE }

    fun acknowledge(records: List<CervicalFaultRecord>): CervicalFaultLedger {
        val newest = records.maxOfOrNull { it.sessionCounter } ?: return this
        return CervicalFaultLedger(newest)
    }
}
