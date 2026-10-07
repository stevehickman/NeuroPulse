package life.neurone.web

import androidx.compose.runtime.Composable
import kotlinx.browser.localStorage
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.shared.NoOpAnalyticsBackend
import life.neurone.shared.PlatformServices
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.UnavailableBleCentral

/**
 * The browser half of the cross-platform seam.
 *
 * Web Bluetooth is not wired yet, so the hub link is [UnavailableBleCentral] (OI-UI-KMP-03). Storage is
 * `localStorage`, which a user can clear; the app treats that as a fresh install, as it would any other
 * lost storage.
 */
class WebPlatformServices(
    override val keyValueStore: KeyValueStore = LocalStorageKeyValueStore(),
    override val bleCentral: BleCentral = UnavailableBleCentral(),
    override val analyticsBackend: AnalyticsBackend = NoOpAnalyticsBackend(),
) : PlatformServices {

    @Composable
    override fun rememberBleConnectAction(): () -> Unit = { bleCentral.refresh() }
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
