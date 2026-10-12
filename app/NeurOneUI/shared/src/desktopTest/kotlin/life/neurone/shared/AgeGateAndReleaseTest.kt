package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertCountEquals
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.assertIsOff
import androidx.compose.ui.test.assertIsOn
import androidx.compose.ui.test.hasSetTextAction
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
import life.neurone.shared.ui.NeurOneApp
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import java.util.Locale
import kotlin.test.Test

/**
 * The age declaration and the biometric written release, one rule at a time (`REQ-ONBOARD-01`,
 * `REQ-ONBOARD-02`). `AppFlowTest` walks the whole flow; this class holds each rule on its own so a failure
 * names the rule.
 *
 * Written against the shared composables in `OnboardingScreens.kt`. Not run where it was written (no Gradle
 * there): CI is its first run.
 */
@OptIn(ExperimentalTestApi::class)
class AgeGateAndReleaseTest {

    private val used = listOf(
        Res.string.setup_title_welcome, Res.string.setup_continue_button,
        Res.string.age_your_brainwave_data, Res.string.age_agree_and_continue,
        Res.string.consent_research_participation,
    )

    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(resource: StringResource): String = strings.getValue(resource)

    private fun app() = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = InMemoryKeyValueStore()
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            @androidx.compose.runtime.Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    private fun androidx.compose.ui.test.ComposeUiTest.showFresh() = setContent {
        strings = used.associateWith { stringResource(it) }
        NeurOneApp(app())
    }

    // @verifies REQ-ONBOARD-01#a
    @Test
    fun theAgeDeclarationIsTheFirstScreen_andNothingElseIsShownBeforeIt() = runComposeUiTest {
        showFresh()
        onNodeWithText(text(Res.string.setup_title_welcome)).assertExists()
        onNodeWithText(text(Res.string.age_your_brainwave_data)).assertDoesNotExist()
        onNodeWithText(text(Res.string.consent_research_participation)).assertDoesNotExist()
    }

    // @verifies REQ-ONBOARD-01#b
    @Test
    fun theDeclarationCheckboxIsNotPreTicked() = runComposeUiTest {
        showFresh()
        onAllNodes(isToggleable()).assertCountEquals(1)
        onNode(isToggleable()).assertIsOff()
    }

    // @verifies REQ-ONBOARD-01#c
    @Test
    fun theFlowCannotProceedWhileTheDeclarationIsUnticked() = runComposeUiTest {
        showFresh()
        onNodeWithText(text(Res.string.setup_continue_button)).assertIsNotEnabled()
        onNode(isToggleable()).performClick().assertIsOn()
        onNodeWithText(text(Res.string.setup_continue_button)).assertIsEnabled()
        onNode(isToggleable()).performClick().assertIsOff()
        onNodeWithText(text(Res.string.setup_continue_button)).assertIsNotEnabled()
    }

    // @verifies REQ-ONBOARD-01#d
    @Test
    fun nothingBeyondTheDeclarationIsAsked() = runComposeUiTest {
        showFresh()
        // One tick box and no text entry: no date of birth, no document, no code.
        onAllNodes(isToggleable()).assertCountEquals(1)
        onAllNodes(hasSetTextAction()).assertCountEquals(0)
    }

    // @verifies REQ-ONBOARD-02#a
    @Test
    fun theBiometricReleaseIsItsOwnScreen_notBundledWithConsent() = runComposeUiTest {
        showFresh()
        onNode(isToggleable()).performClick()
        onNodeWithText(text(Res.string.setup_continue_button)).performClick()

        onNodeWithText(text(Res.string.age_your_brainwave_data)).assertExists()
        onNodeWithText(text(Res.string.setup_title_welcome)).assertDoesNotExist()
        onNodeWithText(text(Res.string.consent_research_participation)).assertDoesNotExist()
        // Its own tick box, and nothing proceeds without it.
        onNodeWithText(text(Res.string.age_agree_and_continue)).assertIsNotEnabled()
    }

    // @verifies REQ-ONBOARD-02#b
    @Test
    fun theBiometricReleaseIsShownWhateverTheLocale() {
        // The app holds no location, IP or state lookup, so the locale is the only input a test can vary.
        val saved = Locale.getDefault()
        try {
            for (locale in listOf(Locale.US, Locale.GERMANY, Locale.JAPAN, Locale.forLanguageTag("ar-EG"))) {
                Locale.setDefault(locale)
                runComposeUiTest {
                    showFresh()
                    onNode(isToggleable()).performClick()
                    onNodeWithText(text(Res.string.setup_continue_button)).performClick()
                    onNodeWithText(text(Res.string.age_your_brainwave_data)).assertExists()
                }
            }
        } finally {
            Locale.setDefault(saved)
        }
    }
}
