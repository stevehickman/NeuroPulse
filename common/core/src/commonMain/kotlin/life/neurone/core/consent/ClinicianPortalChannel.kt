package life.neurone.core.consent

import life.neurone.core.models.ClinicianAccessExpansionRequest

// The clinician-portal channel — CLAUDE.md §6.1, NP-SW-PORTAL-API-001 §4(c).
// Peer of iOS `Consent/ClinicianPortalChannel.swift`; see that file's header for the argument.

/**
 * Whether a portal-supplied request is the clinician it claims to be.
 *
 * [Unavailable] is not [Rejected]. A signature that cannot be checked — no key, no verifier — is a
 * different fact from one that was checked and failed, and collapsing them would let a missing key
 * read as an attack, or worse, an attack read as a missing key.
 */
sealed interface ClinicianRequestVerification {
    data object Verified : ClinicianRequestVerification
    data object Rejected : ClinicianRequestVerification
    data class Unavailable(val reason: String) : ClinicianRequestVerification
}

/**
 * Port for the clinician portal's device-facing channel. One method, because one thing crosses the
 * boundary inbound: a request, and whether it is genuinely from the clinician.
 */
fun interface ClinicianPortalChannel {
    fun verify(request: ClinicianAccessExpansionRequest): ClinicianRequestVerification
}

/**
 * The only channel that ships today. It refuses every request and says why.
 *
 * Honest rather than permissive: with no clinician-identity anchor there is nothing to check a
 * request against, and a verifier that returned [ClinicianRequestVerification.Verified] on that
 * basis would be asserting something nobody can know. `NP-SW-PORTAL-API-001` §4(c),
 * `OI-CONSENT-05`.
 */
class RefusingClinicianPortalChannel : ClinicianPortalChannel {
    override fun verify(request: ClinicianAccessExpansionRequest): ClinicianRequestVerification =
        ClinicianRequestVerification.Unavailable(
            "no clinician-identity anchor is configured (OI-CONSENT-05)",
        )
}

/** What became of a request offered to the device. */
sealed interface ClinicianRequestAdmission {
    data object Admitted : ClinicianRequestAdmission

    /** The channel said the request is not from the clinician it names. */
    data object RefusedForgedRequest : ClinicianRequestAdmission

    /** The channel could not tell — no key, no verifier. Carries the channel's own reason. */
    data class RefusedUnverifiable(val reason: String) : ClinicianRequestAdmission

    /**
     * No grant with that ID on this device. Checked locally and needing no network opinion: the
     * device issued its own grants and knows which they are.
     */
    data object RefusedUnknownGrant : ClinicianRequestAdmission

    /**
     * The tier change is not an expansion — it adds nothing, or removes something, or touches the
     * Research tier. [ConsentEngine.accessDifferential] decides, and a request it refuses has no
     * consent document to show, so there is nothing the user could usefully be asked.
     */
    data object RefusedNotAnExpansion : ClinicianRequestAdmission
}

/**
 * Ingests clinician requests into [ConsentStore], in an order that is load-bearing.
 *
 * **Identity first, then the device's own records, then the differential.** Nothing an unverified
 * request *claims* may influence a later step: a forged request naming a grant that does not exist
 * is refused as forged, not as unknown, so the refusal never confirms which grants this device
 * holds. That is CLAUDE.md §5.1's rule 2 in the ingestion path — a refusal whose shape varies with
 * a fact the caller should not learn is a way of telling them that fact.
 */
class ClinicianPortalSync(
    private val store: ConsentStore,
    private val channel: ClinicianPortalChannel = RefusingClinicianPortalChannel(),
) {
    fun ingest(request: ClinicianAccessExpansionRequest): ClinicianRequestAdmission {
        when (val verification = channel.verify(request)) {
            is ClinicianRequestVerification.Rejected ->
                return ClinicianRequestAdmission.RefusedForgedRequest
            is ClinicianRequestVerification.Unavailable ->
                return ClinicianRequestAdmission.RefusedUnverifiable(verification.reason)
            is ClinicianRequestVerification.Verified -> Unit
        }

        val grant = store.clinicianGrants.firstOrNull { it.id == request.grantId }
            ?: return ClinicianRequestAdmission.RefusedUnknownGrant

        ConsentEngine.accessDifferential(grant, request.toTier)
            ?: return ClinicianRequestAdmission.RefusedNotAnExpansion

        store.addExpansionRequest(request)
        return ClinicianRequestAdmission.Admitted
    }
}
