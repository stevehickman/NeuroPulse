package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.NPIndividualProfile
import life.neurone.core.protocol.NPLimitsStore
import life.neurone.shared.resources.*
import life.neurone.shared.ui.LimitsSettingsScreen
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** The Limits screen edits all three tiers, and a helmet's tier is the connected hub's (REQ-NPPS-12). */
@OptIn(ExperimentalTestApi::class)
class LimitsTiersTest {

    private val used = listOf(
        Res.string.web_tab_helmet, Res.string.web_tab_individual, Res.string.limits_connect_a_helmet_to_set_its_limits,
        Res.string.limits_saved_for_this_helmet, Res.string.protocol_composer_save,
    )

    /** The English text, read in composition: that is where the resource environment is defined. */
    private var strings: Map<StringResource, String> = emptyMap()

    private fun text(r: StringResource): String = strings.getValue(r)

    private fun androidx.compose.ui.test.ComposeUiTest.show(content: @androidx.compose.runtime.Composable () -> Unit) =
        setContent {
            strings = used.associateWith { stringResource(it) }
            content()
        }

    // @verifies REQ-NPPS-12
    @Test
    fun helmetTier_needsAConnectedHub_andSavesUnderItsIdentity() = runComposeUiTest {
        val store = NPLimitsStore(InMemoryKeyValueStore())
        var hub: String? = null
        show { LimitsSettingsScreen(store, hubId = hub, onDone = {}) }

        onNodeWithText(text(Res.string.web_tab_helmet)).performClick()
        onNodeWithText(text(Res.string.limits_connect_a_helmet_to_set_its_limits)).assertExists()
        assertNull(store.helmetLimits["hub-1"])
    }

    // @verifies REQ-NPPS-12
    @Test
    fun helmetTier_withAHub_savesThatHubsLimits() = runComposeUiTest {
        val store = NPLimitsStore(InMemoryKeyValueStore())
        show { LimitsSettingsScreen(store, hubId = "hub-1", onDone = {}) }

        onNodeWithText(text(Res.string.web_tab_helmet)).performClick()
        onNodeWithText(text(Res.string.limits_saved_for_this_helmet)).assertExists()
        onNodeWithText(text(Res.string.protocol_composer_save)).performClick()
        assertEquals("hub-1", store.helmetLimits["hub-1"]?.helmetId)
    }

    // @verifies REQ-NPPS-12
    @Test
    fun individualTier_listsProfiles_andSavesUnderTheChosenOne() = runComposeUiTest {
        val store = NPLimitsStore(InMemoryKeyValueStore())
        val alex = NPIndividualProfile(name = "Alex")
        store.saveProfile(alex)
        show { LimitsSettingsScreen(store, hubId = null, onDone = {}) }

        onNodeWithText(text(Res.string.web_tab_individual)).performClick()
        onNodeWithText("Alex").assertExists()
        onNodeWithText(text(Res.string.protocol_composer_save)).performClick()
        assertEquals(alex.id, store.individualLimits.keys.single())
    }
}
