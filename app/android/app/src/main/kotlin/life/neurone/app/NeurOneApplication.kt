package life.neurone.app

import android.app.Application
import android.content.Context
import life.neurone.app.ble.AndroidBleCentral
import life.neurone.app.data.EncryptedPrefsDeviceTokenStore
import life.neurone.app.data.ShdrUploadWiring
import life.neurone.app.data.ShdrUploader
import life.neurone.app.session.AndroidProtocolSigner
import life.neurone.app.session.ProtocolUploader
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
    lateinit var protocolUploader: ProtocolUploader
        private set
    lateinit var shdrUploader: ShdrUploader
        private set

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
        services = AppServices(
            platform = AndroidPlatformServices(SharedPrefsKeyValueStore(this), bleCentral),
            scope = bleScope,
        )
        protocolUploader = ProtocolUploader(gattManager, AndroidProtocolSigner())

        // SHDR fleet uploader — gated on the WARRANTY OWNER's consent only
        // (WarrantyAnalyticsGate), structurally independent of user research consent.
        // Device identity is a Keystore-encrypted CSPRNG token, upgraded to the
        // hub-provisioned TRNG token when the GATT characteristic ships (OI-BLE-01).
        shdrUploader = ShdrUploader(EncryptedPrefsDeviceTokenStore(this), warrantyAnalyticsGate)
        composeShdrUploadPipeline()
    }

    /**
     * Compose the Android SHDR upload pipeline to iOS parity (SHDRUploader.swift +
     * SHDRUploadTriggering.swift). Two long-lived collectors on [bleScope]:
     *
     *  (a) shdrUploadPending → upload the hub-staged SHDR payload when the warranty
     *      owner has consented and a payload is available (deleted on success).
     *  (b) warrantyToken → adopt the hub-provisioned TRNG token (OI-BLE-01); nulls
     *      are dropped so a disconnect never downgrades the token.
     */
    private fun composeShdrUploadPipeline() {
        bleScope.launch {
            gattManager.warrantyToken.collect { token ->
                ShdrUploadWiring.applyWarrantyToken(token, shdrUploader)
            }
        }
        bleScope.launch {
            gattManager.shdrUploadPending.collect { pending ->
                val gateOpen = warrantyAnalyticsGate.isOpen
                val payload =
                    if (pending && gateOpen) withContext(Dispatchers.IO) { readShdrStaging() }
                    else null
                ShdrUploadWiring.applyUploadPending(
                    pending = pending,
                    warrantyGateOpen = gateOpen,
                    stagingPayload = payload,
                    uploader = shdrUploader,
                ) { success -> if (success) deleteShdrStaging() }
            }
        }
    }

    /**
     * The hub drops the SHDR binary blob into the app-private files directory over
     * its USB-C CDC interface (parallel of iOS's Documents/shdr_staging.bin, read
     * from the same staging convention). Returns null when nothing is staged.
     */
    private fun readShdrStaging(): ByteArray? {
        val file = File(filesDir, SHDR_STAGING_FILE)
        return if (file.exists()) file.readBytes() else null
    }

    private fun deleteShdrStaging() {
        File(filesDir, SHDR_STAGING_FILE).delete()
    }

    private companion object {
        const val SHDR_STAGING_FILE = "shdr_staging.bin"
    }
}
