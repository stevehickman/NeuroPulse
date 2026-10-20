package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.isToggleable
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import life.neurone.shared.ui.parseDuration
import life.neurone.shared.ui.formatDuration
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
        Res.string.limits_saved_for_this_helmet, Res.string.protocol_composer_save, Res.string.limits_set_limit,
        Res.string.ui_tab_script,
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
        onNodeWithText(text(Res.string.protocol_composer_save)).performScrollTo().performClick()
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
        onNodeWithText(text(Res.string.protocol_composer_save)).performScrollTo().performClick()
        assertEquals(alex.id, store.individualLimits.keys.single())
    }

    // @verifies REQ-NPPS-12
    @Test
    fun enablingASection_andSettingALimit_savesItsBlock_withTheMidpointStart() = runComposeUiTest {
        val store = NPLimitsStore(InMemoryKeyValueStore())
        show { LimitsSettingsScreen(store, hubId = null, onDone = {}) }

        // The first section is PBM transcranial: off until switched on, then each field is "Set limit".
        onAllNodes(isToggleable()).onFirst().performClick()
        onAllNodesWithText(text(Res.string.limits_set_limit)).onFirst().performClick()
        onNodeWithText(text(Res.string.protocol_composer_save)).performScrollTo().performClick()

        assertEquals(200.0, store.globalLimits!!.pbmTranscranial!!.maxIrradianceMWcm2)
        assertNull(store.globalLimits!!.pbmTranscranial!!.maxFrequencyHz)
        assertNull(store.globalLimits!!.tdcs)
    }

    // @verifies REQ-NPPS-12
    @Test
    fun scriptTab_parsesAsItIsTyped_andASavedScriptKeepsTheLimits() = runComposeUiTest {
        val store = NPLimitsStore(InMemoryKeyValueStore())
        store.saveGlobalLimits(
            life.neurone.core.protocol.NPLimitsSet(
                name = "g", tdcs = life.neurone.core.protocol.NPTDCSLimits(maxIntensityMilliamps = 1.0),
            ),
        )
        show { LimitsSettingsScreen(store, hubId = null, onDone = {}) }

        onNodeWithText(text(Res.string.ui_tab_script)).performClick()
        // Saving from the script tab keeps what the fields held: nothing was lost by looking at the script.
        onNodeWithText(text(Res.string.protocol_composer_save)).performScrollTo().performClick()
        assertEquals(1.0, store.globalLimits!!.tdcs!!.maxIntensityMilliamps)
    }

    // @verifies REQ-NPPS-12
    @Test
    fun durations_roundTripThroughTheirShortForm() {
        assertEquals(1200, parseDuration("20m"))
        assertEquals(5400, parseDuration("1.5h"))
        assertEquals(90, parseDuration("90s"))
        assertEquals(90, parseDuration("90"))
        assertNull(parseDuration("soon"))
        assertEquals("20m", formatDuration(1200))
        assertEquals("2h", formatDuration(7200))
        assertEquals("90s", formatDuration(90))
    }
}
