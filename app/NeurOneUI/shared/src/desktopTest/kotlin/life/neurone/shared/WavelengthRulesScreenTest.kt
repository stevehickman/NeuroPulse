package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.hasSetTextAction
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextReplacement
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.NPPBMChannelElement
import life.neurone.core.protocol.NPWavelengthRulesStore
import life.neurone.shared.resources.*
import life.neurone.shared.ui.WavelengthRulesScreen
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** The wavelength rules editor edits the store the compiler reads, and the store refuses an invalid window (REQ-NPPS-03). */
@OptIn(ExperimentalTestApi::class)
class WavelengthRulesScreenTest {

    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)
    private val used = listOf(Res.string.web_wl_rules_reset_all)

    private fun max808(s: NPWavelengthRulesStore) = s.resolved.channels.first { it.element == NPPBMChannelElement.LED_808 }.maxNm
    private fun min808(s: NPWavelengthRulesStore) = s.resolved.channels.first { it.element == NPPBMChannelElement.LED_808 }.minNm

    // @verifies REQ-NPPS-03
    @Test
    fun typingAWindow_changesTheStore_aBadOneDoesNot_andResetRestoresTheDefault() = runComposeUiTest {
        val store = NPWavelengthRulesStore(InMemoryKeyValueStore())
        setContent {
            strings = used.associateWith { stringResource(it) }
            WavelengthRulesScreen(store, onDone = {})
        }

        // Fields come in channel order, from then to: 660, 808, 1064.
        val fields = onAllNodes(hasSetTextAction())
        fields[3].performTextReplacement("860")
        assertEquals(860.0, max808(store))
        assertEquals(setOf(NPPBMChannelElement.LED_808), store.editedChannels)

        // A minimum above the maximum is refused, so the last good window stays.
        fields[2].performTextReplacement("900")
        assertEquals(798.0, min808(store))
        assertEquals(860.0, max808(store))

        onNodeWithText(text(Res.string.web_wl_rules_reset_all)).performClick()
        assertNull(store.userRules)
        assertEquals(840.0, max808(store))
    }
}
