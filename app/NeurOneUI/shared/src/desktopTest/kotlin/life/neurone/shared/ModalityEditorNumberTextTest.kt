package life.neurone.shared

import life.neurone.shared.ui.numberText
import life.neurone.shared.ui.typedDouble
import life.neurone.shared.ui.typedInt
import life.neurone.shared.ui.wholeNumberText
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/**
 * A number field of the modality editor holds a number or says it does not. Blank and a lone point used to be left in the
 * field while the old value stayed stored, so the field and the stored value disagreed without a mark.
 */
class ModalityEditorNumberTextTest {

    // @verifies SWR-UI-01#f
    @Test
    fun blankAndALonePoint_areNotNumbers_soTheFieldMarksThemUnread() {
        assertNull(typedDouble(""))
        assertNull(typedDouble("."))
        assertNull(typedInt(""))
    }

    // @verifies SWR-UI-01#f
    @Test
    fun aTypedNumber_isUsedAsTyped() {
        assertEquals(40.0, typedDouble("40"))
        assertEquals(0.0, typedDouble("0"))
        assertEquals(0.5, typedDouble(".5"))
        assertEquals(5.0, typedDouble("5."))
        assertEquals(0, typedInt("0"))
        assertEquals(25, typedInt("25"))
    }

    // @verifies SWR-UI-01#f
    @Test
    fun anEditThatCouldNotBeTheStartOfANumber_isRefused_andTheFieldKeepsItsText() {
        assertEquals("1.2", numberText("1.2", "1.2."))             // a second point
        assertEquals("1.2", numberText("1.2", "1.2.3"))
        assertEquals("5", numberText("5", "9".repeat(400)))        // too many digits to be finite
        assertEquals("12", wholeNumberText("12", "12345678901"))   // too large for an Int
    }

    // @verifies SWR-UI-01#f
    @Test
    fun theTextAPersonPassesThroughWhileTyping_isKept_notReplaced() {
        assertEquals("", numberText("5", ""))                      // cleared: kept blank, never put back to 5
        assertEquals(".", numberText("", "."))
        assertEquals("1.", numberText("1", "1."))
        assertEquals("", wholeNumberText("5", ""))
        assertEquals("12", numberText("1", "1a2"))                 // characters that are not digits or a point are dropped
    }
}
