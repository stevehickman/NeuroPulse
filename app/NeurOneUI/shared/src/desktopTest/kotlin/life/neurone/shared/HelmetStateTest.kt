package life.neurone.shared

import life.neurone.core.protocol.SocketLattice
import life.neurone.shared.ui.HelmetSocket
import life.neurone.shared.ui.HelmetState
import life.neurone.shared.ui.LineVerdict
import life.neurone.shared.ui.SocketDrawState
import life.neurone.shared.ui.drawStateOf
import kotlin.test.Test
import kotlin.test.assertEquals

/** The helmet view's drawing rule (REQ-SIMFW-07): driven only when commanded and the safety MCU's line is enabled. */
class HelmetStateTest {
    private fun s(fitted: Boolean = true, commanded: Boolean = false, line: LineVerdict = LineVerdict.NOT_IN_LOOP, fault: Boolean = false) =
        HelmetSocket(1, fitted, commanded, line, fault)

    // @verifies REQ-SIMFW-07
    @Test
    fun drivenOnlyWhenCommandedAndTheLineIsEnabled() {
        assertEquals(SocketDrawState.DRIVEN, drawStateOf(s(commanded = true, line = LineVerdict.ENABLED)))
        assertEquals(SocketDrawState.WITHHELD, drawStateOf(s(commanded = true, line = LineVerdict.DISABLED)))
        assertEquals(SocketDrawState.COMMANDED, drawStateOf(s(commanded = true, line = LineVerdict.NOT_IN_LOOP)))
    }

    // @verifies REQ-SIMFW-07
    @Test
    fun anEnabledLineWithNoCommandIsNeverDriven() {
        assertEquals(SocketDrawState.IDLE, drawStateOf(s(commanded = false, line = LineVerdict.ENABLED)))
    }

    // @verifies REQ-SIMFW-07
    @Test
    fun noSafetyMcuInTheLoopNeverDrawsDriven() {
        for (commanded in listOf(true, false)) {
            assertEquals(false, drawStateOf(s(commanded = commanded, line = LineVerdict.NOT_IN_LOOP)) == SocketDrawState.DRIVEN)
        }
    }

    // @verifies REQ-SIMFW-07
    @Test
    fun faultOutranksAFittedModuleAndAnAbsentOneIsNeverDrawn() {
        assertEquals(SocketDrawState.FAULT, drawStateOf(s(commanded = true, line = LineVerdict.ENABLED, fault = true)))
        assertEquals(SocketDrawState.ABSENT, drawStateOf(s(fitted = false, commanded = true, line = LineVerdict.ENABLED, fault = true)))
    }

    // @verifies REQ-SIMFW-07
    @Test
    fun aSocketTheStateDoesNotNameIsAbsentAndTheEmptyStateCoversTheLattice() {
        val st = HelmetState.empty()
        assertEquals(SocketLattice.NP_SOCKET_COUNT, st.sockets.size)
        assertEquals(SocketDrawState.ABSENT, st.drawState(SocketLattice.NP_SOCKET_ID_MAX + 1))
    }
}
