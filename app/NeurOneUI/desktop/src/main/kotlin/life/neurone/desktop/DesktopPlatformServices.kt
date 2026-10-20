package life.neurone.desktop

import life.neurone.core.migration.LinkTransport
import life.neurone.shared.migration.LanLinkTransport
import androidx.compose.runtime.Composable
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.SecretKeyStore
import life.neurone.core.migration.FileStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.core.session.ProtocolSigner
import life.neurone.shared.migration.BACKUP_FOLDER_KEY
import life.neurone.shared.migration.FolderPlaces
import life.neurone.shared.NoOpAnalyticsBackend
import life.neurone.shared.PlatformServices
import life.neurone.shared.ble.BleCentral
import java.io.File
import java.util.Properties

/**
 * The macOS / Windows / Linux half of the cross-platform seam.
 *
 * Bluetooth is btleplug through JNI ([DesktopBleCentral]); where its native library is absent the hub link is
 * [life.neurone.shared.ble.UnavailableBleCentral] and everything that does not need the hub still works.
 */
class DesktopPlatformServices(
    override val keyValueStore: KeyValueStore = FileKeyValueStore(
        desktopDataDirectory.resolve(KEY_VALUE_FILE),
        DataFileProtection.forThisSystem(),
    ),
    override val bleCentral: BleCentral = createDesktopBleCentral(keyValueStore),
    override val protocolSigner: ProtocolSigner = Ed25519ProtocolSigner(),
    override val analyticsBackend: AnalyticsBackend = NoOpAnalyticsBackend(),
    override val httpTransport: life.neurone.core.net.HttpTransport? = JdkHttpTransport(),
    override val analyticsConfig: life.neurone.core.analytics.AnalyticsConfig? = desktopAnalyticsConfig(),
) : PlatformServices {

    /** The JDK client with the relay's SPKI pins (REQ-SHDR-04). */
    override fun fleetTransport(relayHost: String, pins: List<String>): life.neurone.core.net.HttpTransport =
        JdkHttpTransport(pinnedHttpClient(pins))

    /** Where the hub leaves the SHDR record, beside the app's data (REQ-SHDR-01). */
    override val shdrStaging: life.neurone.core.analytics.ShdrStaging =
        life.neurone.core.analytics.FileShdrStaging(desktopDataDirectory.resolve(life.neurone.core.analytics.FileShdrStaging.FILE_NAME))

    /** The system credential store, if it proves it can keep and remove a secret; otherwise none (SWR-MG-06). */
    override val secretKeyStore: SecretKeyStore? by lazy { DesktopSecretKeyStore.create() }

    /** The credential store can be missing on a run (a tool blocked, a keychain locked), and the data is then on disk unsealed. */
    override val reportsUnsealedStorage: Boolean get() = true

    /**
     * Where a backup goes: the folder the person chose, or `backups` beside the app's data, from which they copy it to a USB drive or
     * the new computer. The operating system arbitrates the claim (a hard link), so a folder promises an exclusive create (SWR-MG-07).
     * Made on first use, not at start-up.
     */
    override val folderPlaces: FolderPlaces = DesktopFolderPlaces(desktopDataDirectory)

    /**
     * SWR-MG-06: every file in the app's data folder (the key-value store's file and its temporary copy, and anything else the app
     * wrote there) except the chosen backup folder when it is inside it, which the clear runs against. Read each time, because the
     * choice can change.
     */
    override val migrationFiles: FileStore
        get() = desktopOwnFiles(
            desktopDataDirectory,
            keyValueStore.getString(BACKUP_FOLDER_KEY)?.takeIf { it.isNotBlank() } ?: folderPlaces.defaultFolder,
        )

    /** After "erase this device" the run holds the old state in memory, so the app ends and is started again (SWR-MG-17). */
    override val endRunAfterErase: (() -> Unit) = { kotlin.system.exitProcess(0) }

    /** A direct move to another phone on the same Wi-Fi: sockets written once for every host that has them (SWR-MG-10). */
    override val linkTransport: LinkTransport = LanLinkTransport()

    @Composable
    override fun rememberBleConnectAction(): () -> Unit = { bleCentral.refresh() }
}

/**
 * The per-user application-data directory of the operating system the app is running on. Windows uses the per-machine `LOCALAPPDATA`,
 * not the roaming `APPDATA`: the data key is held in a per-machine credential store, so a data file that roamed would reach a machine
 * with no key (OI-DESKTOP-01, item 2).
 */
fun appDataDirectory(
    os: String = System.getProperty("os.name").lowercase(),
    home: String = System.getProperty("user.home"),
    env: (String) -> String? = System::getenv,
): File = when {
    "mac" in os -> File(home, "Library/Application Support/NeurOne")
    "win" in os -> File(env("LOCALAPPDATA") ?: "$home/AppData/Local", "NeurOne")
    else -> File(env("XDG_DATA_HOME") ?: "$home/.local/share", "NeurOne")
}

/** Where earlier releases kept the data on Windows (roaming), which [moveLegacyDataDirectory] moves out of; null on the other systems. */
fun legacyAppDataDirectory(
    os: String = System.getProperty("os.name").lowercase(),
    home: String = System.getProperty("user.home"),
    env: (String) -> String? = System::getenv,
): File? = if ("win" in os) File(env("APPDATA") ?: "$home/AppData/Roaming", "NeurOne") else null

/**
 * Moves what an earlier release left in [legacy] to [current], once. It acts only when [current] has no key-value file of its own, so
 * state already made here is never overwritten, and it leaves [legacy] untouched if the move fails (the next start tries again). A
 * rename is tried first, so the data never exists in two places; a copy-then-delete is the fallback across volumes.
 */
fun moveLegacyDataDirectory(legacy: File?, current: File) {
    if (legacy == null || !legacy.isDirectory || legacy.canonicalFile == current.canonicalFile) return
    if (current.resolve(KEY_VALUE_FILE).exists()) return
    runCatching {
        current.parentFile?.mkdirs()
        if (current.exists() && current.list().isNullOrEmpty()) current.delete()
        if (!current.exists() && legacy.renameTo(current)) return
        legacy.copyRecursively(current, overwrite = false)
        legacy.deleteRecursively()
    }
}

internal const val KEY_VALUE_FILE = "np-app.properties"

/** The data folder for this run, with anything an earlier release left elsewhere moved into it first. */
val desktopDataDirectory: File by lazy { appDataDirectory().also { moveLegacyDataDirectory(legacyAppDataDirectory(), it) } }

/**
 * [KeyValueStore] in one properties file, written whole to a temporary file and renamed over the
 * real one so a crash mid-write leaves the previous contents, not half of them. Every put persists
 * at once: this store holds consent decisions, and a decision the user made must survive the
 * process being killed a moment later.
 */
class FileKeyValueStore(
    private val file: File,
    private val protection: DataFileProtection = DataFileProtection.NONE,
) : KeyValueStore {
    private val properties = Properties()

    init {
        if (file.exists()) {
            // A file an earlier release wrote is brought under the protection now, not at the next save.
            protection.prepare(file)
            file.inputStream().use { properties.load(it) }
        }
    }

    @Synchronized private fun persist() {
        file.parentFile?.let { it.mkdirs(); protection.prepare(it) }
        val temporary = File(file.parentFile, file.name + ".tmp")
        // The temporary file is made owner-only and excluded from backups before anything is written to it, and the rename carries
        // both to the real file, so the real file is never in the open (SWR-MG-18).
        protection.prepare(temporary)
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

/** The folder `backups` inside the data folder, which is where a backup goes until another is chosen. */
internal const val BACKUP_FOLDER = "backups"
