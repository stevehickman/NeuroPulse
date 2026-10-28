package life.neurone.app

import android.app.Application
import android.content.Context
import life.neurone.app.ble.AndroidBleCentral
import life.neurone.core.cloudauth.ExternalBrowserLauncher
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.session.ProtocolUploader
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.analytics.WarrantyAnalyticsGate
import life.neurone.shared.AppServices
import life.neurone.shared.NoOpAnalyticsBackend
import life.neurone.shared.ble.NeurOneGattManager
import life.neurone.core.common.KeyValueStore
import life.neurone.core.models.ActiveUserTag
import life.neurone.core.consent.ConsentStore
import life.neurone.core.consumable.ConsumableTracker
import life.neurone.core.protocol.NPLimitsStore
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.NPValidationText
import life.neurone.core.research.ResearchSuggestionStore
import life.neurone.core.migration.AppFolder
import life.neurone.core.migration.appFileStore
import life.neurone.core.session.SessionHistoryStore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

/** SharedPreferences adapter for the core KeyValueStore abstraction. */
class SharedPrefsKeyValueStore(context: Context) : KeyValueStore {
    private val prefs = context.getSharedPreferences("np-app", Context.MODE_PRIVATE)

    override fun getString(key: String): String? = prefs.getString(key, null)
    override fun putString(key: String, value: String) =
        prefs.edit().putString(key, value).apply()
    override fun getInt(key: String, default: Int): Int = prefs.getInt(key, default)
    override fun putInt(key: String, value: Int) = prefs.edit().putInt(key, value).apply()
    override fun getBoolean(key: String, default: Boolean): Boolean =
        prefs.getBoolean(key, default)
    override fun putBoolean(key: String, value: Boolean) =
        prefs.edit().putBoolean(key, value).apply()
    override fun remove(key: String) = prefs.edit().remove(key).apply()
}

class NeurOneApplication : Application() {

    /** The cross-platform composition root (app/NeurOneUI/shared). Everything below is a view onto it. */
    lateinit var services: AppServices
        private set

    /**
     * The sign-in to a cloud provider opens the system browser and is answered by an intent to [life.neurone.app.ui.MainActivity],
     * which reports it here (SWR-MG-19). Nothing constructs a `CloudAuth` yet; this is the launcher it will be given.
     */
    val browserLauncher = ExternalBrowserLauncher { AndroidBrowser.open(this, it) }

    /** Whether an activity is on screen: the presence lock's background timeout reads it (`REQ-PRES-03`). */
    private val foreground = AndroidAppForeground()

    val keyValueStore: KeyValueStore get() = services.keyValueStore
    val researchAnalyticsGate: ResearchAnalyticsGate get() = services.researchAnalyticsGate
    val warrantyAnalyticsGate: WarrantyAnalyticsGate get() = services.warrantyAnalyticsGate
    val consentStore: ConsentStore get() = services.consentStore
    val sessionHistoryStore: SessionHistoryStore get() = services.sessionHistoryStore
    val protocolLibrary: NPProtocolLibrary get() = services.protocolLibrary
    val gattManager: NeurOneGattManager get() = services.gattManager
    val consumableTracker: ConsumableTracker get() = services.consumableTracker
    val researchSuggestionStore: ResearchSuggestionStore get() = services.researchSuggestionStore
    val limitsStore: NPLimitsStore get() = services.limitsStore

    /** Real Android BLE central. Permission-aware — no scan until BLUETOOTH_SCAN/CONNECT
     *  are granted. Call `bleCentral.refresh()` after a permission grant to start scanning. */
    lateinit var bleCentral: AndroidBleCentral
        private set
    val protocolUploader: ProtocolUploader get() = services.protocolUploader

    private val bleScope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    override fun onCreate() {
        super.onCreate()
        // :core cannot reach resources, so the validator's locale keys are resolved here. sync-locales
        // lowercases a key into its resource name and turns `{0}` into `%1$s`.
        NPValidationText.use { key, args ->
            val id = resources.getIdentifier(key.lowercase(), "string", packageName)
            if (id == 0) null else getString(id, *args.toTypedArray())
        }
        // BLE central + manager are constructed here but do not scan until the UI obtains
        // runtime permission and calls bleCentral.refresh() (adapterState = UNAUTHORIZED
        // until then, so the manager's auto-scan-on-ON path is inert at startup).
        bleCentral = AndroidBleCentral(this)
        foreground.register(this)
        services = AppServices(
            platform = AndroidPlatformServices(
                SharedPrefsKeyValueStore(this), bleCentral, Ed25519ProtocolSigner(), secretKeyStore = AndroidSecretKeyStore(this),
                migrationFiles = ownFiles(),
                httpTransport = OkHttpTransport(),
                analyticsConfig = life.neurone.core.analytics.AnalyticsConfig(BuildConfig.ANALYTICS_HOST, BuildConfig.POSTHOG_PROJECT_TOKEN),
                shdrStaging = life.neurone.core.analytics.FileShdrStaging(File(filesDir, life.neurone.core.analytics.FileShdrStaging.FILE_NAME)),
                folderPlaces = AndroidFolderPlaces(this),
                backgroundBackup = AndroidBackgroundBackupScheduler(this),
                uhdrFolder = AndroidUhdrFolder(File(filesDir, "UHDR")),
                reminderNotifier = AndroidReminderNotifier(this),
                // Dark: the gated key is built but not offered until installs that already hold sealed UHDR can be moved to it (REQ-PRES-07).
                gatedKeyStore = if (OFFER_PRESENCE_GATE) AndroidGatedSecretKeyStore(this) else null,
                appInForeground = foreground.inForeground,
            ),
            scope = bleScope,
        )
    }

    /**
     * SWR-MG-06: the files the old phone's clear overwrites and deletes. The key-value store's backing file is
     * `shared_prefs/np-app.xml`, and the framework keeps `np-app.xml.bak` beside it while it writes; both can hold the plain
     * values of keys not yet sealed, so both are listed, with everything in the app's private `files` folder. The backup place
     * is a document the person chose through the system's dialogs, outside these folders. The preferences are then written
     * again from memory as the clear removes its keys, so a clear that stops part way does not lose its progress.
     */
    private fun ownFiles() = appFileStore(
        listOf(
            AppFolder("files", filesDir.toPath()),
            AppFolder("shared_prefs", java.io.File(dataDir, "shared_prefs").toPath()),
        ),
    )

    private companion object {
        /**
         * Whether the app offers the presence gate (`NP-APP-PRESENCE-001`). False until `REQ-PRES-07` (moving an install that already holds
         * sealed UHDR to the gated key) is built: offering it earlier would let a person turn on a gate that cannot keep their record.
         */
        const val OFFER_PRESENCE_GATE = false
    }
}
