package life.neurone.shared

import life.neurone.core.migration.LinkTransport
import life.neurone.shared.migration.LanLinkTransport
import androidx.compose.runtime.Composable
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.SecretKeyStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.core.session.ProtocolSigner
import life.neurone.shared.ble.BleCentral
import life.neurone.core.migration.FileStore
import life.neurone.core.migration.FolderUnionFileStore
import life.neurone.core.migration.KeepingFileStore
import life.neurone.core.migration.ObjectStoreRoute
import platform.Foundation.NSApplicationSupportDirectory
import platform.Foundation.NSDocumentDirectory
import platform.Foundation.NSSearchPathForDirectoriesInDomains
import platform.Foundation.NSUserDefaults
import platform.Foundation.NSUserDomainMask

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
    override val secretKeyStore: SecretKeyStore? = IosSecretKeyStore(),
    override val httpTransport: life.neurone.core.net.HttpTransport? = IosHttpTransport(),
    override val analyticsConfig: life.neurone.core.analytics.AnalyticsConfig? = iosAnalyticsConfig(),
) : PlatformServices {

    /** `NSURLSession` with the relay's SPKI pins (REQ-SHDR-04). */
    override fun fleetTransport(relayHost: String, pins: List<String>): life.neurone.core.net.HttpTransport =
        IosHttpTransport(pinnedHost = relayHost, pins = pins.toSet())

    /** The record the hub leaves in Documents, as the Swift app reads it (REQ-SHDR-01). */
    override val shdrStaging: life.neurone.core.analytics.ShdrStaging by lazy {
        val documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, true).first() as String
        IosShdrStaging("$documents/shdr_staging.bin")
    }

    /**
     * A folder in the app's Documents directory, which the Files app shows (the host declares file sharing), so the person can
     * copy the backup to a drive or another phone. The file system arbitrates the claim (a hard link), as on the desktop.
     */
    /** A direct move to another phone on the same Wi-Fi: sockets written once for every host that has them (SWR-MG-10). */
    override val linkTransport: LinkTransport = LanLinkTransport()

    /** iOS wakes the app for the backup through `BGTaskScheduler` ([registerBackgroundBackup] is called by the host at launch). */
    override val backgroundBackup: life.neurone.shared.migration.BackgroundBackupScheduler = IosBackgroundBackupScheduler()

    /** Mode 4: the hub's EDF+ recordings in `Documents/UHDR`, protected as the Swift app protects them. */
    override val uhdrFolder: life.neurone.shared.session.UhdrFolder by lazy {
        val documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, true).first() as String
        IosUhdrFolder("$documents/UHDR")
    }

    /** The consumable reminders as local notifications. */
    override val reminderNotifier: ReminderNotifier = IosReminderNotifier()

    /** HealthKit, read-only, for the live HRV display of an HRV biofeedback session. */
    override val healthSignals: HealthSignals = IosHealthSignals()

    override val migrationPlace: MigrationPlace? by lazy {
        val documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, true).first() as String
        MigrationPlace(FOLDER_NAME, IosFolderObjectStore("$documents/$FOLDER_NAME"), ObjectStoreRoute("documents-folder", atomicClaim = true))
    }

    /**
     * SWR-MG-06: the app's own files for the clear to overwrite and delete: its Documents folder, except the backup place the
     * clear is clearing against, and its Application Support folder. The key-value store is `NSUserDefaults`, which the system
     * keeps in a property list it owns; the clear removes those keys through the store and cannot overwrite that list in place.
     */
    override val migrationFiles: FileStore by lazy {
        fun folder(directory: platform.Foundation.NSSearchPathDirectory) =
            NSSearchPathForDirectoriesInDomains(directory, NSUserDomainMask, true).first() as String
        FolderUnionFileStore(
            mapOf(
                "documents" to KeepingFileStore(IosDirectoryFileStore(folder(NSDocumentDirectory)), listOf(FOLDER_NAME)),
                "support" to IosDirectoryFileStore(folder(NSApplicationSupportDirectory)),
            ),
        )
    }

    /** The system's document picker, for the person's protocols as a file and for a backup they carry. */
    private val fileTransfer = IosFileTransfer()

    @Composable
    override fun rememberFileTransfer(): life.neurone.shared.migration.FileTransfer = fileTransfer

    @Composable
    override fun rememberBleConnectAction(): () -> Unit = { bleCentral.refresh() }
}

private const val FOLDER_NAME = "NeurOne Backups"

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
