package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.consent.ConsentStore
import life.neurone.core.models.ResearchConsentState
import life.neurone.shared.resources.*
import life.neurone.shared.ui.ConsentOnboardingScreen
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals

/** The research-consent onboarding is two screens whose controls follow the layers, and L3 carries its notice (ported from `ConsentStoreTests`, `ConsentEngineTests`). */
@OptIn(ExperimentalTestApi::class)
class ConsentOnboardingScreenTest {
    private val used = listOf(
        Res.string.consent_l4_results_toggle, Res.string.consent_l4_portal_toggle, Res.string.consent_l1_toggle,
        Res.string.consent_s2_select_all_toggle, Res.string.consent_s2_blanket_toggle, Res.string.consent_important_label,
        Res.string.setup_continue_button, Res.string.consent_finish,
        Res.string.consent_and_permanently_stops_any_further_data_flowi,
    )
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)

    private fun store(blanket: Boolean = false): ConsentStore {
        val kv = InMemoryKeyValueStore()
        val store = ConsentStore(kv, ResearchAnalyticsGate(kv, NoOpAnalyticsBackend()))
        if (blanket) store.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true))
        return store
    }

    // @verifies REQ-CONSENT-06
    @Test
    fun onboardingIsTwoScreens_S1CarriesL4AndL1_S2CarriesL2AndL3AsTwoControls() = runComposeUiTest {
        var completed = 0
        val store = store()
        setContent {
            strings = used.associateWith { stringResource(it) }
            ConsentOnboardingScreen(store, onComplete = { completed++ })
        }
        // S1: what you get back. L4 and L1 are here; the L2 and L3 controls are not.
        onNodeWithText(text(Res.string.consent_l4_results_toggle)).assertExists()
        onNodeWithText(text(Res.string.consent_l1_toggle)).assertExists()
        onNodeWithText(text(Res.string.consent_s2_select_all_toggle)).assertDoesNotExist()
        onNodeWithText(text(Res.string.consent_s2_blanket_toggle)).assertDoesNotExist()

        onNodeWithText(text(Res.string.setup_continue_button)).performClick()

        // S2: what you share. Select-all (scope) and the blanket toggle (posture) are two separate controls; L4 and L1 are gone.
        onNodeWithText(text(Res.string.consent_s2_select_all_toggle)).assertExists()
        onNodeWithText(text(Res.string.consent_s2_blanket_toggle)).assertExists()
        onNodeWithText(text(Res.string.consent_l4_results_toggle)).assertDoesNotExist()
        assertEquals(0, completed, "two screens: the second one is the last, not a third")

        onNodeWithText(text(Res.string.consent_finish)).performClick()
        assertEquals(1, completed)
    }

    private fun androidx.compose.ui.test.ComposeUiTest.showSecondScreen(blanket: Boolean) {
        val store = store(blanket)
        setContent {
            strings = used.associateWith { stringResource(it) }
            ConsentOnboardingScreen(store, onComplete = {})
        }
        onNodeWithText(text(Res.string.setup_continue_button)).performClick()
    }

    // @verifies REQ-CONSENT-07
    @Test
    fun theIrreversibilityNoticeIsShownWhenBlanketConsentIsOn() = runComposeUiTest {
        showSecondScreen(blanket = true)
        onNodeWithText(text(Res.string.consent_important_label)).assertExists()
        // The copy says what withdrawal does do.
        onNodeWithText(text(Res.string.consent_and_permanently_stops_any_further_data_flowi), substring = true).assertExists()
    }

    // @verifies REQ-CONSENT-07
    @Test
    fun noNoticeIsShownWhileBlanketConsentIsOff() = runComposeUiTest {
        showSecondScreen(blanket = false)
        onNodeWithText(text(Res.string.consent_important_label)).assertDoesNotExist()
    }
}
