package life.neurone.web

import androidx.compose.runtime.Composable
import kotlinx.browser.localStorage
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.core.session.ProtocolSigner
import life.neurone.shared.NoOpAnalyticsBackend
import life.neurone.shared.PlatformServices
import life.neurone.shared.ble.BleCentral

/**
 * The browser half of the cross-platform seam.
 *
 * The hub link is Web Bluetooth ([WebBleCentral]). Storage is
 * `localStorage`, which a user can clear; the app treats that as a fresh install, as it would any other
 * lost storage.
 */
class WebPlatformServices(
    override val keyValueStore: KeyValueStore = LocalStorageKeyValueStore(),
    override val bleCentral: BleCentral = WebBleCentral(),
    override val protocolSigner: ProtocolSigner = Ed25519ProtocolSigner(),
    override val analyticsBackend: AnalyticsBackend = NoOpAnalyticsBackend(),
) : PlatformServices {

    // The chooser must open inside the click, so the action is the central's own, not a generic refresh.
    @Composable
    override fun rememberBleConnectAction(): () -> Unit = { (bleCentral as? WebBleCentral)?.requestDevice() }
}

class LocalStorageKeyValueStore(private val prefix: String = "np-app:") : KeyValueStore {
    override fun getString(key: String): String? = localStorage.getItem(prefix + key)
    override fun putString(key: String, value: String) = localStorage.setItem(prefix + key, value)
    override fun getInt(key: String, default: Int): Int = getString(key)?.toIntOrNull() ?: default
    override fun putInt(key: String, value: Int) = putString(key, value.toString())
    override fun getBoolean(key: String, default: Boolean): Boolean = getString(key)?.toBooleanStrictOrNull() ?: default
    override fun putBoolean(key: String, value: Boolean) = putString(key, value.toString())
    override fun remove(key: String) = localStorage.removeItem(prefix + key)
}
