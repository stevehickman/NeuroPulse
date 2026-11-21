package life.neurone.core.analytics

import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.PresenceState

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

        /**
         * Count this launch. Under the presence gate the count is sealed and a locked read answers 0, so adding one and writing
         * would replace the real count with 1 at the unlock and reset the tier every launch (`OI-PRIV-19`): the launch is counted at the
         * first unlock of the run instead, on the count read then. [lock] is null on a host with no gate.
         */
        fun incrementLaunchCount(store: KeyValueStore, lock: PresenceLock? = null) {
            if (lock == null || lock.isUnlocked) {
                store.putInt(LAUNCH_COUNT_KEY, store.getInt(LAUNCH_COUNT_KEY) + 1)
                return
            }
            var counted = false
            lock.onStateChanged { state ->
                if (state == PresenceState.UNLOCKED && !counted) {
                    counted = true
                    store.putInt(LAUNCH_COUNT_KEY, store.getInt(LAUNCH_COUNT_KEY) + 1)
                }
            }
        }
    }
}
