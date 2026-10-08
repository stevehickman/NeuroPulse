package life.neurone.shared

import androidx.compose.runtime.Composable
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.core.session.ProtocolSigner
import life.neurone.shared.ble.BleCentral
import platform.Foundation.NSUserDefaults

/**
 * The iOS half of the cross-platform seam.
 *
 * The hub link is CoreBluetooth ([IosBleCentral]). The Swift app's own BLECentralManager remains the production
 * path until this build has run on a device.
 */
class IosPlatformServices(
    override val keyValueStore: KeyValueStore = UserDefaultsKeyValueStore(),
    override val bleCentral: BleCentral = IosBleCentral(),
    override val protocolSigner: ProtocolSigner = Ed25519ProtocolSigner(),
    override val analyticsBackend: AnalyticsBackend = NoOpAnalyticsBackend(),
) : PlatformServices {

    @Composable
    override fun rememberBleConnectAction(): () -> Unit = { bleCentral.refresh() }
}

/** [KeyValueStore] over `NSUserDefaults`. An absent key reads as the default, never as zero or false. */
class UserDefaultsKeyValueStore(
    private val defaults: NSUserDefaults = NSUserDefaults.standardUserDefaults,
) : KeyValueStore {
    override fun getString(key: String): String? = defaults.stringForKey(key)
    override fun putString(key: String, value: String) = defaults.setObject(value, forKey = key)
    override fun getInt(key: String, default: Int): Int =
        if (defaults.objectForKey(key) == null) default else defaults.integerForKey(key).toInt()
    override fun putInt(key: String, value: Int) = defaults.setInteger(value.toLong(), forKey = key)
    override fun getBoolean(key: String, default: Boolean): Boolean =
        if (defaults.objectForKey(key) == null) default else defaults.boolForKey(key)
    override fun putBoolean(key: String, value: Boolean) = defaults.setBool(value, forKey = key)
    override fun remove(key: String) = defaults.removeObjectForKey(key)
}
