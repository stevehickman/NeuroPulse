package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.shared.resources.*
import life.neurone.shared.ui.pluralString
import kotlin.test.Test
import kotlin.test.assertEquals

/** A count picks the singular or the plural text, from the same keys the SwiftUI app reads (`REQ-SW03-09`). */
@OptIn(ExperimentalTestApi::class)
class PluralStringTest {

    // @verifies REQ-SW03-09
    @Test
    fun theCountSelectsThePluralCategory() = runComposeUiTest {
        val seen = mutableListOf<String>()
        setContent {
            seen += pluralString(Res.string.protocol_timing_intervals_one, Res.string.protocol_timing_intervals_other, 1)
            seen += pluralString(Res.string.protocol_timing_intervals_one, Res.string.protocol_timing_intervals_other, 3)
            seen += pluralString(Res.string.modality_sockets_targeted_one, Res.string.modality_sockets_targeted_other, 1)
            seen += pluralString(Res.string.modality_sockets_targeted_one, Res.string.modality_sockets_targeted_other, 7)
        }
        assertEquals(listOf("1 interval", "3 intervals", "1 socket targeted.", "7 sockets targeted."), seen)
    }
}
