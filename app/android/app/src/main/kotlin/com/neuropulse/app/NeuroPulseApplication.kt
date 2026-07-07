package com.neuropulse.app

import android.app.Application
import android.content.Context
import com.neuropulse.app.ble.AndroidBleCentral
import com.neuropulse.app.ble.NeuroPulseGattManager
import com.neuropulse.app.session.AndroidProtocolSigner
import com.neuropulse.app.session.ProtocolUploader
import com.neuropulse.core.analytics.EngagementTier
import com.neuropulse.core.analytics.ResearchAnalyticsGate
import com.neuropulse.core.analytics.WarrantyAnalyticsGate
import com.neuropulse.core.common.KeyValueStore
import com.neuropulse.core.consent.ConsentStore
import com.neuropulse.core.consumable.ConsumableCountsProviding
import com.neuropulse.core.consumable.ConsumableTracker
import com.neuropulse.core.models.SessionState
import com.neuropulse.core.protocol.NPProtocolLibrary
import com.neuropulse.core.session.SessionHistoryStore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch

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
class NoOpAnalyticsBackend : com.neuropulse.core.analytics.AnalyticsBackend {
    override fun configure() {}
    override fun reset() {}
    override fun track(event: String, properties: Map<String, String>) {}
}

class NeuroPulseApplication : Application() {

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
    lateinit var gattManager: NeuroPulseGattManager
        private set
    lateinit var consumableTracker: ConsumableTracker
        private set
    lateinit var protocolUploader: ProtocolUploader
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
        gattManager = NeuroPulseGattManager(bleCentral, bleScope)
        // Consumable reminder engine, fed by the hub's CONSUMABLE_STATUS counts (SHDR-class).
        consumableTracker = ConsumableTracker(
            GattConsumableCountsProvider(gattManager.session, bleScope),
            keyValueStore,
        )
        protocolUploader = ProtocolUploader(gattManager, AndroidProtocolSigner())

        EngagementTier.incrementLaunchCount(keyValueStore)
        // SDK initialization gate (NP-APP-TELEMETRY-001 Rev B §5): configure()
        // no-ops unless the user actively completed the consent flow.
        researchAnalyticsGate.configure()
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
