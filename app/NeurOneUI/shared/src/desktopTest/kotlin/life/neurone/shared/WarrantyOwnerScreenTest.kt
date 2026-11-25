package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertCountEquals
import androidx.compose.ui.test.assertIsOff
import androidx.compose.ui.test.assertIsOn
import androidx.compose.ui.test.onAllNodesWithContentDescription
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.familyKey
import life.neurone.core.protocol.name
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.resources.*
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
 * The Helmet owner screen's offered set (`SWR-WO-01`): the switch writes the owner's record for the connected helmet and never
 * the person's archive, and the screen says nothing about which protocols have been run (`OI-PRIV-60`, NP-PRIV-ANALYSIS-006 F-04).
 */
@OptIn(ExperimentalTestApi::class)
class WarrantyOwnerScreenTest {

    private val used = listOf(Res.string.warranty_owner_offered_switch, Res.string.warranty_owner_offered_no_helmet)
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

    private fun androidx.compose.ui.test.ComposeUiTest.show(app: AppServices) = setContent {
        strings = used.associateWith { stringResource(it) }
        WarrantyOwnerScreen(app, onDone = {})
    }

    // @verifies SWR-WO-01
    // @verifies REQ-SHDR-10#d
    @Test
    fun theOfferedSwitchWritesTheOwnersRecordForTheHelmetAndNotThePersonsArchive() = runComposeUiTest {
        val kv = InMemoryKeyValueStore()
        val app = services(kv)
        val gamma = app.protocolLibrary.bundledProtocols.first { it.name == "Gamma Focus" }
        app.gattManager.onConnected("hub-a")
        show(app)
        waitForIdle()

        val switch = onNodeWithContentDescription("${text(Res.string.warranty_owner_offered_switch)}: ${gamma.name}")
        switch.assertIsOn()
        switch.performClick()
        waitForIdle()
        switch.assertIsOff()

        assertEquals(setOf(gamma.familyKey()), app.warrantyOwnerStore.withdrawnFamilies("hub-a"))
        assertTrue(app.warrantyOwnerStore.withdrawnFamilies("hub-b").isEmpty(), "this helmet's choice only")
        assertNull(kv.getString(NPProtocolLibrary.ARCHIVED_FAMILIES_KEY), "the person's archive is not written")
        assertFalse(app.protocolLibrary.isArchived(gamma))
        assertFalse(app.protocolLibrary.offeredProtocols.any { it.id == gamma.id }, "and the person is not offered it on this helmet")

        switch.performClick()
        waitForIdle()
        switch.assertIsOn()
        assertTrue(app.warrantyOwnerStore.withdrawnFamilies("hub-a").isEmpty())
    }

    // @verifies SWR-WO-01
    @Test
    fun withNoHelmetThereIsNoOfferedSetToChange() = runComposeUiTest {
        val app = services(InMemoryKeyValueStore())
        show(app)
        waitForIdle()
        onNodeWithText(text(Res.string.warranty_owner_offered_no_helmet)).assertExists()
        onAllNodesWithContentDescription(text(Res.string.warranty_owner_offered_switch), substring = true).assertCountEquals(0)
    }

    // Which protocols the wearer has run is the wearer's (CLAUDE.md §5.1); the owner may be a clinic.
    // @verifies SWR-WO-01
    @Test
    fun theOwnerPageDoesNotSayWhichProtocolsHaveBeenRun() = runComposeUiTest {
        val app = services(InMemoryKeyValueStore())
        val library = app.protocolLibrary
        val gamma = library.bundledProtocols.first { it.name == "Gamma Focus" }
        val hash = assertNotNull(library.contentHash(gamma))
        library.recordUploadResult(gamma, hash, "accepted", hash)
        assertTrue(library.isUsed(gamma), "control: the family has been run")
        app.gattManager.onConnected("hub-a")
        show(app)
        waitForIdle()
        onNodeWithContentDescription("${text(Res.string.warranty_owner_offered_switch)}: ${gamma.name}").assertExists()
        onAllNodesWithText("Used").assertCountEquals(0)
    }
}
