package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.ui.NeurOneApp
import life.neurone.shared.ui.SIMULATED_HUB_NOTICE_TAG
import kotlin.test.Test

/** A screen that shows a simulated hub says so (NP-SIM-FW-001 §27, `REQ-SIMFW-19` (c)); a host with a real central says nothing. */
@OptIn(ExperimentalTestApi::class)
class SimulatedHubNoticeTest {
    private fun services(simulated: Boolean) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = InMemoryKeyValueStore()
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val simulatedHub = simulated
            @androidx.compose.runtime.Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    // @verifies REQ-SIMFW-19#c
    @Test
    fun aSimulatedHubIsSaidToBeSimulatedAboveEveryScreen() = runComposeUiTest {
        val app = services(simulated = true)
        setContent { NeurOneApp(app) }
        waitForIdle()
        onNodeWithTag(SIMULATED_HUB_NOTICE_TAG).assertExists()
    }

    // @verifies REQ-SIMFW-19#c
    @Test
    fun aRealHubHostShowsNoSuchNotice() = runComposeUiTest {
        val app = services(simulated = false)
        setContent { NeurOneApp(app) }
        waitForIdle()
        onNodeWithTag(SIMULATED_HUB_NOTICE_TAG).assertDoesNotExist()
    }
}
