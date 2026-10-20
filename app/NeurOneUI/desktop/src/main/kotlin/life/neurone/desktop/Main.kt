package life.neurone.desktop

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.window.Tray
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.window.isTraySupported
import life.neurone.shared.AppServices
import life.neurone.shared.createAppScope
import life.neurone.shared.ui.NeurOneApp
import life.neurone.shared.ui.trayText

fun main() = application {
    val services = remember { AppServices(DesktopPlatformServices(), createAppScope()) }
    var windowShown by remember { mutableStateOf(true) }
    // Closing the window ends the app, except while the automatic backup is on: the app then stays in the tray so the backup keeps
    // being made (SWR-MG-03). The tray has the one way back and the one way out.
    Window(
        onCloseRequest = { if (keepsRunningInTray(isTraySupported, services.scheduledBackup.isOn)) windowShown = false else exitApplication() },
        visible = windowShown,
        title = "NeurOne",
    ) {
        NeurOneApp(services)
    }
    if (isTraySupported) {
        val text = trayText()
        Tray(
            icon = remember { TrayIcon },
            tooltip = text.tooltip,
            onAction = { windowShown = true },
            menu = {
                Item(text.open, onClick = { windowShown = true })
                Item(text.quit, onClick = ::exitApplication)
            },
        )
    }
}

/** Whether closing the window leaves the app running: only where there is a tray to bring it back from, and only while the backup is on. */
internal fun keepsRunningInTray(trayAvailable: Boolean, backupOn: Boolean): Boolean = trayAvailable && backupOn
