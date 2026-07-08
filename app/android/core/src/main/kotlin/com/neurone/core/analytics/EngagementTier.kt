package com.neurone.core.analytics

import com.neurone.core.common.KeyValueStore

// engagement_tier replaces raw session_count per NP-APP-TELEMETRY-001 Rev B §3.
// Counts app launches (not stimulation sessions). Resets on uninstall.
// Never transmitted as a raw integer — only the bucketed enum label is sent.

enum class EngagementTier(val label: String) {
    NEW("new"),                 // 1–5 app launches
    ACTIVE("active"),           // 6–50 app launches
    ESTABLISHED("established"); // 51+ app launches

    companion object {
        const val LAUNCH_COUNT_KEY = "np.analytics.launch-count"

        fun current(store: KeyValueStore): EngagementTier {
            val count = store.getInt(LAUNCH_COUNT_KEY)
            return when {
                count < 6 -> NEW           // covers 0 (key absent) and 1–5
                count <= 50 -> ACTIVE
                else -> ESTABLISHED
            }
        }

        fun incrementLaunchCount(store: KeyValueStore) {
            store.putInt(LAUNCH_COUNT_KEY, store.getInt(LAUNCH_COUNT_KEY) + 1)
        }
    }
}
