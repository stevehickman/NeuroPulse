package life.neurone.shared

import androidx.compose.ui.window.ComposeUIViewController
import life.neurone.shared.ui.NeurOneApp
import platform.UIKit.UIViewController

/** The one entry point the Swift host calls: the whole app, as a view controller. */
fun MainViewController(): UIViewController {
    val services = AppServices(IosPlatformServices(), createAppScope())
    return ComposeUIViewController { NeurOneApp(services) }
}
