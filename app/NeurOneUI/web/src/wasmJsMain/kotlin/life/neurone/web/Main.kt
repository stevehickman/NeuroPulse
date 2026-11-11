package life.neurone.web

import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.window.ComposeViewport
import kotlinx.browser.document
import kotlinx.browser.window
import life.neurone.shared.ble.SimulatedBleCentral
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import life.neurone.core.npps.initNppsCore
import life.neurone.shared.AppServices
import life.neurone.shared.createAppScope
import life.neurone.shared.ui.NeurOneApp

/**
 * The host for this page. `?hub=sim` runs the app against the simulator's twin (NP-SIM-FW-001 §27) over a WebSocket to the simulator
 * server; `&bridge=ws://127.0.0.1:9000` names it (9000 is the server's port). Only a loopback address is taken (`OI-SIMFW-07`: the server
 * binds to 127.0.0.1), so a link cannot send the app's traffic to another machine. Without `hub=sim` this is the real-hub host.
 */
private fun platformForLocation(): WebPlatformServices {
    val query = window.location.search.removePrefix("?").split("&").mapNotNull { p -> p.split("=", limit = 2).let { if (it.size == 2) it[0] to it[1] else null } }.toMap()
    if (query["hub"] != "sim") return WebPlatformServices()
    val bridge = query["bridge"]?.takeIf { Regex("ws://(127\\.0\\.0\\.1|localhost):[0-9]{1,5}").matches(it) } ?: "ws://127.0.0.1:9000"
    return WebPlatformServices(bleCentral = SimulatedBleCentral(WebSocketHubBridge(bridge)), simulatedHub = true)
}

@OptIn(ExperimentalComposeUiApi::class)
fun main() {
    MainScope().launch {
        // The NPPS core is a WebAssembly module a browser cannot compile synchronously, so it loads first.
        initNppsCore("neurone_npps.wasm")
        val services = AppServices(platformForLocation(), createAppScope())
        ComposeViewport(document.body!!) {
            NeurOneApp(services)
        }
    }
}
