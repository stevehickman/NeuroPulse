package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.isToggleable
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.assertCountEquals
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import life.neurone.core.analytics.CharacterisationEnrolmentGate
import life.neurone.core.analytics.GeneralConsentAnswer
import life.neurone.core.analytics.GeneralConsentPrompt
import life.neurone.core.analytics.GeneralConsentPromptKind
import life.neurone.core.analytics.GeneralShdrContent
import life.neurone.core.analytics.GeneralShdrContentSet
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.resources.*
import life.neurone.shared.ui.GENERAL_CONSENT_ITEM_NAMES
import life.neurone.shared.ui.GeneralConsentPromptDialog
import life.neurone.shared.ui.WarrantyOwnerScreen
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The warranty owner's general-consent prompts on the owner's page (`REQ-SHDR-12`; `OI-NVRAM-26`). The hub does not yet tell the
 * app that a window has ended, so the test records the end through the enrolment gate, as that signal would.
 */
@OptIn(ExperimentalTestApi::class)
class GeneralConsentPromptTest {

    private val used = listOf(
        Res.string.general_consent_window_title, Res.string.general_consent_content_title, Res.string.general_consent_keep,
        Res.string.general_consent_stop, Res.string.general_consent_agree, Res.string.general_consent_nothing_added,
        Res.string.general_consent_added_heading,
    )
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(resource: StringResource): String = strings.getValue(resource)

    private fun services(kv: InMemoryKeyValueStore) = AppServices(
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

    /** A helmet whose owner turned reports on and enrolled it, and whose window has ended. */
    private fun windowEnded(app: AppServices) {
        app.gattManager.onConnected(HUB)
        assertTrue(app.warrantyAnalyticsGate.grant(HUB))
        val char = CharacterisationEnrolmentGate(app.warrantyOwnerStore, app.warrantyAnalyticsGate)
        assertNotNull(char.grant(HUB))
        assertTrue(char.endWindow(HUB))
    }

    // @verifies REQ-SHDR-12#a
    // @verifies REQ-SHDR-12#c
    @Test
    fun theOwnersPageAsksOnceWhenTheWindowEndsAndKeepChangesNothing() = runComposeUiTest {
        val kv = InMemoryKeyValueStore()
        val app = services(kv)
        windowEnded(app)
        var showPage by mutableStateOf(true)
        setContent {
            strings = used.associateWith { stringResource(it) }
            if (showPage) WarrantyOwnerScreen(app, onDone = {})
        }
        waitForIdle()
        onNodeWithText(text(Res.string.general_consent_window_title)).assertExists()
        onNodeWithText(text(Res.string.general_consent_nothing_added)).assertExists()
        assertNull(app.generalShdrConsentPrompts.due(HUB), "recorded as put when first shown")
        onNodeWithText(text(Res.string.general_consent_keep)).performClick()
        waitForIdle()
        onAllNodesWithText(text(Res.string.general_consent_window_title)).assertCountEquals(0)
        assertTrue(app.warrantyAnalyticsGate.isOpen(HUB), "keep: the consent is as it was")
        assertEquals(0, app.warrantyAnalyticsGate.contentLevel(HUB))
        assertFalse(app.warrantyOwnerStore.record(HUB)!!.enrolled, "and characterisation is not enrolled again")
        // Opening the page again does not ask again.
        showPage = false
        waitForIdle()
        showPage = true
        waitForIdle()
        onAllNodesWithText(text(Res.string.general_consent_window_title)).assertCountEquals(0)
    }

    // @verifies REQ-SHDR-12#c
    @Test
    fun turningReportsOffFromThePromptIsAStopNotNeverAsked() = runComposeUiTest {
        val app = services(InMemoryKeyValueStore())
        windowEnded(app)
        setContent {
            strings = used.associateWith { stringResource(it) }
            WarrantyOwnerScreen(app, onDone = {})
        }
        waitForIdle()
        onNodeWithText(text(Res.string.general_consent_stop)).performClick()
        waitForIdle()
        assertFalse(app.warrantyAnalyticsGate.isOpen(HUB))
        assertTrue(app.warrantyAnalyticsGate.isDecided(HUB))
    }

    // Prompt 2 names what the set adds, and "Agree" needs the person to confirm they may decide for the helmet.
    // @verifies REQ-SHDR-12#b
    @Test
    fun theContentPromptNamesWhatIsAddedAndAgreeNeedsTheConfirmation() = runComposeUiTest {
        val prompt = GeneralConsentPrompt(
            GeneralConsentPromptKind.CONTENT_SET,
            GeneralShdrContentSet(1, listOf("emitter_on_seconds")),
            listOf("emitter_on_seconds"),
            reportsOn = true,
        )
        var answered: GeneralConsentAnswer? = null
        var calls = 0
        setContent {
            strings = used.associateWith { stringResource(it) }
            GeneralConsentPromptDialog(prompt, listOf("How long each light has been on")) { answered = it; calls++ }
        }
        waitForIdle()
        onNodeWithText(text(Res.string.general_consent_content_title)).assertExists()
        onNodeWithText(text(Res.string.general_consent_added_heading)).assertExists()
        onNodeWithText("How long each light has been on").assertExists()
        val agree = onNodeWithText(text(Res.string.general_consent_agree))
        agree.assertIsNotEnabled()
        onNode(isToggleable()).performClick()
        agree.assertIsEnabled()
        agree.performClick()
        assertEquals(GeneralConsentAnswer.AGREE, answered)
        assertEquals(1, calls)
    }

    // A set shipped in an update carries the words for every item it adds, or the prompt could not name them.
    // @verifies REQ-SHDR-12#b
    @Test
    fun everyItemTheShippedHistoryNamesHasWords() {
        val ids = GeneralShdrContent.history.flatMap { it.fields }.toSet()
        assertTrue(GENERAL_CONSENT_ITEM_NAMES.keys.containsAll(ids), "missing: ${ids - GENERAL_CONSENT_ITEM_NAMES.keys}")
        assertTrue(GeneralShdrContent.isWellFormed(GeneralShdrContent.history))
    }

    private companion object {
        const val HUB = "hub-a"
    }
}
