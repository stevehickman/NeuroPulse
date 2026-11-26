package life.neurone.shared

import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.platform.UriHandler
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.protocol.NPBundledConditions
import life.neurone.core.protocol.NPConditionLinkPolicy
import life.neurone.core.protocol.NPResolvedCondition
import life.neurone.shared.resources.*
import life.neurone.shared.ui.ConditionChips
import life.neurone.shared.ui.ConditionLinkConfirmation
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** A condition chip asks before it leaves the app, shows the host and the whole address, and opens nothing the link policy refused (NP-COND-LINK-001). */
@OptIn(ExperimentalTestApi::class)
class ConditionChipsTest {

    private class Recorder : UriHandler {
        val opened = mutableListOf<String>()
        override fun openUri(uri: String) { opened += uri }
    }

    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)
    private val used = listOf(
        Res.string.ui_condition_open_in_browser, Res.string.condition_link_not_in_registry, Res.string.common_cancel,
        Res.string.common_close,
    )

    private fun androidx.compose.ui.test.ComposeUiTest.show(names: List<String>, recorder: Recorder) = setContent {
        strings = used.associateWith { stringResource(it) }
        CompositionLocalProvider(LocalUriHandler provides recorder) { ConditionChips(names) }
    }

    // @verifies REQ-NPPS-12
    @Test
    fun aRegisteredCondition_asksFirst_thenOpensExactlyItsLink() = runComposeUiTest {
        val registered = NPBundledConditions.all.first { it.link.startsWith("https://") }
        val recorder = Recorder()
        show(listOf(registered.name), recorder)

        // Choosing the chip opens the confirmation; nothing has left the app yet.
        onNodeWithText(registered.name, substring = true).performClick()
        assertTrue(recorder.opened.isEmpty())
        val host = registered.link.removePrefix("https://").substringBefore('/')
        onNodeWithText(host).assertExists()
        onNodeWithText(registered.link).assertExists()

        onNodeWithText(text(Res.string.ui_condition_open_in_browser)).performClick()
        assertEquals(listOf(registered.link), recorder.opened)
    }

    // @verifies REQ-NPPS-12
    @Test
    fun anUnlistedCondition_isShownStruckThrough_andHasNoOpenAction() = runComposeUiTest {
        val recorder = Recorder()
        show(listOf("A condition nobody registered"), recorder)

        onNodeWithText("A condition nobody registered").performClick()
        onNodeWithText(text(Res.string.condition_link_not_in_registry), substring = true).assertExists()
        onNodeWithText(text(Res.string.ui_condition_open_in_browser)).assertDoesNotExist()
        onNodeWithText(text(Res.string.common_close)).performClick()
        assertTrue(recorder.opened.isEmpty())
    }

    // @verifies REQ-SW03-38
    @Test
    fun theConfirmationShowsTheWholeAddressSoAQueryStringIsSeenBeforeLeaving() = runComposeUiTest {
        val link = "https://en.wikipedia.org/wiki/Migraine?utm_source=np&ref=a1b2c3#Signs"
        val condition = NPResolvedCondition("Migraine", null, NPConditionLinkPolicy.evaluate(link))
        val recorder = Recorder()
        setContent {
            strings = used.associateWith { stringResource(it) }
            CompositionLocalProvider(LocalUriHandler provides recorder) { ConditionLinkConfirmation(condition, onDismiss = {}) }
        }
        onNodeWithText("en.wikipedia.org").assertExists()
        onNodeWithText(link).assertExists()
        onNodeWithText(text(Res.string.ui_condition_open_in_browser)).performClick()
        assertEquals(listOf(link), recorder.opened, "what is opened is the address shown")
    }
}
