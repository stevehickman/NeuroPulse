package life.neurone.shared.ui

// Why a socket is in the state it is in (NP-SIM-FW-001 §25, REQ-SIMFW-18#d, the Principal's ruling of 2026-11-10: show the
// reason beside each state). A reason is a closed set of typed values and never a string a firmware event carries: the
// twin or the hub supplies a status code, a status byte or a fault slot, these functions turn it into one of the values
// below, and the view says it in the user's language through the locale key each value names. A cause the firmware does not
// report is UNREPORTED and is shown as that. No reason is ever inferred: where a state has several causes that the hub
// reports as one code, the reason is that one code (`HubStatusReason.DRIVER_INTERLOCK`), and the table in §25 says so.

/** A reason for a socket state. [key] is the canonical locale key (a key of every locale file); the view maps each value to its resource. */
sealed interface SocketReason {
    val key: String
}

/**
 * The status the hub reported for a command (`np_hub_status_t`, `np_hub_types.h`), for the states REFUSED, WRITE_FAILED and FAULT.
 * [code] is the value in the header; the table in §25 gives the firmware that returns each.
 */
enum class HubStatusReason(val code: Int, override val key: String) : SocketReason {
    /** `NP_HUB_ERR_POWER_BUDGET`: the power governor admitted none of the load (`np_socket_dispatch.c`, `np_pbm_power_gov.c`). */
    POWER_BUDGET(-18, "HELMET_REASON_HUB_POWER_BUDGET"),

    /**
     * `NP_HUB_ERR_SAFETY_REJECTED`: a module driver's own safety check refused the command. The goggles' drivers return it for a halted
     * photic detector, an unarmed or open Hall sensor, a goggle not seated, a closed eye in a retinal mode and the MPE check; the probe's
     * for an unauthenticated sleeve; the auricular VNS driver's for a contact impedance that is not finite or outside the window
     * (`np_mod_vns.c`, `REQ-VNSC-07`). The hub returns the one code, so the
     * cause among them is not reported (`OI-SIMFW-47`).
     */
    DRIVER_INTERLOCK(-10, "HELMET_REASON_HUB_DRIVER_INTERLOCK"),

    /** `NP_HUB_ERR_INVALID_ARG`: the command's parameters were too short or out of range. */
    INVALID_ARGUMENT(-2, "HELMET_REASON_HUB_INVALID_ARGUMENT"),

    /** `NP_HUB_ERR_NOT_PRESENT`: the module was not detected. */
    NOT_PRESENT(-3, "HELMET_REASON_HUB_NOT_PRESENT"),

    /** `NP_HUB_ERR_MOD_FAULT`: the module's driver reported a fault (a failed write, a stop that did not land). */
    MODULE_FAULT(-17, "HELMET_REASON_HUB_MODULE_FAULT"),

    /** `NP_HUB_ERR_MOD_INIT`: the module did not initialise. */
    MODULE_INIT(-16, "HELMET_REASON_HUB_MODULE_INIT"),

    /** `NP_HUB_ERR_TIMEOUT`: the link to the safety MCU or the module timed out. */
    TIMEOUT(-12, "HELMET_REASON_HUB_TIMEOUT"),

    /** `NP_HUB_ERR_SAFETY_FAULT`: the safety MCU reported a fault. */
    SAFETY_FAULT(-11, "HELMET_REASON_HUB_SAFETY_FAULT"),

    /** `NP_HUB_ERR_SESSION_ACTIVE`: a session already holds the hub. */
    SESSION_ACTIVE(-13, "HELMET_REASON_HUB_SESSION_ACTIVE"),

    /** A code this table does not name. It is shown as not reported, never guessed at. */
    UNREPORTED(0, "HELMET_REASON_HUB_UNREPORTED"),
    ;

    companion object {
        /** The reason for a hub status code; a code the table does not name is [UNREPORTED]. */
        fun fromCode(code: Int): HubStatusReason = entries.firstOrNull { it != UNREPORTED && it.code == code } ?: UNREPORTED
    }
}

/** Why the safety MCU's line for a commanded module was never enabled (state WITHHELD). */
enum class WithheldReason(override val key: String) : SocketReason {
    /** `NP_SAFETY_STATUS_SIG_PENDING`: the MCU is awaiting the session signature. */
    AWAITING_SESSION_SIGNATURE("HELMET_REASON_WITHHELD_SIGNATURE"),

    /** `NP_SAFETY_STATUS_IMPEDANCE`: a contact check failed, so the line is withheld and not cut. */
    CONTACT_CHECK_FAILED("HELMET_REASON_WITHHELD_CONTACT"),

    /**
     * No status flag says why. An undeclared electrical waveform class, the tier gate and a line the hub has not requested are all
     * withheld with status OK and no fault slot (§23), so they cannot be told apart from what the MCU reports (`OI-SIMFW-47`).
     */
    UNREPORTED("HELMET_REASON_WITHHELD_UNREPORTED"),
}

/** Why the safety MCU dropped a line it had enabled while the hub still requested it (state CUT_OFF). */
enum class CutoffReason(override val key: String) : SocketReason {
    /** The independent watchdog reset the MCU (`safety_iwdg_reset`); every line the hub still requests is cut. */
    INDEPENDENT_WATCHDOG("HELMET_REASON_CUTOFF_INDEPENDENT_WATCHDOG"),

