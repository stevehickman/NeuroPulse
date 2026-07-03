package com.neuropulse.app

import android.app.Application
import android.content.Context
import com.neuropulse.core.analytics.EngagementTier
import com.neuropulse.core.analytics.ResearchAnalyticsGate
import com.neuropulse.core.analytics.WarrantyAnalyticsGate
import com.neuropulse.core.common.KeyValueStore
import com.neuropulse.core.consent.ConsentStore
import com.neuropulse.core.session.SessionHistoryStore

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

    override fun onCreate() {
        super.onCreate()
        keyValueStore = SharedPrefsKeyValueStore(this)
        researchAnalyticsGate = ResearchAnalyticsGate(keyValueStore, NoOpAnalyticsBackend())
        warrantyAnalyticsGate = WarrantyAnalyticsGate(keyValueStore)
        consentStore = ConsentStore(keyValueStore, researchAnalyticsGate)
        sessionHistoryStore = SessionHistoryStore(keyValueStore)

        EngagementTier.incrementLaunchCount(keyValueStore)
        // SDK initialization gate (NP-APP-TELEMETRY-001 Rev B §5): configure()
        // no-ops unless the user actively completed the consent flow.
        researchAnalyticsGate.configure()
    }
}
