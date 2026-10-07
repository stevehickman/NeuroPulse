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
import life.neurone.core.common.KeyValueStore
import life.neurone.core.protocol.PersistedKeys
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
 * The cross-platform flow, run headless on the JVM: the same composables a phone, a browser and a desktop
 * window show. A platform host can change how this looks; it cannot change what it does, and this test is
 * where that is held (app/NeurOneUI/README.md).
 */
@OptIn(ExperimentalTestApi::class)
class AppFlowTest {

    private val used = listOf(
        Res.string.setup_title_welcome, Res.string.setup_continue_button, Res.string.age_your_brainwave_data,
        Res.string.age_agree_and_continue, Res.string.consent_research_participation, Res.string.consent_skip_button,
        Res.string.app_tab_pending_body, Res.string.tab_history, Res.string.history_session_history,
        Res.string.tab_consumables, Res.string.consumable_intranasal_name,
    )

    /** The English text of [resource], read in composition: that is where the resource environment is defined. */
    private var strings: Map<StringResource, String> = emptyMap()

    private fun text(resource: StringResource): String = strings.getValue(resource)

    private fun androidx.compose.ui.test.ComposeUiTest.show(app: AppServices) = setContent {
        strings = used.associateWith { stringResource(it) }
        NeurOneApp(app)
    }

    private fun services(kv: KeyValueStore = InMemoryKeyValueStore()) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val analyticsBackend = NoOpAnalyticsBackend()
            @androidx.compose.runtime.Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    @Test
    fun freshInstall_walksAgeGate_biometricRelease_consent_thenTheTabs() = runComposeUiTest {
        val kv = InMemoryKeyValueStore()
        val app = services(kv)
        show(app)

        // 1. Nothing proceeds until the age declaration is ticked, and it starts unticked.
        onNodeWithText(text(Res.string.setup_title_welcome)).assertExists()
        onNodeWithText(text(Res.string.setup_continue_button)).assertIsNotEnabled()
        onNode(isToggleable()).performClick()
        onNodeWithText(text(Res.string.setup_continue_button)).assertIsEnabled().performClick()
        assertTrue(kv.getBoolean(PersistedKeys.AGE_CONFIRMED_KEY))
        assertFalse(kv.getBoolean(OnboardingKeys.BIPA_ACCEPTED), "the biometric release is not implied by the age gate")

        // 2. The biometric written release likewise needs an explicit tick.
        onNodeWithText(text(Res.string.age_your_brainwave_data)).assertExists()
        onNodeWithText(text(Res.string.age_agree_and_continue)).assertIsNotEnabled()
        onNode(isToggleable()).performClick()
        onNodeWithText(text(Res.string.age_agree_and_continue)).assertIsEnabled().performClick()
        assertTrue(kv.getBoolean(OnboardingKeys.BIPA_ACCEPTED))

        // 3. Research consent is optional; skipping it grants nothing.
        onNodeWithText(text(Res.string.consent_research_participation)).assertExists()
        onNodeWithText(text(Res.string.consent_skip_button)).performClick()
        assertTrue(kv.getBoolean(OnboardingKeys.CONSENT_SHOWN))
        assertFalse(app.consentStore.researchConsent.blanketConsentGranted, "skipping research consent must not grant blanket consent")

        // 4. The tabs: the screens that moved are real, the rest say so.
        onNodeWithText(text(Res.string.app_tab_pending_body)).assertExists()
        onNodeWithText(text(Res.string.tab_history)).performClick()
        onNodeWithText(text(Res.string.history_session_history)).assertExists()
        onNodeWithText(text(Res.string.tab_consumables)).performClick()
        onNodeWithText(text(Res.string.consumable_intranasal_name)).assertExists()
    }

    @Test
    fun returningUser_goesStraightToTheTabs() = runComposeUiTest {
        val kv = InMemoryKeyValueStore().apply {
            putBoolean(PersistedKeys.AGE_CONFIRMED_KEY, true)
            putBoolean(OnboardingKeys.BIPA_ACCEPTED, true)
            putBoolean(OnboardingKeys.CONSENT_SHOWN, true)
        }
        show(services(kv))
        onNodeWithText(text(Res.string.tab_consumables)).assertExists()
    }
}