    /** `NP_SAFETY_STATUS_FAULT` with fault slot 0xFD: the session signature failed. */
    SESSION_SIGNATURE("HELMET_REASON_CUTOFF_SESSION_SIGNATURE"),

    /** `NP_SAFETY_STATUS_FAULT` with another fault slot (an unprovisioned OTP, a root key that fails its CRC, a frozen SysTick, a flash that cannot record a cutoff). */
    MCU_FAULT("HELMET_REASON_CUTOFF_MCU_FAULT"),

    /** `NP_SAFETY_STATUS_THERMAL`: the thermal interlock. */
    THERMAL("HELMET_REASON_CUTOFF_THERMAL"),

    /** `NP_SAFETY_STATUS_CHARGE`: the charge ceiling. */
    CHARGE("HELMET_REASON_CUTOFF_CHARGE"),

    /** `NP_SAFETY_STATUS_CARDIAC`: the cardiac interlock. */
    CARDIAC("HELMET_REASON_CUTOFF_CARDIAC"),

    /** `NP_SAFETY_STATUS_WATCHDOG`: the hub's heartbeat stopped and the MCU's watchdog fired (about 1500 ms). */
    HEARTBEAT_LOST("HELMET_REASON_CUTOFF_HEARTBEAT"),

    /** `NP_SAFETY_STATUS_CUTOFF` and no other flag: the MCU says it cut and says no more. */
    CUTOFF_ONLY("HELMET_REASON_CUTOFF_ONLY"),

    /** The line dropped and no reply has carried a status flag yet (the reply travels with the next heartbeat). */
    UNREPORTED("HELMET_REASON_CUTOFF_UNREPORTED"),
}

/** `NP_SAFETY_STATUS_*` (`firmware/common/include/np_spi_wire_types.h`, lines 195 to 206). */
object SafetyStatusBits {
    const val FAULT = 0x01
    const val WATCHDOG = 0x02
    const val CUTOFF = 0x04
    const val IMPEDANCE = 0x08
    const val THERMAL = 0x10
    const val CHARGE = 0x20
    const val CARDIAC = 0x40
    const val SIG_PENDING = 0x80

    /** The fault slot of a bad session signature (the simulator's scenarios name it, NP-SIM-FW-001 §22; `safety.js` `SLOT_NAMES`). */
    const val SLOT_BAD_SESSION_SIGNATURE = 0xFD
}

/**
 * The reason for a WITHHELD socket from the MCU's last status byte. Order, first match wins: the session signature, then the
 * contact check. Both can be set at once; the Safety MCU panel shows every flag.
 */
fun withheldReasonOf(status: Int): WithheldReason = when {
    status and SafetyStatusBits.SIG_PENDING != 0 -> WithheldReason.AWAITING_SESSION_SIGNATURE
    status and SafetyStatusBits.IMPEDANCE != 0 -> WithheldReason.CONTACT_CHECK_FAILED
    else -> WithheldReason.UNREPORTED
}

/**
 * The reason for a CUT_OFF socket from the MCU's last status byte and fault slot, and whether the independent watchdog reset it.
 * Several causes can be set at once (a bad signature sets FAULT and CUTOFF together); the first that matches in this order is
 * shown, and the Safety MCU panel shows every flag: the independent watchdog, a fault (the signature, then any other), the
 * thermal, charge and cardiac interlocks, the heartbeat watchdog, a bare cutoff, and last a cut no reply has explained.
 */
fun cutoffReasonOf(status: Int, faultSlot: Int?, independentWatchdogReset: Boolean): CutoffReason = when {
    independentWatchdogReset -> CutoffReason.INDEPENDENT_WATCHDOG
    status and SafetyStatusBits.FAULT != 0 ->
        if (faultSlot == SafetyStatusBits.SLOT_BAD_SESSION_SIGNATURE) CutoffReason.SESSION_SIGNATURE else CutoffReason.MCU_FAULT
    status and SafetyStatusBits.THERMAL != 0 -> CutoffReason.THERMAL
    status and SafetyStatusBits.CHARGE != 0 -> CutoffReason.CHARGE
    status and SafetyStatusBits.CARDIAC != 0 -> CutoffReason.CARDIAC
    status and SafetyStatusBits.WATCHDOG != 0 -> CutoffReason.HEARTBEAT_LOST
    status and SafetyStatusBits.CUTOFF != 0 -> CutoffReason.CUTOFF_ONLY
    else -> CutoffReason.UNREPORTED
}

/** Every reason a state can show, for the tests and for the view's exhaustive mapping to resources. */
val ALL_SOCKET_REASONS: List<SocketReason> = HubStatusReason.entries + WithheldReason.entries + CutoffReason.entries

/** One line of the view's reasons list: [count] sockets are in [state] for [reason]. */
data class HelmetReasonRow(val state: SocketDrawState, val reason: SocketReason, val count: Int)

/**
 * The reasons list: every (state, reason) pair among the sockets, with how many sockets share it, in the order of the states and
 * then of the reasons. The lattice's tiles are too small to carry a sentence, so the reason sits beside the state's own tile in
 * this list and not on the tile.
 */
fun helmetReasonRows(state: HelmetState): List<HelmetReasonRow> =
    state.sockets.keys
        .mapNotNull { id -> state.reason(id)?.let { state.drawState(id) to it } }
        .groupingBy { it }
        .eachCount()
        .map { (pair, n) -> HelmetReasonRow(pair.first, pair.second, n) }
        .sortedWith(compareBy({ it.state.ordinal }, { it.reason.key }))
