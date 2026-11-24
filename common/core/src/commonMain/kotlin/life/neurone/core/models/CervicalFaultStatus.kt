package life.neurone.core.models

import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.longOrNull
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore
import life.neurone.core.protocol.CvfsWire
import life.neurone.core.protocol.ProfileTagState

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
//   byte 0 — version, 0x02 (CvfsWire.WIRE_VERSION)
//   byte 1 — hub re-enable state (np_cvns_reenable_state_t): 0 idle, 1 lockout, 2 awaiting
//            confirmation, 3 impedance check, 4 re-enabling
//   byte 2 — n, the number of records that follow (newest last)
//   byte 3 — flags (from the safety MCU's np_safety_nv_report_t): bit 0 = the ACTIVE user's
//            cervical VNS is withheld; bit 1 = SOMEONE on this device has an outstanding
//            cardiac cutoff (the blanket warning); bit 7 (CvfsWire.FLAG_REPORT_VALID) = bits 0–1
//            come from a safety-MCU report the hub validated; bits 2–6 reserved, 0. Which user is
//            never sent. With bit 7 clear, bits 0–1 are unknown (and must be 0), never "nothing
//            outstanding" (OI-PRIV-29, NP-PRIV-ANALYSIS-004).
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
    /**
     * The two flags above come from a safety-MCU report the hub validated. False: they are unknown, so a false
     * [outstanding] does not mean nothing is outstanding. Only a valid "nothing outstanding" may move a person to a new
     * active-user tag (`REQ-FAULTMSG-01` (c)).
     */
    val reportValid: Boolean = false,
) {
    /** The helmet has said, from a valid report, that nobody on it has an outstanding cutoff. */
    val knownNothingOutstanding: Boolean get() = reportValid && !outstanding

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
        fun fromWire(data: ByteArray): CervicalFaultStatus? {
            if (data.size < CvfsWire.HEADER_LEN) return null
            fun u8(i: Int) = data[i].toInt() and 0xFF
            if (u8(0) != CvfsWire.WIRE_VERSION) return null
            val state = ReenableState.from(u8(1)) ?: return null
            val n = u8(2)
            if (n > CvfsWire.MAX_WIRE_RECORDS) return null
            val flags = u8(3)
            if (flags and (0x03 or CvfsWire.FLAG_REPORT_VALID).inv() and 0xFF != 0) return null
            // Without a valid report the hub sends 0 in bits 0-1; anything else is not the hub's answer.
            if (flags and CvfsWire.FLAG_REPORT_VALID == 0 && flags and 0x03 != 0) return null
            if (data.size != CvfsWire.HEADER_LEN + n * CvfsWire.RECORD_LEN) return null

            val records = (0 until n).map { i ->
                val o = CvfsWire.HEADER_LEN + i * CvfsWire.RECORD_LEN
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
                userBlocked = flags and 0x01 != 0,
                outstanding = flags and 0x02 != 0,
                reportValid = flags and CvfsWire.FLAG_REPORT_VALID != 0,
            )
        }
    }
}

/**
 * The opaque tag that tells a helmet which person is using it (per-user cardiac scope, principal 2026-09-22;
 * `REQ-FAULTMSG-01`, `OI-PRIV-29` (NP-PRIV-ANALYSIS-004)).
 *
 * The tag is HMAC-SHA-256 of the helmet's serial under the profile's own secret ([ProfileTagState.secret]), so a person has
 * a different tag on each helmet and no tag can be computed from the profile id. The derivation, the old tag and the rule
 * for leaving it are the shared Rust core's (`common/npps-core/src/active_user_tag.rs`, CLAUDE.md §22); this calls them.
 *
 * A profile made before the per-helmet tag ([ProfileTagState.legacy]) may be known to a helmet by its old tag (the profile
 * id's first four bytes), and a cardiac cutoff may be held under it. Naming the new tag there would release that hold. So
 * the old tag is named until the helmet reports, from a valid safety-MCU report, that nobody on it has an outstanding
 * cutoff ([CervicalFaultStatus.knownNothingOutstanding]); then the move is recorded and the new tag named, for good.
 */
object ActiveUserTag {
    /** What to name to this helmet: [tag], and [moveTo] when this choice is the move to the per-helmet tag, to record first. */
    data class Choice(val tag: Long, val moveTo: Long? = null)

    /** The per-helmet tag, or null if the core refuses (the choice is then to name nobody, and the helmet keeps who it knew). */
    fun derived(state: ProfileTagState, helmetSerial: ByteArray): Long? {
        if (!state.isWellFormed || helmetSerial.isEmpty()) return null
        val secret = ByteArray(32) { state.secret.substring(2 * it, 2 * it + 2).toInt(16).toByte() }
        return try {
            NppsCore.migrationSecret(
                buildJsonObject {
                    put("op", "activeUserTag")
                    put("profileSecret", NppsCore.secretRef(0))
                    put("helmetSerial", helmetSerial.joinToString("") { ((it.toInt() and 0xFF) + 0x100).toString(16).substring(1) })
                },
                listOf(secret),
            ).answer["tag"]?.jsonPrimitive?.longOrNull
        } catch (_: RuntimeException) {
            null
        } finally {
            secret.fill(0)
        }
    }

    /** The old tag (the profile id's first four bytes), for a legacy profile only. */
    fun legacy(profileId: String): Long? = try {
        NppsCore.migration(buildJsonObject { put("op", "activeUserTagLegacy"); put("profileId", profileId.lowercase()) })["tag"]
            ?.jsonPrimitive?.longOrNull
    } catch (_: RuntimeException) {
        null
    }

    /**
     * Which tag to name to the helmet with [helmetSerial], for the profile [profileId] in [state], given the helmet's latest
     * fault status ([status], null before one is read). Null when no tag can be named: nothing is sent, and the helmet keeps
     * assuming whoever it last knew.
     */
    fun choose(profileId: String, state: ProfileTagState, helmetSerial: ByteArray, status: CervicalFaultStatus?): Choice? {
        val derived = derived(state, helmetSerial) ?: return null
        if (!state.legacy || derived in state.migrated) return Choice(derived)
        val nothingOutstanding: Boolean? = status?.takeIf { it.reportValid }?.let { !it.outstanding }
        val step = try {
            NppsCore.migration(buildJsonObject {
                put("op", "activeUserTagStep")
                put("migrated", false)
                put("nothingOutstanding", nothingOutstanding?.let(::JsonPrimitive) ?: JsonNull)
            })["step"]?.jsonPrimitive?.contentOrNull
        } catch (_: RuntimeException) {
            null
        }
        return if (step == "derived") Choice(derived, moveTo = derived) else legacy(profileId)?.let { Choice(it) }
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
