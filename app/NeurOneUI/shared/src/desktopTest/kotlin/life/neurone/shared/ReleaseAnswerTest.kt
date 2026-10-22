package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.isToggleable
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.PersistedKeys
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.resources.*
import life.neurone.shared.ui.NeurOneApp
import life.neurone.shared.ui.OnboardingKeys
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The biometric written release states what it collects and what NeurOne will not do (`REQ-ONBOARD-03`), and the answer is recorded as
 * given (`REQ-ONBOARD-06` (a)). Ported from the SwiftUI app's `BIPAConsentFlowTests`, which only simulated the callbacks; these drive the
 * real screen.
 */
@OptIn(ExperimentalTestApi::class)
class ReleaseAnswerTest {
    private val used = listOf(
        Res.string.age_agree_and_continue, Res.string.bipa_step4_decline_button, Res.string.consent_research_participation,
        Res.string.age_retention_your_recordings_stay_on_your_devic, Res.string.age_neurone_s_biometric_data_retention_and_destr,
        Res.string.age_on_your_device_for_session_delivery_and_neur,
        Res.string.bipa_step3_limit_1, Res.string.bipa_step3_limit_2,
    )
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)

    private fun app(kv: InMemoryKeyValueStore) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            @androidx.compose.runtime.Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    /** The app, past the age gate, on the release screen. */
    private fun androidx.compose.ui.test.ComposeUiTest.showRelease(kv: InMemoryKeyValueStore) {
        kv.putBoolean(PersistedKeys.AGE_CONFIRMED_KEY, true)
        val services = app(kv)
        setContent {
            strings = used.associateWith { stringResource(it) }
            NeurOneApp(services)
        }
    }

    // @verifies REQ-ONBOARD-03
    @Test
    fun theReleaseStatesPurposeRetentionDestructionAndWhatNeurOneWillNotDo() = runComposeUiTest {
        showRelease(InMemoryKeyValueStore())
        // Purpose, retention, and where the destruction method is published.
        onNodeWithText(text(Res.string.age_on_your_device_for_session_delivery_and_neur), substring = true).assertExists()
        onNodeWithText(text(Res.string.age_retention_your_recordings_stay_on_your_devic), substring = true).assertExists()
        onNodeWithText(text(Res.string.age_neurone_s_biometric_data_retention_and_destr), substring = true).assertExists()
        // What NeurOne will not do: sell, lease or profit; share with a third party without separate consent.
        onNodeWithText(text(Res.string.bipa_step3_limit_1), substring = true).assertExists()
        onNodeWithText(text(Res.string.bipa_step3_limit_2), substring = true).assertExists()
    }

    // @verifies REQ-ONBOARD-06#a
    @Test
    fun acceptingRecordsTheScreenAsShownAndAccepted() = runComposeUiTest {
        val kv = InMemoryKeyValueStore()
        showRelease(kv)
        assertFalse(kv.getBoolean(OnboardingKeys.BIPA_SHOWN))
        onNode(isToggleable()).performClick()
        onNodeWithText(text(Res.string.age_agree_and_continue)).performClick()
        assertTrue(kv.getBoolean(OnboardingKeys.BIPA_SHOWN), "shown")
        assertTrue(kv.getBoolean(OnboardingKeys.BIPA_ACCEPTED), "accepted")
        // The next screen is the research consent, so the decline callback did not also fire.
        onNodeWithText(text(Res.string.consent_research_participation)).assertExists()
    }

    // @verifies REQ-ONBOARD-06#a
    @Test
    fun decliningRecordsTheScreenAsShownAndNotAccepted() = runComposeUiTest {
        val kv = InMemoryKeyValueStore()
        showRelease(kv)
        onNodeWithText(text(Res.string.bipa_step4_decline_button)).performClick()
        assertTrue(kv.getBoolean(OnboardingKeys.BIPA_SHOWN), "shown")
        assertFalse(kv.getBoolean(OnboardingKeys.BIPA_ACCEPTED), "not accepted")
    }
}
