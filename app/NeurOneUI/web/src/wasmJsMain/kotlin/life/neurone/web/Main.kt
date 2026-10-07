package life.neurone.web

import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.window.ComposeViewport
import kotlinx.browser.document
import life.neurone.shared.AppServices
import life.neurone.shared.createAppScope
import life.neurone.shared.ui.NeurOneApp

@OptIn(ExperimentalComposeUiApi::class)
fun main() {
    val services = AppServices(WebPlatformServices(), createAppScope())
    ComposeViewport(document.body!!) {
        NeurOneApp(services)
    }
}
