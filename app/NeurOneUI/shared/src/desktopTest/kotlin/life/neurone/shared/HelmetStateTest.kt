package life.neurone.shared

import life.neurone.core.protocol.SocketLattice
import life.neurone.shared.ui.CommandOutcome
import life.neurone.shared.ui.HELMET_STYLE
import life.neurone.shared.ui.HelmetSocket
import life.neurone.shared.ui.HelmetState
import life.neurone.shared.ui.LineVerdict
import life.neurone.shared.ui.SocketDrawState
import life.neurone.shared.ui.TileMarker
import life.neurone.shared.ui.TileOutline
import life.neurone.shared.ui.drawStateOf
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue

/** The helmet view's drawing rule (REQ-SIMFW-07, REQ-SIMFW-18): driven only when commanded and the line is enabled; every state apart. */
class HelmetStateTest {
    private fun s(
        fitted: Boolean = true,
        commanded: Boolean = false,
        line: LineVerdict = LineVerdict.NOT_IN_LOOP,
        fault: Boolean = false,
        cutoff: Boolean = false,
        outcome: CommandOutcome = CommandOutcome.NONE,
    ) = HelmetSocket(1, fitted, commanded, line, fault, cutoff, outcome)

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
            assertNotEquals(SocketDrawState.DRIVEN, drawStateOf(s(commanded = commanded, line = LineVerdict.NOT_IN_LOOP)))
            assertNotEquals(SocketDrawState.DRIVEN_UNGATED, drawStateOf(s(commanded = commanded, line = LineVerdict.NOT_IN_LOOP)))
        }
    }

    // @verifies REQ-SIMFW-18#a
    @Test
    fun absentIdleAndFaultAreTheirOwnStates() {
        assertEquals(SocketDrawState.ABSENT, drawStateOf(s(fitted = false)))
        assertEquals(SocketDrawState.IDLE, drawStateOf(s()))
        assertEquals(SocketDrawState.FAULT, drawStateOf(s(fault = true)))
    }

    // @verifies REQ-SIMFW-18#a
    @Test
    fun anAbsentModuleIsNeverDrawnAsAnythingElseAndAFaultOutranksTheRest() {
        val everything = s(
            fitted = false, commanded = true, line = LineVerdict.ENABLED, fault = true, cutoff = true,
            outcome = CommandOutcome.WRITE_FAILED,
        )
        assertEquals(SocketDrawState.ABSENT, drawStateOf(everything))
        assertEquals(SocketDrawState.FAULT, drawStateOf(everything.copy(fitted = true)))
    }

    // @verifies REQ-SIMFW-18#a
    @Test
    fun aModuleWithNoEnableLineIsDrivenOnTheHubsCommandOnlyWithAnMcuInTheLoop() {
        assertEquals(SocketDrawState.DRIVEN_UNGATED, drawStateOf(s(commanded = true, line = LineVerdict.UNGATED)))
        assertEquals(SocketDrawState.IDLE, drawStateOf(s(commanded = false, line = LineVerdict.UNGATED)))
        assertNotEquals(SocketDrawState.DRIVEN, drawStateOf(s(commanded = true, line = LineVerdict.UNGATED)))
    }

    // @verifies REQ-SIMFW-18#b
    @Test
    fun aRefusedCommandIsItsOwnStateNotCommandedAndNotDriven() {
        for (line in LineVerdict.entries) {
            assertEquals(SocketDrawState.REFUSED, drawStateOf(s(commanded = false, line = line, outcome = CommandOutcome.REFUSED)), "line $line")
        }
    }

    // @verifies REQ-SIMFW-18#b
    @Test
    fun aFailedWriteIsNotExecutedEvenWithTheLineEnabled() {
        for (line in LineVerdict.entries) {
            assertEquals(SocketDrawState.WRITE_FAILED, drawStateOf(s(commanded = true, line = line, outcome = CommandOutcome.WRITE_FAILED)), "line $line")
        }
    }

    // @verifies REQ-SIMFW-18#b
    @Test
    fun aCutoffIsNotAWithholdingAndOnlyCountsWhileTheLineIsDown() {
        assertEquals(SocketDrawState.CUT_OFF, drawStateOf(s(commanded = true, line = LineVerdict.DISABLED, cutoff = true)))
        assertEquals(SocketDrawState.WITHHELD, drawStateOf(s(commanded = true, line = LineVerdict.DISABLED, cutoff = false)))
        // A cutoff recorded earlier does not hide a line that has since been re-enabled.
        assertEquals(SocketDrawState.DRIVEN, drawStateOf(s(commanded = true, line = LineVerdict.ENABLED, cutoff = true)))
    }

    // @verifies REQ-SIMFW-18#a
    @Test
    fun everyStateIsReachableFromSomeFacts() {
        val reached = setOf(
            drawStateOf(s(fitted = false)),
            drawStateOf(s()),
            drawStateOf(s(outcome = CommandOutcome.REFUSED)),
            drawStateOf(s(commanded = true, outcome = CommandOutcome.WRITE_FAILED)),
            drawStateOf(s(commanded = true)),
            drawStateOf(s(commanded = true, line = LineVerdict.ENABLED)),
            drawStateOf(s(commanded = true, line = LineVerdict.UNGATED)),
            drawStateOf(s(commanded = true, line = LineVerdict.DISABLED)),
            drawStateOf(s(commanded = true, line = LineVerdict.DISABLED, cutoff = true)),
            drawStateOf(s(fault = true)),
        )
        assertEquals(SocketDrawState.entries.toSet(), reached)
    }

    // @verifies REQ-SIMFW-18#c
    @Test
    fun everyStateHasAStyleAndNoTwoStatesDifferByColourAlone() {
        assertEquals(SocketDrawState.entries.toSet(), HELMET_STYLE.keys)
        val shapes = HELMET_STYLE.values.map { it.outline to it.marker }
        assertEquals(shapes.size, shapes.toSet().size, "two states share an outline and a marker")
    }

    // @verifies REQ-SIMFW-18#c
    @Test
    fun commandedAndDrivenDifferInOutlineAndMarkerNotJustColour() {
        val commanded = HELMET_STYLE.getValue(SocketDrawState.COMMANDED)
        val driven = HELMET_STYLE.getValue(SocketDrawState.DRIVEN)
        assertNotEquals(commanded.outline, driven.outline)
        assertNotEquals(commanded.marker, driven.marker)
    }

    // @verifies REQ-SIMFW-18#c
    @Test
    fun theStatesTheMcuAndTheHubSeparateEachCarryTheirOwnMarker() {
        // Withheld, cut off, refused, failed and ungated are the cases a reader could mistake for commanded or driven.
        val markers = listOf(
            SocketDrawState.WITHHELD, SocketDrawState.CUT_OFF, SocketDrawState.REFUSED, SocketDrawState.WRITE_FAILED,
            SocketDrawState.DRIVEN_UNGATED, SocketDrawState.COMMANDED, SocketDrawState.DRIVEN, SocketDrawState.FAULT,
        ).map { HELMET_STYLE.getValue(it).marker }
        assertEquals(markers.size, markers.toSet().size)
        assertTrue(markers.none { it == TileMarker.NONE })
        assertEquals(TileOutline.DOTTED, HELMET_STYLE.getValue(SocketDrawState.ABSENT).outline)
    }

    // @verifies REQ-SIMFW-07
    @Test
    fun aSocketTheStateDoesNotNameIsAbsentAndTheEmptyStateCoversTheLattice() {
        val st = HelmetState.empty()
        assertEquals(SocketLattice.NP_SOCKET_COUNT, st.sockets.size)
        assertEquals(SocketDrawState.ABSENT, st.drawState(SocketLattice.NP_SOCKET_ID_MAX + 1))
    }
}
