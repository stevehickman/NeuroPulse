package life.neurone.core.analytics

/**
 * Consent gate for SHDR fleet telemetry uploads, per helmet (`REQ-SHDR-08`).
 *
 * Consent subject: the WARRANTY OWNER of the helmet in question, the entity that registered that device's warranty, which may be
 * a clinic or institution, NOT the person wearing the device and NOT whoever owns another helmet this phone meets
 * (CLAUDE.md §6.0). So every question here names a helmet, and consent recorded for one helmet opens nothing for another. With no
 * helmet named (none is connected) the gate is closed and nothing can be granted.
 *
 * Warranty consent is entirely separate from user research consent:
 *  - Revoking user research consent has no effect on SHDR uploads.
 *  - Revoking warranty consent has no effect on research participation.
 *  - This class must never reference ResearchAnalyticsGate or ConsentStore,
 *    and must never share a persistence key with them.
 */
class WarrantyAnalyticsGate(private val records: WarrantyOwnerStore) {

    companion object {
        /** Distinct from PersistedKeys.RESEARCH_ANALYTICS_KEY by design. */
        const val WARRANTY_CONSENT_KEY = WarrantyOwnerStore.RECORDS_KEY
    }

    /** True only when this helmet's warranty owner has consented on this install. */
    fun isOpen(hub: String?): Boolean = hub != null && records.record(hub)?.fleetConsent == true

    /** True once the owner has decided either way for this helmet; false when they have never been asked. */
    fun isDecided(hub: String?): Boolean = hub != null && records.record(hub) != null

    /**
     * Granted by the warranty owner for this helmet. Refused (false) when no helmet is named: consent with no helmet to attach it
     * to would be consent for whichever helmet connects next.
     */
    fun grant(hub: String?): Boolean {
        if (hub == null) return false
        records.update(hub) { it.copy(fleetConsent = true) }
        return true
    }

    /**
     * SHDR uploads for this helmet stop immediately; research consent state is untouched, and so is every other helmet.
     *
     * The helmet's characterisation enrolment is withdrawn with it (`REQ-SHDR-09` (b)): the extended set travels on the upload
     * this consent opens, and a later grant, possibly by a new owner, must not bring the enrolment back unasked. The epoch is kept,
     * as [CharacterisationEnrolmentGate.withdraw] keeps it.
     */
    fun revoke(hub: String?) {
        if (hub == null) return
        records.update(hub) { it.copy(fleetConsent = false, enrolled = false) }
    }
}
