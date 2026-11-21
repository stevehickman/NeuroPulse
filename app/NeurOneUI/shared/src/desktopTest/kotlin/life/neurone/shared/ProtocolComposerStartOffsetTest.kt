package life.neurone.shared

import life.neurone.shared.ui.MAX_START_OFFSET_MINUTES
import life.neurone.shared.ui.startOffsetSecondsOrNull
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** A typed layer start offset is used as typed or refused, never replaced by a figure of the composer's own. */
class ProtocolComposerStartOffsetTest {

    // @verifies SWR-UI-01#f
    @Test
    fun aTypedOffset_isUsedAsTyped_andZeroIsAnOffset() {
        assertEquals(0, startOffsetSecondsOrNull("0"))
        assertEquals(60, startOffsetSecondsOrNull("1"))
        assertEquals(1500, startOffsetSecondsOrNull("25"))
    }

    // @verifies SWR-UI-01#f
    @Test
    fun aBlankOrUnrepresentableOffset_isRefused_notReplacedByZero() {
        // Both were saved as 0.
        assertNull(startOffsetSecondsOrNull(""))
        assertNull(startOffsetSecondsOrNull("99999999999999999999"))
        assertNull(startOffsetSecondsOrNull("not a number"))
    }

    // @verifies SWR-UI-01#f
    @Test
    fun theLargestRepresentableOffset_isTheLargestWholeMinuteThatFitsInSeconds() {
        assertEquals(MAX_START_OFFSET_MINUTES * 60, startOffsetSecondsOrNull(MAX_START_OFFSET_MINUTES.toString()))
        assertNull(startOffsetSecondsOrNull((MAX_START_OFFSET_MINUTES.toLong() + 1).toString()))
    }
}
