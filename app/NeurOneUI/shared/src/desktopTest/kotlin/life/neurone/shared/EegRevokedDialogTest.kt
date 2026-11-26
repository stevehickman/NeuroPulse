package life.neurone.shared

import androidx.compose.runtime.Composable
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.runBlocking
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.migration.ObjectStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.resources.*
import life.neurone.shared.session.UhdrFolder
import life.neurone.shared.ui.EegRevokedDialog
import life.neurone.shared.ui.OnboardingKeys
import life.neurone.shared.ui.SettingsScreen
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse

/**
 * What stays after the EEG release is withdrawn, and the offer to delete it (`REQ-SW03-37`; NP-PRIV-ANALYSIS-006 F-16): the withdrawal
 * deletes nothing by itself, the person is told what stays, and the recordings on this device go only when the person asks.
 */
@OptIn(ExperimentalTestApi::class)
class EegRevokedDialogTest {
    private val used = listOf(
        Res.string.dashboard_revoke_button, Res.string.eeg_revoked_title, Res.string.eeg_revoked_body,
        Res.string.eeg_revoked_delete_button, Res.string.eeg_revoked_keep_button, Res.string.eeg_revoked_deleted,
        Res.string.eeg_revoked_delete_failed, Res.string.common_close,
    )
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)

    private class Folder(override val store: ObjectStore = MemoryObjectStore()) : UhdrFolder {
        override fun protect(name: String) {}
    }

    /** A folder that lists its files and refuses to delete them. */
    private class StuckStore(private val inner: MemoryObjectStore = MemoryObjectStore()) : ObjectStore by inner {
        override suspend fun delete(name: String) = error("refused")
    }

    private fun folderWith(store: ObjectStore = MemoryObjectStore(), vararg names: String) = Folder(store).also { f ->
        runBlocking { names.forEach { f.store.put(it, byteArrayOf(1)) } }
    }

    private fun names(folder: UhdrFolder) = runBlocking { folder.store.list("") }.sorted()

    private fun services(kv: InMemoryKeyValueStore, folder: UhdrFolder?) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val uhdrFolder = folder
            @Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    // @verifies REQ-SW03-37#a
    @Test
    fun withdrawingTheReleaseSaysWhatStaysAndDeletesNothingByItself() = runComposeUiTest {
        val kv = InMemoryKeyValueStore().apply { putBoolean(OnboardingKeys.BIPA_ACCEPTED, true) }
        val folder = folderWith(names = arrayOf("session_101.edf", "session_102.edf"))
        val app = services(kv, folder)
        setContent {
            strings = used.associateWith { stringResource(it) }
            SettingsScreen(app)
        }
        onNodeWithText(text(Res.string.dashboard_revoke_button)).performScrollTo().performClick()
        waitForIdle()
        assertFalse(kv.getBoolean(OnboardingKeys.BIPA_ACCEPTED), "the release is withdrawn")
        onNodeWithText(text(Res.string.eeg_revoked_title)).assertExists()
        onNodeWithText(text(Res.string.eeg_revoked_body)).assertExists()
        onNodeWithText(text(Res.string.eeg_revoked_keep_button)).performClick()
        waitForIdle()
        onNodeWithText(text(Res.string.eeg_revoked_title)).assertDoesNotExist()
        assertEquals(listOf("session_101.edf", "session_102.edf"), names(folder), "keeping them deletes nothing")
    }

    // @verifies REQ-SW03-37#b
    @Test
    fun deletingRemovesTheRecordingsOnThisDeviceAndNothingElse() = runComposeUiTest {
        val folder = folderWith(names = arrayOf("session_101.edf", "session_102.edf", "notes.txt", "session_x.edf"))
        setContent {
            strings = used.associateWith { stringResource(it) }
            EegRevokedDialog(folder, onDismiss = {})
        }
        waitForIdle()
        onNodeWithText(text(Res.string.eeg_revoked_delete_button)).performClick()
        waitForIdle()
        onNodeWithText(text(Res.string.eeg_revoked_deleted)).assertExists()
        assertEquals(listOf("notes.txt", "session_x.edf"), names(folder), "only files named as a hub session's recording are deleted")
    }

    // @verifies REQ-SW03-37#b
    @Test
    fun withNoRecordingsOnThisDeviceNothingIsOfferedForDeletion() = runComposeUiTest {
        setContent {
            strings = used.associateWith { stringResource(it) }
            EegRevokedDialog(folderWith(names = arrayOf("notes.txt")), onDismiss = {})
        }
        waitForIdle()
        onNodeWithText(text(Res.string.eeg_revoked_body)).assertExists()
        onNodeWithText(text(Res.string.eeg_revoked_delete_button)).assertDoesNotExist()
        onNodeWithText(text(Res.string.common_close)).assertExists()
    }

    // @verifies REQ-SW03-37#b
    @Test
    fun aRecordingTheFolderWillNotDeleteIsReportedAsStillThere() = runComposeUiTest {
        val folder = folderWith(StuckStore(), "session_101.edf")
        setContent {
            strings = used.associateWith { stringResource(it) }
            EegRevokedDialog(folder, onDismiss = {})
        }
        waitForIdle()
        onNodeWithText(text(Res.string.eeg_revoked_delete_button)).performClick()
        waitForIdle()
        onNodeWithText(text(Res.string.eeg_revoked_delete_failed)).assertExists()
        onNodeWithText(text(Res.string.eeg_revoked_deleted)).assertDoesNotExist()
    }
}
