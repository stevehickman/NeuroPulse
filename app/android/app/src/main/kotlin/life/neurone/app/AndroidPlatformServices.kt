package life.neurone.app

import life.neurone.core.migration.LinkTransport
import life.neurone.shared.migration.LanLinkTransport
import android.Manifest
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.SecretKeyStore
import life.neurone.core.migration.FileStore
import life.neurone.core.session.ProtocolSigner
import life.neurone.shared.NoOpAnalyticsBackend
import life.neurone.shared.PlatformServices
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.migration.FileTransfer

/** The Android half of the cross-platform seam (app/NeurOneUI/shared PlatformServices). */
class AndroidPlatformServices(
    override val keyValueStore: KeyValueStore,
    override val bleCentral: BleCentral,
    override val protocolSigner: ProtocolSigner,
    override val analyticsBackend: AnalyticsBackend = NoOpAnalyticsBackend(),
    override val secretKeyStore: SecretKeyStore? = null,
    /** The app's own files for the clear to overwrite (SWR-MG-06); see [NeurOneApplication]'s `ownFiles`. */
    override val migrationFiles: FileStore? = null,
    /** The network leg of analytics and the erasure request (REQ-SW03-19, REQ-SW03-20). */
    override val httpTransport: life.neurone.core.net.HttpTransport? = null,
    /** The analytics host and project token this build was made with; null or unusable means analytics is off. */
    override val analyticsConfig: life.neurone.core.analytics.AnalyticsConfig? = null,
    /** Where the hub leaves the SHDR record: a file in the app's private folder (REQ-SHDR-01). */
    override val shdrStaging: life.neurone.core.analytics.ShdrStaging? = null,
    /** The folder a backup is kept in, once the person has chosen one (SWR-MG-10); see [AndroidFolderPlaces]. */
    override val folderPlaces: AndroidFolderPlaces? = null,
    /** The system job that wakes the app for the automatic backup (SWR-MG-03); see [AndroidBackgroundBackupScheduler]. */
    override val backgroundBackup: life.neurone.shared.migration.BackgroundBackupScheduler? = null,
) : PlatformServices {

    /** OkHttp with the relay's SPKI pins (REQ-SHDR-04). */
    override fun fleetTransport(relayHost: String, pins: List<String>): life.neurone.core.net.HttpTransport =
        OkHttpTransport(OkHttpTransport.pinnedClient(relayHost, pins))

    /** A direct move to another phone on the same Wi-Fi: sockets written once for every host that has them (SWR-MG-10). */
    override val linkTransport: LinkTransport = LanLinkTransport()

    /** A backup is a file the person saves and later opens, through the system's own dialogs (SAF). */
    @Composable
    override fun rememberFileTransfer(): FileTransfer {
        val context = LocalContext.current
        val transfer = remember(context) { AndroidFileTransfer(context) }
        val save = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) {
            transfer.onPicked(it)
        }
        val open = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { transfer.onPicked(it) }
        transfer.launchSave = { name -> save.launch(name) }
        transfer.launchOpen = { open.launch(arrayOf("*/*")) }
        // The folder dialog is registered here too: the backup and restore screens that offer the folder call this first.
        val tree = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { folderPlaces?.onPicked(it) }
        folderPlaces?.launchTree = { tree.launch(null) }
        return transfer
    }

    @Composable
    override fun rememberBleConnectAction(): () -> Unit {
        // Android 12+ asks for BLUETOOTH_SCAN/CONNECT at runtime; on grant, kick off scanning.
        val launcher = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { grants ->
            if (grants.values.all { it }) bleCentral.refresh()
        }
        return {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                launcher.launch(arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT))
            } else {
                bleCentral.refresh()
            }
        }
    }
}
