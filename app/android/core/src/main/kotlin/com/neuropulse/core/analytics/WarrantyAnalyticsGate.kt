package com.neuropulse.core.analytics

import com.neuropulse.core.common.KeyValueStore

/**
 * Consent gate for SHDR fleet telemetry uploads. Port of iOS WarrantyAnalyticsGate.
 *
 * Consent subject: the WARRANTY OWNER — the entity that registered the device
 * warranty, which may be a clinic or institution, NOT the person wearing the
 * device (CLAUDE.md §6.0). Warranty consent is entirely separate from user
 * research consent:
 *  - Revoking user research consent has no effect on SHDR uploads.
 *  - Revoking warranty consent has no effect on research participation.
 *  - This class must never reference ResearchAnalyticsGate or ConsentStore,
 *    and must never share a persistence key with them.
 */
class WarrantyAnalyticsGate(private val store: KeyValueStore) {

    companion object {
        /** Distinct from ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY by design. */
        const val WARRANTY_CONSENT_KEY = "np.warranty.consent-granted"
    }

    val isOpen: Boolean
        get() = store.getBoolean(WARRANTY_CONSENT_KEY)

    /** Granted at device warranty registration by the warranty owner. */
    fun grant() = store.putBoolean(WARRANTY_CONSENT_KEY, true)

    /** SHDR uploads stop immediately; research consent state is untouched. */
    fun revoke() = store.remove(WARRANTY_CONSENT_KEY)
}
