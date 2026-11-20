package life.neurone.shared

import life.neurone.shared.ui.durationSecondsOrNull
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** A typed protocol duration is used as typed or refused, never replaced by a figure of the editor's own. */
class ProtocolEditorDurationTest {

    // @verifies SWR-UI-01#f
    @Test
    fun aTypedDuration_isUsedAsTyped() {
        assertEquals(60, durationSecondsOrNull("1"))
        assertEquals(1200, durationSecondsOrNull("20"))
        assertEquals(5400, durationSecondsOrNull("90"))
    }

    // @verifies SWR-UI-01#f
    @Test
    fun aBlankZeroOrUnrepresentableDuration_isRefused_notReplaced() {
        // These were saved as 20 minutes (blank, or digits too many for an Int) and as 1 minute (zero).
        assertNull(durationSecondsOrNull(""))
        assertNull(durationSecondsOrNull("0"))
        assertNull(durationSecondsOrNull("99999999999999999999"))
        assertNull(durationSecondsOrNull("not a number"))
    }

    // @verifies SWR-UI-01#f
    @Test
    fun theLargestRepresentableDuration_isTheLargestWholeMinuteThatFitsInSeconds() {
        assertEquals(Int.MAX_VALUE / 60 * 60, durationSecondsOrNull((Int.MAX_VALUE / 60).toString()))
        assertNull(durationSecondsOrNull((Int.MAX_VALUE / 60 + 1).toString()))
    }
}
