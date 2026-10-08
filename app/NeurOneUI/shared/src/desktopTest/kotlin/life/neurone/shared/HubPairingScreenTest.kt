package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.models.SessionState
import life.neurone.shared.resources.*
import life.neurone.shared.ui.SessionScreen
import life.neurone.shared.ble.ConnectionState
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals

/** The pairing notice and the forget-hub action on the Session screen (`OI-UI-KMP-03`). */
@OptIn(ExperimentalTestApi::class)
class HubPairingScreenTest {
    private lateinit var title: String
    private lateinit var forget: String

    private fun androidx.compose.ui.test.ComposeUiTest.show(
        state: ConnectionState,
        pairingRequired: Boolean,
        onForgetHub: (() -> Unit)?,
    ) = setContent {
        title = stringResource(Res.string.ble_pairing_required_title)
        forget = stringResource(Res.string.ble_forget_hub)
        SessionScreen(
            connectionState = state, session = SessionState.EMPTY, onConnect = {}, onChooseProtocol = {}, onStop = {},
            pairingRequired = pairingRequired, onForgetHub = onForgetHub,
        )
    }

    @Test fun the_pairing_notice_shows_only_when_connected_and_unpaired() = runComposeUiTest {
        show(ConnectionState.CONNECTED, pairingRequired = true, onForgetHub = null)
        onNodeWithText(title).assertExists()
    }

    @Test fun no_notice_when_paired_or_not_connected() = runComposeUiTest {
        show(ConnectionState.CONNECTED, pairingRequired = false, onForgetHub = null)
        onNodeWithText(title).assertDoesNotExist()
    }

    @Test fun the_forget_action_is_offered_only_when_there_is_a_hub_to_forget() = runComposeUiTest {
        var forgotten = 0
        show(ConnectionState.DISCONNECTED, pairingRequired = false, onForgetHub = { forgotten++ })
        onNodeWithText(forget).performClick()
        assertEquals(1, forgotten)
    }

    @Test fun no_forget_action_without_a_remembered_hub() = runComposeUiTest {
        show(ConnectionState.DISCONNECTED, pairingRequired = false, onForgetHub = null)
        onNodeWithText(forget).assertDoesNotExist()
    }
}
