package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.NPIntervalConfig
import life.neurone.core.protocol.NPLimitsSet
import life.neurone.core.protocol.NPModalityParams
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.NPProtocolModality
import life.neurone.core.protocol.NPTDCSParams
import life.neurone.core.protocol.NPTimingMode
import life.neurone.shared.migration.FileTransfer
import life.neurone.shared.resources.*
import life.neurone.shared.ui.ProtocolMenuScreen
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** A person keeps their protocols as one `.npps` file and opens it again, on any host with save and open dialogs (REQ-NPPS-18). */
@OptIn(ExperimentalTestApi::class)
class ProtocolFileTest {

    private class FakeTransfer(var toOpen: ByteArray? = null) : FileTransfer {
        var savedName: String? = null
        var saved: ByteArray? = null
        override suspend fun save(name: String, bytes: ByteArray): Boolean { savedName = name; saved = bytes; return true }
        override suspend fun open(): ByteArray? = toOpen
    }

    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)
    private val used = listOf(Res.string.web_protocols_backup_save, Res.string.web_protocols_backup_open, Res.string.web_protocols_backup_none)

    private fun mine(name: String) = NPProtocolEntry.Single(
        NPProtocolDefinition(
            name = name, timingMode = NPTimingMode.Duration(600),
            modalities = listOf(
                NPProtocolModality(params = NPModalityParams.Tdcs(NPTDCSParams(intensityMilliamps = 1.0)), interval = NPIntervalConfig.CONTINUOUS),
            ),
        ),
    )

    private fun androidx.compose.ui.test.ComposeUiTest.show(library: NPProtocolLibrary, transfer: FileTransfer?) = setContent {
        strings = used.associateWith { stringResource(it) }
        ProtocolMenuScreen(
            library = library, consentGranted = true, eegUnavailableMessage = "", limits = NPLimitsSet(name = "none"),
            onSelect = {}, onBack = {}, fileTransfer = transfer,
        )
    }

    // @verifies REQ-NPPS-18
    @Test
    fun savingOffersTheUsersProtocolsAsOneFile_andNothingToSaveSaysSo() = runComposeUiTest {
        val library = NPProtocolLibrary(InMemoryKeyValueStore())
        val transfer = FakeTransfer()
        show(library, transfer)

        onNodeWithText(text(Res.string.web_protocols_backup_save)).performClick()
        onNodeWithText(text(Res.string.web_protocols_backup_none)).assertExists()
        assertEquals(null, transfer.saved)

        library.save(mine("Mine"))
        onNodeWithText(text(Res.string.web_protocols_backup_save)).performClick()
        waitForIdle()
        assertEquals("neurone-protocols.npps", transfer.savedName)
        assertTrue(transfer.saved!!.decodeToString().contains("Mine"))
    }

    // @verifies REQ-NPPS-18
    @Test
    fun openingAFileAddsItsProtocols_andNoFileChangesNothing() = runComposeUiTest {
        val source = NPProtocolLibrary(InMemoryKeyValueStore()).also { it.save(mine("From a file")) }
        val bytes = source.exportAllScript()!!.encodeToByteArray()

        val library = NPProtocolLibrary(InMemoryKeyValueStore())
        val transfer = FakeTransfer(toOpen = null)
        show(library, transfer)

        // The person cancels the dialog: nothing happens.
        onNodeWithText(text(Res.string.web_protocols_backup_open)).performClick()
        waitForIdle()
        assertEquals(0, library.userProtocols.size)

        transfer.toOpen = bytes
        onNodeWithText(text(Res.string.web_protocols_backup_open)).performClick()
        waitForIdle()
        assertEquals(listOf("From a file"), library.userProtocols.map { (it as NPProtocolEntry.Single).protocol.name })
    }

    // @verifies REQ-NPPS-18
    @Test
    fun aHostWithoutDialogs_showsNoSaveOrOpenButtons() = runComposeUiTest {
        show(NPProtocolLibrary(InMemoryKeyValueStore()), transfer = null)
        onNodeWithText(text(Res.string.web_protocols_backup_save)).assertDoesNotExist()
        onNodeWithText(text(Res.string.web_protocols_backup_open)).assertDoesNotExist()
    }
}
