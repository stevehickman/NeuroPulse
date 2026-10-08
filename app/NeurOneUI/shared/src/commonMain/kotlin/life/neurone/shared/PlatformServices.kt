package life.neurone.shared

import androidx.compose.runtime.Composable
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.session.ProtocolSigner
import life.neurone.shared.ble.BleCentral

/**
 * Everything the shared app needs from the platform it runs on, and nothing else.
 *
 * This interface is the whole seam. A platform host (Android activity, desktop window, browser
 * page, iOS view controller) implements it and hands it to [AppServices]; no shared code reaches
 * past it. Keeping it small is the point: every member is a place where behaviour can differ
 * between platforms, so each one has to earn its place. Add a member only when no common
 * implementation exists, and prefer a type from `:core` over a new one here.
 */
interface PlatformServices {

    /** Durable, app-private key-value storage. Excluded from any cloud or device backup. */
    val keyValueStore: KeyValueStore

    /** The Bluetooth LE central that talks to the hub. */
    val bleCentral: BleCentral

    /**
     * Signs the session descriptor the hub will run (Ed25519 over its signed region, NP-FW-HUB-001 §4.4). The hub
     * rejects an unsigned or corrupted protocol, so a host must supply one: the JDK's on the JVM targets, and the common
     * `Ed25519ProtocolSigner` on iOS and the browser, where no synchronous platform signer exists.
     */
    val protocolSigner: ProtocolSigner

    /**
     * Analytics vendor. Reached only through the consent-gated analytics gates in `:core`; a host
     * with no vendor passes [NoOpAnalyticsBackend].
     */
    val analyticsBackend: AnalyticsBackend

    /**
     * An action that asks the operating system for whatever it needs before [bleCentral] may scan
     * (Android's runtime Bluetooth permissions) and then re-reads the adapter state. Composable
     * because Android registers its permission launcher with the composition; a platform with
     * nothing to ask for returns an action that only refreshes the central.
     */
    @Composable
    fun rememberBleConnectAction(): () -> Unit
}

/** The analytics backend until the production vendor is selected and its DPA executed (NP-PRIV-AUDIT-001 HIGH-1). */
class NoOpAnalyticsBackend : AnalyticsBackend {
    override fun configure() {}
    override fun reset() {}
    override fun track(event: String, properties: Map<String, String>) {}
}
