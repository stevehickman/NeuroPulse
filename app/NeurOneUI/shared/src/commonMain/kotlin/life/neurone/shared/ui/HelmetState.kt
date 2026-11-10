package life.neurone.shared.ui

import life.neurone.core.protocol.SocketLattice

// The helmet view's input (NP-SIM-FW-001 §25, OI-SIMFW-37): what to draw for each socket, decided by one rule that does not
// depend on where the facts came from. The simulator feeds it from the firmware twin's module state and the safety MCU's
// `safety_state`; the app controlling real hardware feeds it from the hub's own reports. Nothing here reads a platform.

/** What the safety MCU says about the enable line that gates a socket's module. */
enum class LineVerdict {
    /** No safety MCU is in the loop (the simulator before an attach), or the source cannot see its enable line. */
    NOT_IN_LOOP,
    /** The MCU's line for the module is enabled. The line is the authority, not the last reply's granted mask. */
    ENABLED,
    /** The MCU is in the loop and its line for the module is not enabled. */
    DISABLED,
}

/** What the view draws in a socket. */
enum class SocketDrawState {
    /** No module in the socket. */
    ABSENT,
    /** A module is fitted and not commanded. */
    IDLE,
    /** The hub commanded it and no safety MCU is in the loop to say whether the line is enabled: never drawn as driven. */
    COMMANDED,
    /** The hub commanded it and the safety MCU's line for it is enabled. */
    DRIVEN,
    /** The hub commanded it and the safety MCU's line is not enabled: the MCU cut or never granted it. */
    WITHHELD,
    /** The module reported a fault. */
    FAULT,
}

/** The facts about one socket. */
data class HelmetSocket(
    val id: Int,
    val fitted: Boolean,
    val commanded: Boolean = false,
    val line: LineVerdict = LineVerdict.NOT_IN_LOOP,
    val fault: Boolean = false,
)

/**
 * What the helmet view needs. [safetyInLoop] says whether the source has a safety MCU to ask; the view says so on screen
 * when it does not, so a drawing is never read as more than it is (`REQ-SIMFW-07`).
 */
data class HelmetState(val sockets: Map<Int, HelmetSocket>, val safetyInLoop: Boolean) {
    fun drawState(id: Int): SocketDrawState = sockets[id]?.let(::drawStateOf) ?: SocketDrawState.ABSENT

    companion object {
        /** Every socket of the lattice, none fitted. */
        fun empty(safetyInLoop: Boolean = false) = HelmetState(
            SocketLattice.cells.associate { it.id to HelmetSocket(it.id, fitted = false) }, safetyInLoop,
        )
    }
}

/**
 * The rule of `REQ-SIMFW-07`: a module is drawn as driven only if the hub commanded it and the safety MCU's enable for it is
 * asserted. A fault outranks everything else a fitted module can be; an absent module is never drawn as anything.
 */
fun drawStateOf(s: HelmetSocket): SocketDrawState = when {
    !s.fitted -> SocketDrawState.ABSENT
    s.fault -> SocketDrawState.FAULT
    !s.commanded -> SocketDrawState.IDLE
    s.line == LineVerdict.ENABLED -> SocketDrawState.DRIVEN
    s.line == LineVerdict.DISABLED -> SocketDrawState.WITHHELD
    else -> SocketDrawState.COMMANDED
}
