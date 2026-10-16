package life.neurone.app

import android.Manifest
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
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
) : PlatformServices {

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
