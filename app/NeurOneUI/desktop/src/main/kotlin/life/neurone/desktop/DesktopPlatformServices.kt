package life.neurone.desktop

import androidx.compose.runtime.Composable
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.session.JvmProtocolSigner
import life.neurone.core.session.ProtocolSigner
import life.neurone.shared.NoOpAnalyticsBackend
import life.neurone.shared.PlatformServices
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.UnavailableBleCentral
import java.io.File
import java.util.Properties

/**
 * The macOS / Windows / Linux half of the cross-platform seam.
 *
 * Bluetooth is not wired: the JDK has no BLE API, so the hub link is [UnavailableBleCentral] until a
 * desktop central is chosen and written (OI-UI-KMP-03). Everything that does not need the hub works.
 */
class DesktopPlatformServices(
    override val keyValueStore: KeyValueStore = FileKeyValueStore(appDataDirectory().resolve("np-app.properties")),
    override val bleCentral: BleCentral = UnavailableBleCentral(),
    override val protocolSigner: ProtocolSigner = JvmProtocolSigner(),
    override val analyticsBackend: AnalyticsBackend = NoOpAnalyticsBackend(),
) : PlatformServices {

    @Composable
    override fun rememberBleConnectAction(): () -> Unit = { bleCentral.refresh() }
}

/** The per-user application-data directory of the operating system the app is running on. */
fun appDataDirectory(
    os: String = System.getProperty("os.name").lowercase(),
    home: String = System.getProperty("user.home"),
    env: (String) -> String? = System::getenv,
): File = when {
    "mac" in os -> File(home, "Library/Application Support/NeurOne")
    "win" in os -> File(env("APPDATA") ?: "$home/AppData/Roaming", "NeurOne")
    else -> File(env("XDG_DATA_HOME") ?: "$home/.local/share", "NeurOne")
}

/**
 * [KeyValueStore] in one properties file, written whole to a temporary file and renamed over the
 * real one so a crash mid-write leaves the previous contents, not half of them. Every put persists
 * at once: this store holds consent decisions, and a decision the user made must survive the
 * process being killed a moment later.
 */
class FileKeyValueStore(private val file: File) : KeyValueStore {
    private val properties = Properties()

    init {
        if (file.exists()) file.inputStream().use { properties.load(it) }
    }

    @Synchronized private fun persist() {
        file.parentFile?.mkdirs()
        val temporary = File(file.parentFile, file.name + ".tmp")
        temporary.outputStream().use { properties.store(it, null) }
        java.nio.file.Files.move(
            temporary.toPath(), file.toPath(),
            java.nio.file.StandardCopyOption.REPLACE_EXISTING, java.nio.file.StandardCopyOption.ATOMIC_MOVE,
        )
    }

    @Synchronized override fun getString(key: String): String? = properties.getProperty(key)
    @Synchronized override fun putString(key: String, value: String) {
        properties.setProperty(key, value); persist()
    }
    @Synchronized override fun getInt(key: String, default: Int): Int = properties.getProperty(key)?.toIntOrNull() ?: default
    @Synchronized override fun putInt(key: String, value: Int) = putString(key, value.toString())
    @Synchronized override fun getBoolean(key: String, default: Boolean): Boolean =
        properties.getProperty(key)?.toBooleanStrictOrNull() ?: default
    @Synchronized override fun putBoolean(key: String, value: Boolean) = putString(key, value.toString())
    @Synchronized override fun remove(key: String) {
        properties.remove(key); persist()
    }
}
