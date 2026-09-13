import Foundation

// The clinician-portal channel — CLAUDE.md §6.1, NP-SW-PORTAL-API-001 §4(c).
//
// §6.1's expansion workflow has a clinician raise a request, the user decide it, and — for the
// third response — a question travel back. `OI-CONSENT-02` built the user's half. This file is the
// port the other half plugs into, and the reason it is a port rather than a transport is that the
// thing that would authenticate a request does not exist yet: a clinician-identity anchor, so a
// request can be shown to have come from the clinician who holds the grant.
//
// Deliberately the same shape `OI-CONSENT-03` gave the study-descriptor side. A port whose default
// implementation REFUSES makes the gap impossible to close by accident: a transport cannot be
// wired up without also supplying the identity check, and a half-built channel fails closed rather
// than ingesting whatever arrives.

/// Whether a portal-supplied request is the clinician it claims to be.
///
/// `unavailable` is not `rejected`. A signature that cannot be checked — no key, no verifier — is
/// a different fact from one that was checked and failed, and collapsing them would let a missing
/// key read as an attack, or worse, an attack read as a missing key.
enum ClinicianRequestVerification: Equatable {
    case verified
    case rejected
    case unavailable(reason: String)
}

/// Port for the clinician portal's device-facing channel. One method, because one thing crosses
/// the boundary inbound: a request, and whether it is genuinely from the clinician.
protocol ClinicianPortalChannel {
    func verify(_ request: ClinicianAccessExpansionRequest) -> ClinicianRequestVerification
}

/// The only channel that ships today. It refuses every request and says why.
///
/// Honest rather than permissive: with no clinician-identity anchor there is nothing to check a
/// request against, and a verifier that returned `.verified` on that basis would be asserting
/// something nobody can know. `NP-SW-PORTAL-API-001` §4(c) and `OI-CONSENT-05`.
struct RefusingClinicianPortalChannel: ClinicianPortalChannel {
    func verify(_ request: ClinicianAccessExpansionRequest) -> ClinicianRequestVerification {
        .unavailable(reason: "no clinician-identity anchor is configured (OI-CONSENT-05)")
    }
}

/// What became of a request offered to the device.
enum ClinicianRequestAdmission: Equatable {
    case admitted
    /// The channel said the request is not from the clinician it names.
    case refusedForgedRequest
    /// The channel could not tell — no key, no verifier. Carries the channel's own reason.
    case refusedUnverifiable(reason: String)
    /// No grant with that ID on this device. Checked locally and needing no network opinion: the
    /// device issued its own grants and knows which they are.
    case refusedUnknownGrant
    /// The tier change is not an expansion — it adds nothing, or removes something, or touches the
    /// Research tier. `ConsentEngine.accessDifferential` decides, and a request it refuses has no
    /// consent document to show, so there is nothing the user could usefully be asked.
    case refusedNotAnExpansion
}

/// Ingests clinician requests into `ConsentStore`, in an order that is load-bearing.
///
/// **Identity first, then the device's own records, then the differential.** Nothing an unverified
/// request *claims* may influence a later step: a forged request naming a grant that does not exist
/// is refused as forged, not as unknown, so the refusal never confirms which grants this device
/// holds. That is CLAUDE.md §5.1's rule 2 in the ingestion path — a refusal whose shape varies with
/// a fact the caller should not learn is a way of telling them that fact.
@MainActor
struct ClinicianPortalSync {
    let channel: ClinicianPortalChannel
    let store: ConsentStore

    init(store: ConsentStore, channel: ClinicianPortalChannel = RefusingClinicianPortalChannel()) {
        self.store = store
        self.channel = channel
    }

    @discardableResult
    func ingest(_ request: ClinicianAccessExpansionRequest) -> ClinicianRequestAdmission {
        switch channel.verify(request) {
        case .rejected:
            return .refusedForgedRequest
        case let .unavailable(reason):
            return .refusedUnverifiable(reason: reason)
        case .verified:
            break
        }

        guard let grant = store.clinicianGrants.first(where: { $0.id == request.grantID })
        else { return .refusedUnknownGrant }

        guard ConsentEngine.accessDifferential(for: grant, expandingTo: request.toTier) != nil
        else { return .refusedNotAnExpansion }

        store.addExpansionRequest(request)
        return .admitted
    }
}
