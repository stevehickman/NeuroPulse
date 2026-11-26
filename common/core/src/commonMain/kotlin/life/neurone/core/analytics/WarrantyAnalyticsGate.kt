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

    /**
     * True once the owner has answered the reports question for this helmet, yes or no; false when they have never been asked. A
     * record made only by another of the owner's choices (the stealth lock, the offered set) is not an answer (`REQ-SHDR-12` (e)).
     */
    fun isDecided(hub: String?): Boolean = hub != null && records.record(hub)?.fleetDecided == true

    /**
     * The general-consent content-set version this helmet's owner said yes to (`REQ-SHDR-12` (d)), or `null` when reports are not
     * on. Content a later set adds is not covered until the owner says yes to that set.
     */
    fun contentLevel(hub: String?): Int? = hub?.let { records.record(it) }?.takeIf { it.fleetConsent }?.contentLevel

    /**
     * Granted by the warranty owner for this helmet, for the content set [contentVersion] that they were shown (the one this app
     * ships, [GeneralShdrContent.shipped], unless a caller showed another). Refused (false) when no helmet is named: consent with
     * no helmet to attach it to would be consent for whichever helmet connects next. A grant answers the content question for
     * that set too, so prompt 2 does not ask it again (`REQ-SHDR-12` (b)).
     */
    fun grant(hub: String?, contentVersion: Int = GeneralShdrContent.shipped.version): Boolean {
        if (hub == null || contentVersion < 0) return false
        records.update(hub) {
            it.copy(
                fleetConsent = true, fleetDecided = true, contentLevel = contentVersion,
                contentAsked = maxOf(it.contentAsked, contentVersion),
            )
        }
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
        records.update(hub) { it.copy(fleetConsent = false, fleetDecided = true, enrolled = false) }
        // A record staged under the consent now revoked is never sent, by a later grant or by anyone else (`REQ-SHDR-08` (g)).
        records.clearStaged(hub)
    }

    /**
     * The SHDR record staged for this helmet, only while this helmet's owner has consented; `null` otherwise. The helmet is the
     * one the caller names, once, so the consent and the record are this one helmet's (`REQ-SHDR-08` (f)).
     */
    fun stagedRecord(hub: String?): ByteArray? = if (hub != null && isOpen(hub)) records.staged(hub) else null

    /** Stage a record for this helmet. Refused (false) unless this helmet's owner has consented: nothing waits to be sent unasked. */
    fun stage(hub: String?, record: ByteArray): Boolean = hub != null && isOpen(hub) && records.stage(hub, record)

    /** Delete this helmet's staged record, once the fleet service has taken it. */
    fun clearStaged(hub: String?) {
        if (hub != null) records.clearStaged(hub)
    }

    /**
     * A change of custody: the helmet came back from a factory reset or went to a new owner (`REQ-SHDR-09` (d); `NP-MOD-ID-001`
     * §A.4). The previous owner's fleet consent, enrolment, epoch and programme, and the helmet's staged record, are forgotten, so
     * the helmet reads as never asked and its new owner decides afresh. Every other helmet is untouched.
     */
    fun endCustody(hub: String?) {
        if (hub != null) records.forget(hub)
    }
}
