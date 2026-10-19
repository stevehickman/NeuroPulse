package life.neurone.shared

import androidx.compose.runtime.Composable
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.SecretKeyStore
import life.neurone.core.migration.FileStore
import life.neurone.core.migration.LinkTransport
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

    /**
     * Durable, app-private key-value storage. Excluded from any cloud or device backup. A host with nowhere safe to keep a
     * person's records may hold some keys in memory only (the browser does: `MemoryOnlyKeysKeyValueStore`), so a key can read as
     * absent after a restart and every reader of it must treat absent as an empty start.
     */
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
     * Folders a backup may be kept in, chosen by the person, or null when the host has no choice to offer (a fixed
     * [migrationPlace], or the system's own dialogs on a phone). When it is supplied the screens take the place from the chosen
     * folder ([AppServices.backupPlace]) and offer the choice; [migrationPlace] is then not used.
     */
    val folderPlaces: life.neurone.shared.migration.FolderPlaces? get() = null

    /**
     * Where the key that protects the app's UHDR-class data is kept: the platform's own keystore (SWR-MG-06). Null when the
     * host has no keystore it can destroy a key in. The shared app then keeps its data unencrypted and says the device cannot
     * clear itself, because a clear that cannot destroy the key would only claim to protect what it leaves readable.
     */
    val secretKeyStore: SecretKeyStore? get() = null

    /**
     * Whether the person is told when the app holds this host's data unsealed (no usable keystore). True for a host that keeps
     * the data on disk and can lose its keystore between runs (the desktop apps); the browser keeps nothing of the protected
     * keys at rest, so a notice that the data is not sealed would say nothing true about it. A host whose keystore is always
     * there (a phone) never reaches the notice either way.
     */
    val reportsUnsealedStorage: Boolean get() = false

    /**
     * Whether this host keeps the app's state in memory only, so that it is gone when the page or the app ends (the browser:
     * it has no keystore to seal state in and its storage is readable by any script, REQ-SW03-18 (b)). The person is told once, above
     * every screen, so that the onboarding screens coming back and the settings that did not stay are not a surprise.
     */
    val holdsStateInMemoryOnly: Boolean get() = false

    /**
     * How this host reaches another phone on the local network for a direct move (SWR-MG-10), or null when it cannot (the
     * browser has no sockets). The sockets are written once for Android, the desktop apps and iOS (`LanLinkTransport`);
     * everything above them is the core's.
     */
    val linkTransport: LinkTransport? get() = null

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
     * Erases everything this host keeps for the app and starts it afresh, or null when the host has no such action to offer.
     * The browser has one: its storage is a page's, readable by any script on it, so the person is given a way to empty it
     * (the privacy review, Finding 2). A phone or a computer has the device-clear after a move (SWR-MG-06) and the system's own
     * "delete app data". The host does its own restart (the browser reloads the page); the shared screen only asks, after
     * the person has said they understand it cannot be undone.
     */
    val eraseLocalData: (() -> Unit)? get() = null

    /**
     * Ends this run of the app, for a host that keeps its state on disk and has a way to stop (the desktop apps). Having it is what
     * offers the person "erase this device" (SWR-MG-17): the erase deletes the files and the stored keys, and the run still holds
     * the old state in memory, so the app must end and be started again rather than carry on and write it back. Null for a host
     * that is not asked to (the browser has [eraseLocalData]; a phone's data goes with the app).
     */
    val endRunAfterErase: (() -> Unit)? get() = null

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
