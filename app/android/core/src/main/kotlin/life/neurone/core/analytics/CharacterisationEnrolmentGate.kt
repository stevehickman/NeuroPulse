package life.neurone.core.analytics

import life.neurone.core.common.KeyValueStore

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
 */
class CharacterisationEnrolmentGate(
    private val store: KeyValueStore,
    private val warrantyGate: WarrantyAnalyticsGate,
) {

    companion object {
        /** Distinct from WarrantyAnalyticsGate.WARRANTY_CONSENT_KEY by design. */
        const val ENROLMENT_KEY = "np.characterisation.enrolled"

        /**
         * Consent generation. Incremented on every grant, never reset by
         * withdrawal, so a row cannot outlive the grant that authorised it and a
         * re-enrolment is distinguishable from the enrolment before it — without
         * needing a clock to order them.
         */
        const val CONSENT_EPOCH_KEY = "np.characterisation.consent-epoch"

        /** Must match NP_ACCEL_CHAR_PROGRAMME_ID in np_accel_shdr.h. */
        const val PROGRAMME_ID_KEY = "np.characterisation.programme-id"

        /** A new programme takes a new id and requires fresh enrolment. */
        const val CURRENT_PROGRAMME_ID = 1
    }

    /**
     * True only when the warranty owner has affirmatively enrolled AND warranty
     * consent itself is still open.
     *
     * The warranty conjunct is a strict subset relation, not a merge:
     * characterisation records travel on the SHDR upload path, so an owner who
     * revoked SHDR telemetry cannot still be contributing the more detailed set.
     * It runs one way only — enrolling here never grants warranty consent.
     */
    val isOpen: Boolean
        get() = warrantyGate.isOpen &&
            store.getBoolean(ENROLMENT_KEY) &&
            store.getInt(PROGRAMME_ID_KEY) == CURRENT_PROGRAMME_ID &&
            consentEpoch >= 1

    /** Zero means "never granted", which the firmware treats as not enrolled. */
    val consentEpoch: Int
        get() = store.getInt(CONSENT_EPOCH_KEY)

    /**
     * Record an affirmative enrolment by the warranty owner.
     *
     * Call only from the §H enrolment screen after an explicit choice — never
     * from onboarding defaults, never from warranty registration, never as a
     * side effect of another setting.
     *
     * Refused unless warranty consent is open: enrolling a device whose owner
     * has not consented to SHDR telemetry at all would be incoherent, so it is
     * refused rather than silently recorded.
     *
     * @return the new consent epoch, or null if refused.
     */
    fun grant(): Int? {
        if (!warrantyGate.isOpen) return null
        val epoch = consentEpoch + 1
        store.putBoolean(ENROLMENT_KEY, true)
        store.putInt(CONSENT_EPOCH_KEY, epoch)
        store.putInt(PROGRAMME_ID_KEY, CURRENT_PROGRAMME_ID)
        return epoch
    }

    /**
     * Withdraw. Collection stops from the next session gap and this device's
     * extended rows are deleted at the next sync (§H.5). What withdrawal cannot
     * do is un-train a model that has already learned from a contribution; the
     * enrolment copy says so at enrolment time rather than at the point someone
     * tries to leave (OI-EMMC2-12).
     *
     * The epoch is deliberately not reset — it must keep increasing.
     * No effect on WarrantyAnalyticsGate, ConsentStore or any UHDR flow.
     */
    fun withdraw() = store.putBoolean(ENROLMENT_KEY, false)

    /**
     * Clear enrolment on factory reset / change of custody. A consent decision
     * by a previous registrant must not keep authorising collection for a new
     * one (NP-MOD-ID-001 §A.4); a device out of reset starts un-enrolled and
     * asks again, epoch included.
     */
    fun clearForDeviceTransfer() {
        store.remove(ENROLMENT_KEY)
        store.remove(CONSENT_EPOCH_KEY)
        store.remove(PROGRAMME_ID_KEY)
    }
}
