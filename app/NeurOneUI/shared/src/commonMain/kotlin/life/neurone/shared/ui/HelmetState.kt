package life.neurone.shared.ui

import life.neurone.core.protocol.SocketLattice

// The helmet view's input (NP-SIM-FW-001 §25, OI-SIMFW-37): what to draw for each socket, decided by one rule that does not
// depend on where the facts came from. The simulator feeds it from the firmware twin's module state and the safety MCU's
// `safety_state`; the app controlling real hardware feeds it from the hub's own reports. Nothing here reads a platform.

/** What the safety MCU says about the enable line that gates a socket's module. */
enum class LineVerdict {
    /** No safety MCU is in the loop (the simulator before an attach), or the source cannot see its enable line. */
    NOT_IN_LOOP,
    /** The MCU is in the loop and the module's driver requests no enable (audio, the EEG front end): there is no line to ask. */
    UNGATED,
    /** The MCU's line for the module is enabled. The line is the authority, not the last reply's granted mask. */
    ENABLED,
    /** The MCU is in the loop and its line for the module is not enabled. */
    DISABLED,
}

/** What became of the hub's last command to a socket, where it did not simply take effect. */
enum class CommandOutcome {
    /** Nothing to report: no command, or one the module took. */
    NONE,
    /** The hub refused the command before sending anything (the power governor's -18, a refused descriptor): nothing was commanded. */
    REFUSED,
    /** The hub sent the command and the write to the module failed: the module did not take it. */
    WRITE_FAILED,
}

/**
 * What the view draws in a socket. Each is a different fact about what is happening at the socket, with its own evidence
 * (NP-SIM-FW-001 §25, the state table), and the view gives each its own shape as well as its own colour (`HelmetStyle.kt`).
 */
enum class SocketDrawState {
    /** No module in the socket. */
    ABSENT,
    /** A module is fitted and not commanded. */
    IDLE,
    /** The hub refused to send the command; nothing was commanded. */
    REFUSED,
    /** The hub sent the command and the write to the module failed: commanded, and not executed. */
    WRITE_FAILED,
    /** The hub commanded it and no safety MCU is in the loop to say whether the line is enabled: never drawn as driven. */
    COMMANDED,
    /** The hub commanded it and the safety MCU's line for it is enabled. */
    DRIVEN,
    /** The hub commanded it, a safety MCU is in the loop, and the module has no enable line: it runs on the hub's command alone. */
    DRIVEN_UNGATED,
    /** The hub commanded it and the safety MCU's line was never enabled for it. */
    WITHHELD,
    /** The line was enabled and the safety MCU dropped it while the hub still requests it: a cutoff. */
    CUT_OFF,
    /** The module reported a fault of its own. */
    FAULT,
}

/** The facts about one socket. */
data class HelmetSocket(
    val id: Int,
    val fitted: Boolean,
    val commanded: Boolean = false,
    val line: LineVerdict = LineVerdict.NOT_IN_LOOP,
    val fault: Boolean = false,
    /** The safety MCU has recorded a cutoff of this socket's line; it counts only while the line is not enabled. */
    val cutoff: Boolean = false,
    val outcome: CommandOutcome = CommandOutcome.NONE,
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
 * asserted (or the module has no enable line and a safety MCU is there to say so). Commanded is never driven: a refusal or a
 * failed write means the command was not executed, and the safety MCU may have withheld or cut the line. Precedence, highest
 * first: absent, the module's own fault, a command that did not execute, no command, then what the safety MCU says.
 */
fun drawStateOf(s: HelmetSocket): SocketDrawState = when {
    !s.fitted -> SocketDrawState.ABSENT
    s.fault -> SocketDrawState.FAULT
    s.outcome == CommandOutcome.WRITE_FAILED -> SocketDrawState.WRITE_FAILED
    s.outcome == CommandOutcome.REFUSED -> SocketDrawState.REFUSED
    !s.commanded -> SocketDrawState.IDLE
    s.line == LineVerdict.ENABLED -> SocketDrawState.DRIVEN
    s.line == LineVerdict.UNGATED -> SocketDrawState.DRIVEN_UNGATED
    s.line == LineVerdict.DISABLED -> if (s.cutoff) SocketDrawState.CUT_OFF else SocketDrawState.WITHHELD
    else -> SocketDrawState.COMMANDED
}
