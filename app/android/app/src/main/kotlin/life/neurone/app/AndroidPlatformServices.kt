package life.neurone.app

import android.Manifest
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.shared.NoOpAnalyticsBackend
import life.neurone.shared.PlatformServices
import life.neurone.shared.ble.BleCentral

/** The Android half of the cross-platform seam (app/NeurOneUI/shared PlatformServices). */
class AndroidPlatformServices(
    override val keyValueStore: KeyValueStore,
    override val bleCentral: BleCentral,
    override val analyticsBackend: AnalyticsBackend = NoOpAnalyticsBackend(),
) : PlatformServices {

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
