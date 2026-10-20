package life.neurone.shared

import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.hasSetTextAction
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.click
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.SocketLattice
import life.neurone.shared.resources.*
import life.neurone.shared.ui.ZoneEditorScreen
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals

/** A zone defined in the editor is saved with the user's scripts and resolves at once (REQ-NPPS-12). */
@OptIn(ExperimentalTestApi::class)
class ZoneEditorScreenTest {

    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)
    private val used = listOf(Res.string.web_define_new_zone, Res.string.zones_save_zone)

    // @verifies REQ-NPPS-12
    @Test
    fun typingSockets_andTappingTheMap_shareOneSelection_andSaveMakesTheZone() = runComposeUiTest {
        val library = NPProtocolLibrary(InMemoryKeyValueStore())
        setContent {
            strings = used.associateWith { stringResource(it) }
            ZoneEditorScreen(library, onDone = {})
        }

        onNodeWithText(text(Res.string.web_define_new_zone)).performClick()
        val fields = onAllNodes(hasSetTextAction())
        fields[0].performTextInput("Spot")
        fields[1].performTextInput("4, 6")

        // Tap socket 2 on the map: it joins the selection the typed list made.
        val cell = SocketLattice.cells.first { it.id == 2 }
        val minX = SocketLattice.cells.minOf { it.x }
        val maxX = SocketLattice.cells.maxOf { it.x }
        val pad = 0.9
        val map = onNodeWithTag("socket_map")
        map.performScrollTo()
        val width = map.fetchSemanticsNode().size.width
        val unit = width / ((maxX - minX) + 2 * pad)
        map.performTouchInput { click(Offset(((cell.x - minX + pad) * unit).toFloat(), ((cell.y + pad) * unit).toFloat())) }

        onNodeWithText(text(Res.string.zones_save_zone)).performScrollTo().performClick()
        assertEquals(listOf(2, 4, 6), library.userZones.single().sockets)
        assertEquals("Spot", library.userZones.single().name)
    }

    // @verifies REQ-NPPS-12
    @Test
    fun aNameAShippedZoneHas_cannotBeSaved() = runComposeUiTest {
        val library = NPProtocolLibrary(InMemoryKeyValueStore())
        setContent {
            strings = used.associateWith { stringResource(it) }
            ZoneEditorScreen(library, onDone = {})
        }
        onNodeWithText(text(Res.string.web_define_new_zone)).performClick()
        val fields = onAllNodes(hasSetTextAction())
        fields[0].performTextInput("Frontal Left")
        fields[1].performTextInput("1, 2")
        onNodeWithText(text(Res.string.zones_save_zone)).performScrollTo().assertIsNotEnabled()
        assertEquals(emptyList(), library.userZones)
    }
}
