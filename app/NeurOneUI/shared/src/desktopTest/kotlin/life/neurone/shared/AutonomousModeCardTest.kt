package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.models.SessionState
import life.neurone.shared.ble.ConnectionState
import life.neurone.shared.resources.*
import life.neurone.shared.ui.SessionScreen
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals

/** The Session tab offers Mode 3 programming once the helmet is connected and idle (`CLAUDE.md` §4.6; the iOS setup card). */
@OptIn(ExperimentalTestApi::class)
class AutonomousModeCardTest {
    private var title = ""
    private var choose = ""


    // @verifies REQ-SW03-03
    @Test fun the_card_is_offered_when_connected_and_idle_and_opens_the_menu_in_programming_mode() = runComposeUiTest {
        var opened = 0
        setContent {
            title = stringResource(Res.string.setup_autonomous_title)
            choose = stringResource(Res.string.setup_autonomous_choose_button)
            SessionScreen(
                connectionState = ConnectionState.CONNECTED, session = SessionState.EMPTY,
                onConnect = {}, onChooseProtocol = {}, onStop = {}, onProgramAutonomous = { opened++ },
            )
        }
        waitForIdle()
        onNodeWithText(title).assertExists()
        // The Session screen has its own "Choose Protocol" button above the card's, which is the second.
        onAllNodesWithText(choose)[1].performClick()
        assertEquals(1, opened)
    }

    // @verifies REQ-SW03-03
    @Test fun the_card_is_absent_when_the_host_offers_no_programming() = runComposeUiTest {
        setContent {
            title = stringResource(Res.string.setup_autonomous_title)
            SessionScreen(
                connectionState = ConnectionState.CONNECTED, session = SessionState.EMPTY,
                onConnect = {}, onChooseProtocol = {}, onStop = {},
            )
        }
        waitForIdle()
        onNodeWithText(title).assertDoesNotExist()
    }
}
