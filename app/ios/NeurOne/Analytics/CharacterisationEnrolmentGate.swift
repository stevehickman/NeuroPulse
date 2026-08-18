import Foundation
import os

/// Consent gate for the SHDR accelerometer **characterisation programme**
/// (NP-FW-EMMC-002 Rev 2 §H).
///
/// During a time-boxed characterisation window an enrolled device additionally
/// emits a coarsened per-gap impact histogram — counts per fixed g-bin, never a
/// raw series, a per-event value, an orientation vector or a per-event
/// timestamp. That is data §G.3 otherwise prohibits, which is exactly why it
/// needs its own affirmative opt-in rather than riding on an existing one.
///
/// ## Why this is a separate gate and not a flag on `WarrantyAnalyticsGate`
///
/// Warranty consent is granted **once, at device registration**, for ordinary
/// fleet telemetry. The characterisation set is a materially different category
/// of content, and folding it into that one-time grant would be consent for a
/// purpose the registrant never contemplated — the reasoning `NP-MOD-ID-001`
/// §7.5 sets out for the module duty-map programme. §H requires the opt-in to be
/// **separate, affirmative, granular and revocable**, so it gets its own type,
/// its own key, and its own lifecycle.
///
/// ## Consent subject
///
/// The **warranty owner** (CLAUDE.md §6.0) — the entity that registered the
/// device, which may be a clinic or institution and is *not* assumed to be the
/// person wearing it. Registrant consent is sufficient in every configuration
/// because there is no identifiable subject: SHDR holds no user identifier, the
/// warranty database (which SHDR cannot join) names the registrant, and nothing
/// in either store records the registrant↔wearer relationship
/// (`NP-MOD-ID-001` §7.5.1).
///
/// The argument is *stronger* here than in that precedent. A duty map at least
/// requires the device to be worn; **a drop does not** — the headset can be
/// dropped by a family member, by clinic staff, by a courier, by anyone handling
/// it. An impact histogram is frequently not a record of the wearer's behaviour
/// at all.
///
/// ## Structural independence (CLAUDE.md §6.0)
///
/// This type must never reference `ConsentStore` or `ResearchAnalyticsGate`, and
/// must never share a persistence key with either. User research consent and
/// warranty-owner consent are separate subjects; revoking one has no effect on
/// the other. The one coupling that *does* exist is a strict subset relation
/// with `WarrantyAnalyticsGate`, and it runs one way only — see `isOpen`.
@MainActor
enum CharacterisationEnrolmentGate {

    private static let log = Logger(subsystem: "life.neurone.analytics",
                                    category: "characterisation-gate")

    /// Distinct from `WarrantyAnalyticsGate.warrantyConsentKey` by design — a
    /// shared key would make the two consents one consent.
    static let enrolmentKey = "np.characterisation.enrolled"

    /// Consent generation. Incremented on every `grant()`, never reset by
    /// `withdraw()`. It travels with each uploaded record so a row cannot
    /// outlive the grant that authorised it: a device that withdrew and later
    /// re-enrolled produces records under a new epoch, and rows from the old one
    /// are identifiable for deletion without needing a clock to order them.
    static let consentEpochKey = "np.characterisation.consent-epoch"

    /// The programme this device is enrolled in. Must match
    /// `NP_ACCEL_CHAR_PROGRAMME_ID` in `firmware/shdr/include/np_accel_shdr.h`.
    /// Consent to programme N does not authorise programme N+1; the firmware
    /// rejects a mismatch and emits a standard-only record.
    static let programmeIdKey = "np.characterisation.programme-id"

    /// Current programme generation. A new programme takes a new id AND requires
    /// fresh enrolment — it does not inherit this one's participants.
    static let currentProgrammeId = 1

    /// True only when the warranty owner has affirmatively enrolled **and**
    /// warranty consent itself is still open.
    ///
    /// The `WarrantyAnalyticsGate.isOpen` conjunct is a strict subset relation,
    /// not a merge: characterisation records travel on the SHDR upload path, so
    /// an owner who has revoked SHDR telemetry altogether cannot still be
    /// contributing the *more* detailed set. It runs one way only — enrolling
    /// here never grants warranty consent, and this gate never writes
    /// `WarrantyAnalyticsGate`'s key.
    static var isOpen: Bool {
        guard WarrantyAnalyticsGate.isOpen else { return false }
        guard UserDefaults.standard.bool(forKey: enrolmentKey) else { return false }
        guard UserDefaults.standard.integer(forKey: programmeIdKey) == currentProgrammeId else {
            return false
        }
        return consentEpoch >= 1
    }

    /// Non-zero only after a grant. Zero means "never granted", which the
    /// firmware treats as not enrolled.
    static var consentEpoch: Int {
        UserDefaults.standard.integer(forKey: consentEpochKey)
    }

    /// Record an affirmative enrolment by the warranty owner.
    ///
    /// Must be called only from the §H enrolment screen after an explicit
    /// choice — never from onboarding defaults, never from warranty
    /// registration, and never as a side effect of any other setting. Returns
    /// the new consent epoch to be written to the device's enrolment record.
    ///
    /// - Precondition: `WarrantyAnalyticsGate.isOpen`. Enrolling a device whose
    ///   owner has not consented to SHDR telemetry at all would be incoherent,
    ///   so it is refused rather than silently recorded.
    @discardableResult
    static func grant() -> Int? {
        guard WarrantyAnalyticsGate.isOpen else {
            log.error("CharacterisationEnrolmentGate.grant() refused — warranty consent is not open.")
            return nil
        }
        let epoch = consentEpoch + 1
        UserDefaults.standard.set(true, forKey: enrolmentKey)
        UserDefaults.standard.set(epoch, forKey: consentEpochKey)
        UserDefaults.standard.set(currentProgrammeId, forKey: programmeIdKey)
        log.debug("Characterisation enrolment recorded — epoch \(epoch), programme \(currentProgrammeId).")
        return epoch
    }

    /// Withdraw from the characterisation programme.
    ///
    /// Collection stops from the next session gap, and that device's extended
    /// rows are deleted at the next sync (§H.5). What withdrawal cannot do is
    /// un-train a model that has already learned from a contribution — the
    /// enrolment copy must say so plainly at enrolment time rather than at the
    /// point someone tries to leave (OI-EMMC2-12).
    ///
    /// The consent epoch is deliberately NOT reset: it must keep increasing so
    /// that a later re-enrolment is distinguishable from this one.
    ///
    /// Has no effect on `WarrantyAnalyticsGate`, `ConsentStore`,
    /// `ResearchAnalyticsGate`, or any UHDR flow.
    static func withdraw() {
        UserDefaults.standard.set(false, forKey: enrolmentKey)
        log.debug("Characterisation enrolment withdrawn — extended collection stops next gap.")
    }

    /// Clear enrolment entirely on factory reset / change of custody.
    ///
    /// Reset marks a change of custody (`NP-FW-EMMC-002` §A.6,
    /// `device_transferred`), and a consent decision made by a previous
    /// registrant must not keep authorising collection for a new one
    /// (`NP-MOD-ID-001` §A.4). A device coming out of reset starts un-enrolled
    /// and asks again — including the epoch, because the new owner's first
    /// enrolment is genuinely their first.
    static func clearForDeviceTransfer() {
        UserDefaults.standard.removeObject(forKey: enrolmentKey)
        UserDefaults.standard.removeObject(forKey: consentEpochKey)
        UserDefaults.standard.removeObject(forKey: programmeIdKey)
        log.debug("Characterisation enrolment cleared for device transfer.")
    }
}
