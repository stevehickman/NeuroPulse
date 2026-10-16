package life.neurone.shared

import androidx.compose.runtime.Composable
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.migration.ClearActions
import life.neurone.core.migration.FileStore
import life.neurone.core.migration.ObjectStore
import life.neurone.core.migration.RouteDescriptor
import life.neurone.shared.migration.FileTransfer
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
     * Where this host can put a backup, or null when it has nowhere yet (the backup screen then says so). A member with a
     * default so that a host with no storage for it needs no code; the hosts that have a folder or an account override it.
     */
    val migrationPlace: MigrationPlace? get() = null

    /**
     * The two clear steps only the platform can do (SWR-MG-06, SWR-MG-09): destroy the key that protects the app's data, and
     * forget the helmet pairing record. Null when the host cannot do both truly, and the clear screen then says this device
     * cannot clear itself: a host must not report a step done that it did not do.
     */
    val clearActions: ClearActions? get() = null

    /** The app's own files, for the clear to overwrite and delete (SWR-MG-06), or null when the host holds none. */
    val migrationFiles: FileStore? get() = null

    /**
     * The save and open dialogs for a backup that is a file the person carries, or null when the host has none. A host with a
     * [migrationPlace] of its own needs no file; one without (a phone, a browser) supplies this, and the screens then make the
     * backup as a file ([life.neurone.shared.migration.PortableFilePlace]). Composable because Android registers its dialogs
     * with the composition.
     */
    @Composable
    fun rememberFileTransfer(): FileTransfer? = null

    /**
     * An action that asks the operating system for whatever it needs before [bleCentral] may scan
     * (Android's runtime Bluetooth permissions) and then re-reads the adapter state. Composable
     * because Android registers its permission launcher with the composition; a platform with
     * nothing to ask for returns an action that only refreshes the central.
     */
    @Composable
    fun rememberBleConnectAction(): () -> Unit
}

/**
 * A place a backup can go (SWR-MG-10): the six object calls of its [store], and the [route] descriptor that says what the
 * place can promise (an exclusive create, above all), which the core checks before it offers the place for a migration.
 * [label] names the place for the person, such as the folder; it is shown as given and is never a user's data.
 */
class MigrationPlace(val label: String, val store: ObjectStore, val route: RouteDescriptor)

/** The analytics backend until the production vendor is selected and its DPA executed (NP-PRIV-AUDIT-001 HIGH-1). */
class NoOpAnalyticsBackend : AnalyticsBackend {
    override fun configure() {}
    override fun reset() {}
    override fun track(event: String, properties: Map<String, String>) {}
}
