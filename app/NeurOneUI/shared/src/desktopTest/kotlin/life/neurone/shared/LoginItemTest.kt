package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import androidx.compose.runtime.Composable
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.login_item_option
import life.neurone.shared.ui.AutomaticBackupStatus
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** Starting the desktop app at sign-in: only by choice, only while the automatic backup is on, and gone with it (`REQ-SW03-28`). */
@OptIn(ExperimentalTestApi::class)
class LoginItemTest {

    private class Item(var on: Boolean = false) : LoginItem {
        val calls = mutableListOf<Boolean>()
        override val isOn: Boolean get() = on
        override fun set(on: Boolean): Boolean { calls += on; this.on = on; return true }
    }

    private fun services(item: Item, kv: InMemoryKeyValueStore = InMemoryKeyValueStore()) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val loginItem: LoginItem = item
            @Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    private fun secret() = BundleKeyDerivation.newRecoveryCode(SystemRandomSource).secret

    // @verifies REQ-SW03-28#a
    @Test fun nothing_turns_it_on_by_itself() {
        val item = Item()
        val app = services(item)
        app.scheduledBackup.turnOn(BundleKeyMethod.RECOVERY_CODE, secret())
        assertTrue(item.calls.isEmpty(), "turning the backup on does not start the app at sign-in")
        assertFalse(item.isOn)
    }

    // @verifies REQ-SW03-28#c
    @Test fun turning_the_backup_off_turns_the_start_at_sign_in_off() {
        val item = Item()
        val app = services(item)
        app.scheduledBackup.turnOn(BundleKeyMethod.RECOVERY_CODE, secret())
        item.on = true  // the person turns it on while the backup is on
        assertTrue(item.isOn, "while the backup is on it is left as the person set it")
        app.scheduledBackup.turnOff()
        assertEquals(listOf(false), item.calls)
        assertFalse(item.isOn)
    }

    // @verifies REQ-SW03-28#c
    @Test fun an_item_left_on_while_the_backup_is_off_is_turned_off_at_launch() {
        val item = Item(on = true)
        services(item)
        assertFalse(item.isOn)
    }

    // @verifies REQ-SW03-28#b
    @Test fun the_choice_is_not_offered_while_the_backup_is_off() = runComposeUiTest {
        val app = services(Item())
        var option = ""
        setContent {
            option = stringResource(Res.string.login_item_option)
            AutomaticBackupStatus(app)
        }
        waitForIdle()
        onNodeWithText(option).assertDoesNotExist()
    }

    // @verifies REQ-SW03-28#a
    // @verifies REQ-SW03-28#b
    @Test fun the_choice_is_offered_while_the_backup_is_on_and_stays_off_until_it_is_turned_on() = runComposeUiTest {
        val item = Item()
        val app = services(item)
        app.scheduledBackup.turnOn(BundleKeyMethod.RECOVERY_CODE, secret())
        var option = ""
        setContent {
            option = stringResource(Res.string.login_item_option)
            AutomaticBackupStatus(app)
        }
        waitForIdle()
        onNodeWithText(option).assertExists()
        assertFalse(item.isOn)
        assertTrue(item.calls.isEmpty())
    }

    // @verifies REQ-SW03-28#d
    @Test fun a_start_at_sign_in_hides_the_window_only_when_a_tray_can_bring_it_back() {
        val args = listOf(LoginItemFiles.BACKGROUND_ARGUMENT)
        assertTrue(LoginItemFiles.startsInTray(args, trayAvailable = true))
        assertFalse(LoginItemFiles.startsInTray(args, trayAvailable = false), "no tray: the window opens")
        assertFalse(LoginItemFiles.startsInTray(emptyList(), trayAvailable = true), "opened by hand: the window opens")
    }

    // @verifies REQ-SW03-28#e
    @Test fun the_entries_start_the_installed_app_in_the_background_with_its_path_quoted() {
        val path = "/Applications/Neur One & Co/NeurOne \"x\" \$HOME"
        val plist = LoginItemFiles.macPlist(path)
        assertTrue("<string>/Applications/Neur One &amp; Co/NeurOne &quot;x&quot; \$HOME</string><string>--background</string>" in plist, plist)
        assertTrue("<key>RunAtLoad</key><true/>" in plist)

        val entry = LoginItemFiles.linuxEntry(path)
        assertTrue("Exec=\"/Applications/Neur One & Co/NeurOne \\\"x\\\" \\\$HOME\" --background" in entry, entry)

        assertEquals("\"C:\\Program Files\\NeurOne\\NeurOne.exe\" --background", LoginItemFiles.windowsCommand("C:\\Program Files\\NeurOne\\NeurOne.exe"))
    }
}

