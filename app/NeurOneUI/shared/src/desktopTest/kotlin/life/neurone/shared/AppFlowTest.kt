package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.getBoundsInRoot
import androidx.compose.ui.unit.dp
import life.neurone.core.models.ResearchCategory
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
        Res.string.tab_history, Res.string.history_session_history,
        Res.string.tab_consumables, Res.string.consumable_intranasal_name,
        Res.string.session_browse_protocols, Res.string.tab_protocols, Res.string.setup_privacy_card_title,
        Res.string.dashboard_title, Res.string.dashboard_no_clinicians, Res.string.tab_settings,
        Res.string.dashboard_add_clinician, Res.string.clinician_grant_title, Res.string.common_cancel,
        Res.string.ota_firmware, Res.string.dashboard_revoke_button, Res.string.and_ui_brainwave_eeg_data_consent,
        Res.string.and_ui_review_consent, Res.string.and_ui_blanket_research_consent, Res.string.dashboard_per_category,
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
            override val protocolSigner = UnsupportedProtocolSigner()
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
        onNodeWithText(text(Res.string.session_browse_protocols)).assertExists()
        onNodeWithText(text(Res.string.tab_history)).performClick()
        onNodeWithText(text(Res.string.history_session_history)).assertExists()
        onNodeWithText(text(Res.string.tab_consumables)).performClick()
        onNodeWithText(text(Res.string.consumable_intranasal_name)).assertExists()
        // The Privacy tab is the shared consent dashboard; a fresh install has no clinician grants.
        onNodeWithText(text(Res.string.setup_privacy_card_title)).performClick()
        onNodeWithText(text(Res.string.dashboard_title)).assertExists()
        onNodeWithText(text(Res.string.dashboard_no_clinicians)).assertExists()
        // Settings is the shared screen too: every tab is, so no host can show a different one.
        onNodeWithText(text(Res.string.tab_settings)).performClick()
        onNodeWithText(text(Res.string.ota_firmware)).assertExists()
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

    @Test
    fun sessionTab_browsesTheBundledProtocolLibrary_onAHostWithNoHub() = runComposeUiTest {
        val kv = InMemoryKeyValueStore().apply {
            putBoolean(PersistedKeys.AGE_CONFIRMED_KEY, true)
            putBoolean(OnboardingKeys.BIPA_ACCEPTED, true)
            putBoolean(OnboardingKeys.CONSENT_SHOWN, true)
        }
        show(services(kv))

        // The Session tab is the first tab and is the shared screen, not the "not available" panel.
        onNodeWithText(text(Res.string.session_browse_protocols)).performClick()
        // The library is read through the shared NPPS core; a shipped protocol is listed.
        onNodeWithText(text(Res.string.tab_protocols)).assertExists()
        onNodeWithText("Alpha Calm").assertExists()
    }

    @Test
    fun privacyTab_opensTheClinicianGrantFormAndReturnsWithoutGrantingAnything() = runComposeUiTest {
        val kv = InMemoryKeyValueStore().apply {
            putBoolean(PersistedKeys.AGE_CONFIRMED_KEY, true)
            putBoolean(OnboardingKeys.BIPA_ACCEPTED, true)
            putBoolean(OnboardingKeys.CONSENT_SHOWN, true)
        }
        val app = services(kv)
        show(app)

        onNodeWithText(text(Res.string.setup_privacy_card_title)).performClick()
        onNodeWithText(text(Res.string.dashboard_add_clinician)).performClick()
        onNodeWithText(text(Res.string.clinician_grant_title)).assertExists()
        onNodeWithText(text(Res.string.common_cancel)).performClick()

        // Cancelling is not a grant: the dashboard is back and no clinician access exists.
        onNodeWithText(text(Res.string.dashboard_no_clinicians)).assertExists()
        assertTrue(app.consentStore.clinicianGrants.isEmpty())
    }

    @Test
    fun settings_revokingBrainwaveConsentTakesEffectAndCanBeReviewed() = runComposeUiTest {
        val kv = InMemoryKeyValueStore().apply {
            putBoolean(PersistedKeys.AGE_CONFIRMED_KEY, true)
            putBoolean(OnboardingKeys.BIPA_ACCEPTED, true)
            putBoolean(OnboardingKeys.CONSENT_SHOWN, true)
        }
        show(services(kv))

        onNodeWithText(text(Res.string.tab_settings)).performClick()
        onNodeWithText(text(Res.string.and_ui_brainwave_eeg_data_consent)).assertExists()
        // Revoking is immediate and persisted; the screen then offers to review the release again.
        onNodeWithText(text(Res.string.dashboard_revoke_button)).performClick()
        assertFalse(kv.getBoolean(OnboardingKeys.BIPA_ACCEPTED), "revoking the brainwave release must be stored")
        onNodeWithText(text(Res.string.and_ui_review_consent)).assertExists()
    }

    @Test
    fun privacyTab_putsThePerCategoryTitleDirectlyAboveTheCategoryList() = runComposeUiTest {
        val kv = InMemoryKeyValueStore().apply {
            putBoolean(PersistedKeys.AGE_CONFIRMED_KEY, true)
            putBoolean(OnboardingKeys.BIPA_ACCEPTED, true)
            putBoolean(OnboardingKeys.CONSENT_SHOWN, true)
        }
        show(services(kv))
        onNodeWithText(text(Res.string.setup_privacy_card_title)).performClick()

        val blanket = onNodeWithText(text(Res.string.and_ui_blanket_research_consent)).getBoundsInRoot()
        val title = onNodeWithText(text(Res.string.dashboard_per_category)).getBoundsInRoot()
        val firstCategory = onNodeWithText(ResearchCategory.entries.first().displayName).getBoundsInRoot()

        // The blanket switch is a separate control above the heading, not between the heading and its list.
        assertTrue(blanket.bottom <= title.top, "the blanket switch must come before the per-category title")
        // Nothing sits between the title and the first category: only a small gap.
        assertTrue(firstCategory.top >= title.bottom, "the list follows its title")
        assertTrue(firstCategory.top - title.bottom <= 12.dp, "the title is immediately adjacent to the category list")
    }
}
