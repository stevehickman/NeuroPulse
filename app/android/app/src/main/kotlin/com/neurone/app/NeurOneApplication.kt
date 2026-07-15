package com.neurone.app

import android.app.Application
import android.content.Context
import com.neurone.app.ble.AndroidBleCentral
import com.neurone.app.ble.NeurOneGattManager
import com.neurone.app.data.EncryptedPrefsDeviceTokenStore
import com.neurone.app.data.ShdrUploadWiring
import com.neurone.app.data.ShdrUploader
import com.neurone.app.session.AndroidProtocolSigner
import com.neurone.app.session.ProtocolUploader
import com.neurone.core.analytics.EngagementTier
import com.neurone.core.analytics.ResearchAnalyticsGate
import com.neurone.core.analytics.WarrantyAnalyticsGate
import com.neurone.core.common.KeyValueStore
import com.neurone.core.consent.ConsentStore
import com.neurone.core.consumable.ConsumableCountsProviding
import com.neurone.core.consumable.ConsumableTracker
import com.neurone.core.models.SessionState
import com.neurone.core.protocol.NPLimitsStore
import com.neurone.core.protocol.NPProtocolLibrary
import com.neurone.core.research.ResearchSuggestionStore
import com.neurone.core.session.SessionHistoryStore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
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

/** No-op analytics backend until the production vendor SDK is selected and its
 *  DPA executed (NP-PRIV-AUDIT-001 HIGH-1 — vendor not selected; the gate
 *  architecture is in place so the SDK swap is a one-line change). */
class NoOpAnalyticsBackend : com.neurone.core.analytics.AnalyticsBackend {
    override fun configure() {}
    override fun reset() {}
    override fun track(event: String, properties: Map<String, String>) {}
}

class NeurOneApplication : Application() {

    lateinit var keyValueStore: KeyValueStore
        private set
    lateinit var researchAnalyticsGate: ResearchAnalyticsGate
        private set
    lateinit var warrantyAnalyticsGate: WarrantyAnalyticsGate
        private set
    lateinit var consentStore: ConsentStore
        private set
    lateinit var sessionHistoryStore: SessionHistoryStore
        private set
    lateinit var protocolLibrary: NPProtocolLibrary
        private set

    /** Real Android BLE central. Permission-aware — no scan until BLUETOOTH_SCAN/CONNECT
     *  are granted. Call `bleCentral.refresh()` after a permission grant to start scanning. */
    lateinit var bleCentral: AndroidBleCentral
        private set
    lateinit var gattManager: NeurOneGattManager
        private set
    lateinit var consumableTracker: ConsumableTracker
        private set
    lateinit var protocolUploader: ProtocolUploader
        private set
    lateinit var shdrUploader: ShdrUploader
        private set
    lateinit var researchSuggestionStore: ResearchSuggestionStore
        private set
    lateinit var limitsStore: NPLimitsStore
        private set

    private val bleScope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    override fun onCreate() {
        super.onCreate()
        keyValueStore = SharedPrefsKeyValueStore(this)
        researchAnalyticsGate = ResearchAnalyticsGate(keyValueStore, NoOpAnalyticsBackend())
        warrantyAnalyticsGate = WarrantyAnalyticsGate(keyValueStore)
        consentStore = ConsentStore(keyValueStore, researchAnalyticsGate)
        sessionHistoryStore = SessionHistoryStore(keyValueStore)
        protocolLibrary = NPProtocolLibrary(keyValueStore)
        // BLE central + manager are constructed here but do not scan until the UI obtains
        // runtime permission and calls bleCentral.refresh() (adapterState = UNAUTHORIZED
        // until then, so the manager's auto-scan-on-ON path is inert at startup).
        bleCentral = AndroidBleCentral(this)
        gattManager = NeurOneGattManager(bleCentral, bleScope)
        // Consumable reminder engine, fed by the hub's CONSUMABLE_STATUS counts (SHDR-class).
        consumableTracker = ConsumableTracker(
            GattConsumableCountsProvider(gattManager.session, bleScope),
            keyValueStore,
        )
        protocolUploader = ProtocolUploader(gattManager, AndroidProtocolSigner())
        researchSuggestionStore = ResearchSuggestionStore(keyValueStore)
        limitsStore = NPLimitsStore(keyValueStore)

        // SHDR fleet uploader — gated on the WARRANTY OWNER's consent only
        // (WarrantyAnalyticsGate), structurally independent of user research consent.
        // Device identity is a Keystore-encrypted CSPRNG token, upgraded to the
        // hub-provisioned TRNG token when the GATT characteristic ships (OI-BLE-01).
        shdrUploader = ShdrUploader(EncryptedPrefsDeviceTokenStore(this), warrantyAnalyticsGate)
        composeShdrUploadPipeline()

        EngagementTier.incrementLaunchCount(keyValueStore)
        // SDK initialization gate (NP-APP-TELEMETRY-001 Rev B §5): configure()
        // no-ops unless the user actively completed the consent flow.
        researchAnalyticsGate.configure()
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

/**
 * Adapts the GATT manager's session flow into the core ConsumableCountsProviding contract.
 * `SessionState.consumableSessionCounts` (SHDR-class device counts, not user biology) is the
 * source. StateFlow.collect emits the current value immediately on subscription, satisfying
 * the "current value synchronously on subscription" contract iOS's CurrentValueSubject has.
 */
private class GattConsumableCountsProvider(
    private val session: StateFlow<SessionState>,
    private val scope: CoroutineScope,
) : ConsumableCountsProviding {
    override fun observe(listener: (List<Int>) -> Unit): AutoCloseable {
        val job = scope.launch {
            session.map { it.consumableSessionCounts }.distinctUntilChanged().collect { listener(it) }
        }
        return AutoCloseable { job.cancel() }
    }
}
