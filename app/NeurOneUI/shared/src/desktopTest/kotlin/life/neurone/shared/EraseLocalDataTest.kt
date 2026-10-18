package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.isToggleable
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.resources.*
import life.neurone.shared.ui.SettingsScreen
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * The browser's "erase this browser's data" action, as the shared Settings screen shows it (REQ-SW03-18 (c)): offered only by a
 * host that supplies it, behind an acknowledgement that it cannot be undone, and run exactly when the person asks.
 */
@OptIn(ExperimentalTestApi::class)
class EraseLocalDataTest {
    private val used = listOf(
        Res.string.settings_erase_local_title, Res.string.settings_erase_local_confirm, Res.string.settings_erase_local_button,
    )
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)

    private fun services(erase: (() -> Unit)?) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = InMemoryKeyValueStore()
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val eraseLocalData = erase
            @androidx.compose.runtime.Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    // @verifies REQ-SW03-18#c
    @Test
    fun theEraseButtonStaysDisabledUntilTheAcknowledgementAndThenRunsTheHostsAction() = runComposeUiTest {
        var erased = 0
        val app = services { erased++ }
        setContent {
            strings = used.associateWith { stringResource(it) }
            SettingsScreen(app)
        }
        onNodeWithText(text(Res.string.settings_erase_local_title)).assertExists()
        onNodeWithText(text(Res.string.settings_erase_local_button)).assertIsNotEnabled()
        onNodeWithText(text(Res.string.settings_erase_local_button)).performClick()
        assertEquals(0, erased, "a disabled button erases nothing")

        onNode(isToggleable().and(androidx.compose.ui.test.hasAnySibling(androidx.compose.ui.test.hasText(text(Res.string.settings_erase_local_confirm))))).performClick()
        onNodeWithText(text(Res.string.settings_erase_local_button)).assertIsEnabled()
        onNodeWithText(text(Res.string.settings_erase_local_button)).performClick()
        assertEquals(1, erased, "the host's action runs once, when asked")
    }

    // @verifies REQ-SW03-18#c
    @Test
    fun aHostWithNoEraseActionShowsNoSection() = runComposeUiTest {
        val app = services(null)
        setContent {
            strings = used.associateWith { stringResource(it) }
            SettingsScreen(app)
        }
        onNodeWithText(text(Res.string.settings_erase_local_title)).assertDoesNotExist()
    }
}
