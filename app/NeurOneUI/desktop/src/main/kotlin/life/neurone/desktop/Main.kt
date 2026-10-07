package life.neurone.desktop

import androidx.compose.runtime.remember
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import life.neurone.shared.AppServices
import life.neurone.shared.createAppScope
import life.neurone.shared.ui.NeurOneApp

fun main() = application {
    val services = remember { AppServices(DesktopPlatformServices(), createAppScope()) }
    Window(onCloseRequest = ::exitApplication, title = "NeurOne") {
        NeurOneApp(services)
    }
}
