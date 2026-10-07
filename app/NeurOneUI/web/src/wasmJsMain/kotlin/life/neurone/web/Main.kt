package life.neurone.web

import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.window.ComposeViewport
import kotlinx.browser.document
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import life.neurone.core.npps.initNppsCore
import life.neurone.shared.AppServices
import life.neurone.shared.createAppScope
import life.neurone.shared.ui.NeurOneApp

@OptIn(ExperimentalComposeUiApi::class)
fun main() {
    MainScope().launch {
        // The NPPS core is a WebAssembly module a browser cannot compile synchronously, so it loads first.
        initNppsCore("neurone_npps.wasm")
        val services = AppServices(WebPlatformServices(), createAppScope())
        ComposeViewport(document.body!!) {
            NeurOneApp(services)
        }
    }
}
