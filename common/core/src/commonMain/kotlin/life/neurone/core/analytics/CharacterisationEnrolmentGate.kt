package life.neurone.core.analytics

import life.neurone.core.protocol.PersistedKeys

/**
 * Consent gate for the SHDR accelerometer characterisation programme
 * (NP-FW-EMMC-002 Rev 2 §H). Port of iOS CharacterisationEnrolmentGate.
 *
 * During a time-boxed characterisation window an enrolled device additionally
 * emits a coarsened per-gap impact histogram — counts per fixed g-bin, never a
 * raw series, a per-event value, an orientation vector or a per-event timestamp.
 * That is data §G.3 otherwise prohibits, which is why it needs its own
 * affirmative opt-in rather than riding on an existing one.
 *
 * WHY THIS IS SEPARATE FROM WarrantyAnalyticsGate. Warranty consent is granted
 * once, at device registration, for ordinary fleet telemetry. The
 * characterisation set is a materially different category of content, and
 * folding it into that one-time grant would be consent for a purpose the
 * registrant never contemplated (NP-MOD-ID-001 §7.5). §H requires the opt-in to
 * be separate, affirmative, granular and revocable.
 *
 * CONSENT SUBJECT is the WARRANTY OWNER (CLAUDE.md §6.0), which may be a clinic
 * or institution and is not assumed to be the person wearing the device.
 * Registrant consent suffices in every configuration because there is no
 * identifiable subject: SHDR holds no user identifier, the warranty database
 * (which SHDR cannot join) names the registrant, and nothing records the
 * registrant-to-wearer relationship (NP-MOD-ID-001 §7.5.1). The argument is
 * stronger here than in that precedent — a duty map at least requires the device
 * to be WORN, and a drop does not.
 *
 * STRUCTURAL INDEPENDENCE (CLAUDE.md §6.0): this class must never reference
 * ConsentStore or ResearchAnalyticsGate and must never share a persistence key
 * with them. The one coupling that exists is a strict subset relation with
 * WarrantyAnalyticsGate, running one way only — see [isOpen].
 *
 * PER HELMET (`REQ-SHDR-09`; the principal, 2026-11-20, `OI-PRIV-04`). Enrolment
 * is a decision about one helmet by that helmet's warranty owner, kept in the
 * warranty owner's store beside that helmet's consent. Every question here
 * names a helmet; with none named the gate is closed and nothing is recorded.
 * It is not user data and never moves with a user's migration or backup
 * (`REQ-SHDR-10`).
 *
 * CHANGE OF CUSTODY (`REQ-SHDR-09` (d); NP-MOD-ID-001 §A.4). A consent decision
 * by a previous registrant must not keep authorising collection for a new one.
 * When the hub delivers a token different from the one kept for it (a factory
 * reset or a new owner), [FleetUploader.upgradeDeviceToken] calls
 * [WarrantyAnalyticsGate.endCustody], which forgets the helmet's whole record:
 * the enrolment, the epoch and the programme with the fleet consent. The helmet
 * starts un-enrolled and its new owner's first enrolment is epoch 1.
 */
class CharacterisationEnrolmentGate(
    private val records: WarrantyOwnerStore,
    private val warrantyGate: WarrantyAnalyticsGate,
) {

    /**
     * True only when this helmet's warranty owner has affirmatively enrolled it
     * AND warranty consent for it is still open.
     *
     * The warranty conjunct is a strict subset relation, not a merge:
     * characterisation records travel on the SHDR upload path, so an owner who
     * revoked SHDR telemetry cannot still be contributing the more detailed set.
     * It runs one way only — enrolling here never grants warranty consent.
     * A warranty revoke also withdraws the enrolment itself
     * ([WarrantyAnalyticsGate.revoke]), so a later grant does not reopen it.
     */
    fun isOpen(hub: String?): Boolean {
        if (hub == null || !warrantyGate.isOpen(hub)) return false
        val r = records.record(hub) ?: return false
        return r.enrolled && r.programmeId == PersistedKeys.CURRENT_PROGRAMME_ID && r.consentEpoch >= 1
    }

    /** Zero means "never granted", which the firmware treats as not enrolled. */
    fun consentEpoch(hub: String?): Int = hub?.let { records.record(it)?.consentEpoch } ?: 0

    /**
     * Record an affirmative enrolment of this helmet by its warranty owner.
     *
     * Call only from the §H enrolment screen after an explicit choice — never
     * from onboarding defaults, never from warranty registration, never as a
     * side effect of another setting.
     *
     * Refused unless warranty consent for this helmet is open: enrolling a
     * device whose owner has not consented to SHDR telemetry at all would be
     * incoherent, so it is refused rather than silently recorded.
     *
     * @return the new consent epoch, or null if refused.
     */
    fun grant(hub: String?): Int? {
        if (hub == null || !warrantyGate.isOpen(hub)) return null
        val epoch = consentEpoch(hub) + 1
        records.update(hub) { it.copy(enrolled = true, consentEpoch = epoch, programmeId = PersistedKeys.CURRENT_PROGRAMME_ID) }
        return epoch
    }

    /**
     * Withdraw. Collection stops from the next session gap and this device's
     * extended rows are deleted at the next sync (§H.5). What withdrawal cannot
     * do is un-train a model that has already learned from a contribution; the
     * enrolment copy says so at enrolment time rather than at the point someone
     * tries to leave (OI-EMMC2-12).
     *
     * The epoch is deliberately not reset — it must keep increasing. Only a
     * change of custody resets it, with the rest of the helmet's record.
     * No effect on WarrantyAnalyticsGate, ConsentStore or any UHDR flow.
     * A helmet with no record is left with none: it was never enrolled.
     */
    fun withdraw(hub: String?) {
        if (hub == null || records.record(hub) == null) return
        records.update(hub) { it.copy(enrolled = false) }
    }
}
