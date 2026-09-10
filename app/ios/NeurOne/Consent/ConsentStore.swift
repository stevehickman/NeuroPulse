import Foundation
import Combine

// Persisted consent state — clinician grants, research consent, study participation audit trail.
// All state serialized to UserDefaults (encrypted by iOS file system protection).
// UHDR/SHDR boundary: study audit trail written to SHDR (study ID, hash, timestamp, byte count).
//
// TWO CONSENT SUBJECTS:
//   • Warranty owner — consent for SHDR fleet telemetry. Granted at warranty registration.
//     The warranty owner may be a clinic or institution, not the end user wearing the device.
//     SHDRUploader holds this consent; it is unrelated to any individual user.
//   • User — consent for UHDR research data flows. Managed here. Four layers:
//       L1 contact · L2 category · L3 blanket · L4 results + community.
//     Layers are not screens — since CLAUDE.md Rev 37 the four layers are presented across
//     two screens (S1 = L4 + L1, S2 = L2 + L3). The layer identities are what this store,
//     the withdrawal surfaces and the document set all key on.
//     Revoking research consent at ANY scope immediately stops data flows for that scope.
//     Revoking blanket research consent (L3) also tears down app analytics (PostHog),
//     because blanket withdrawal signals the user does not want any data collection.
//     That coupling is enforced in BOTH the named withdrawal method and the UI commit path
//     (`updateResearchConsent`), because S2 commits L2 and L3 together — see §6.2.5.

@MainActor
final class ConsentStore: ObservableObject {

    @Published private(set) var clinicianGrants: [ClinicianConsentGrant] = []
    @Published private(set) var researchConsent: ResearchConsentState = ResearchConsentState()
    @Published private(set) var studyParticipations: [StudyParticipationRecord] = []
    @Published private(set) var pendingInvitations: [StudyInvitation] = []
    /// Clinician access-expansion requests (§6.1), decided and undecided alike — a denial is part
    /// of the record. The UI shows `expansionRequests.filter(\.isPending)`.
    @Published private(set) var expansionRequests: [ClinicianAccessExpansionRequest] = []

    private let grantsKey        = "np.consent.clinician-grants"
    private let researchKey      = "np.consent.research"
    private let participationKey = "np.consent.study-participations"
    private let expansionsKey    = "np.consent.clinician-expansions"
    private let invitationsKey   = "np.consent.study-invitations"

    /// Checks the signature on every study descriptor before anything derived from it reaches the
    /// user (§5.3 step 1). The default refuses everything: see `RefusingStudyDescriptorVerifier`.
    private let descriptorVerifier: StudyDescriptorVerifier

    init(descriptorVerifier: StudyDescriptorVerifier = RefusingStudyDescriptorVerifier()) {
        self.descriptorVerifier = descriptorVerifier
        load()
    }

    // MARK: - Clinician consent

    func grantClinicianAccess(_ grant: ClinicianConsentGrant) {
        clinicianGrants.removeAll { $0.id == grant.id }
        clinicianGrants.append(grant)
        save()
    }

    func revokeClinicianAccess(grantID: UUID) {
        clinicianGrants.removeAll { $0.id == grantID }
        // An outstanding request to widen a grant that no longer exists is a notification the user
        // can only answer wrongly, so it goes with the grant. Decided requests stay: they are the
        // record of what was asked and answered.
        expansionRequests.removeAll { $0.grantID == grantID && $0.isPending }
        save()
    }

    // MARK: - Clinician access expansion (§6.1)
    //
    // §6.1's workflow is: differential consent document → persistent user notification → user
    // approves / denies / asks questions → retroactive access is a SEPARATE decision, "presented
    // as separate consent decisions even if made simultaneously".
    //
    // Before this existed, `expandClinicianAccess` was a public method that set `tier` and
    // nothing else, with no caller anywhere (OI-CONSENT-02). That is not merely an unimplemented
    // workflow: because `approvedElements` derives from `tier` and a tier is timeless, raising it
    // hands the clinician the new elements over every session ever recorded — silently taking the
    // retroactive decision §6.1 requires to be asked separately, and taking it in the affirmative.
    // The mutation is now private, and the only way to reach it is a request the user decided.

    /// Ingest a clinician's request to widen a grant. No UI caller: requests arrive from the
    /// clinician-portal sync layer, which does not exist yet (`OI-CONSENT-05`) — the sibling of
    /// the study-service transport that leaves `ingestStudyDescriptor` without one.
    ///
    /// A new request from the same grant supersedes that grant's outstanding one; decided
    /// requests are left alone, because they are history rather than an inbox.
    func addExpansionRequest(_ request: ClinicianAccessExpansionRequest) {
        expansionRequests.removeAll { $0.grantID == request.grantID && $0.isPending }
        expansionRequests.append(request)
        save()
    }

    /// Record the user's approval and apply it.
    ///
    /// `includePriorData` is the second of the two §6.1 decisions and arrives from its own
    /// control on its own step; it is stored beside the approval rather than folded into it, so
    /// the record can still say the user approved the expansion *and* refused it over history.
    func approveExpansion(requestID: UUID, includePriorData: Bool) {
        guard let idx = expansionRequests.firstIndex(where: { $0.id == requestID }),
              expansionRequests[idx].isPending else { return }
        let request = expansionRequests[idx]

        // Fail closed on a stale request: the grant may have been revoked, or already widened past
        // this tier, since the request was raised. Approving what the differential document no
        // longer describes would apply a change the user was not shown.
        guard let grant = clinicianGrants.first(where: { $0.id == request.grantID }),
              ConsentEngine.accessDifferential(for: grant, expandingTo: request.toTier) != nil
        else { return }

        let now = Date()
        expansionRequests[idx].decision = .approved(at: now, includesPriorData: includePriorData)
        expandClinicianAccess(
            grantID: request.grantID,
            to: request.toTier,
            includePriorData: includePriorData,
            decidedAt: now
        )
    }

    /// Record a denial. Nothing about the grant changes — denying is the reversible direction, and
    /// the clinician may raise a fresh request.
    func denyExpansion(requestID: UUID) {
        guard let idx = expansionRequests.firstIndex(where: { $0.id == requestID }),
              expansionRequests[idx].isPending else { return }
        expansionRequests[idx].decision = .denied(at: Date())
        save()
    }

    /// Record that the user asked a question rather than deciding (§6.1's third response).
    ///
    /// The request stays pending: asking is not answering, and the notification must not clear as
    /// though the user had decided. Delivering the question to the clinician needs the same
    /// missing outbound channel as `addExpansionRequest` (`OI-CONSENT-05`); until it exists the UI
    /// says so plainly rather than implying a message was sent.
    func askQuestionAboutExpansion(requestID: UUID) {
        guard let idx = expansionRequests.firstIndex(where: { $0.id == requestID }),
              expansionRequests[idx].isPending else { return }
        expansionRequests[idx].decision = .questionSent(at: Date())
        save()
    }

    /// Apply an approved expansion. Private, and deliberately so: §6.1 makes the consent document
    /// and the two decisions preconditions of the mutation, and a method that can widen a grant
    /// without them is a standing invitation to skip them.
    ///
    /// The new elements are appended as their own `ClinicianAccessScope` rather than folded into
    /// the tier alone, so the retroactive answer survives in the record. The tier still moves —
    /// it is what the subscription and the price are keyed to — but it is no longer the only thing
    /// deciding what the clinician can see.
    private func expandClinicianAccess(
        grantID: UUID,
        to newTier: ClinicianUseCaseTier,
        includePriorData: Bool,
        decidedAt: Date
    ) {
        guard let idx = clinicianGrants.firstIndex(where: { $0.id == grantID }) else { return }
        let grant = clinicianGrants[idx]
        let newlyVisible = newTier.uhdrElements.subtracting(grant.approvedElements)

        clinicianGrants[idx].accessScopes = grant.effectiveScopes + [
            ClinicianAccessScope(
                elements: newlyVisible,
                effectiveFrom: decidedAt,
                includesPriorData: includePriorData
            )
        ]
        clinicianGrants[idx].tier = newTier
        save()
    }

    // MARK: - Research consent

    /// Commit a research-consent state produced by the consent UI.
    ///
    /// This is the single ingestion point for every UI-driven research-consent change, so the
    /// blanket→analytics coupling is enforced here rather than only in the explicitly-named
    /// `withdrawBlanketResearchConsent()`. Before CLAUDE.md Rev 37 nothing on iOS called that
    /// method from the UI: turning blanket consent off in Research Preferences committed through
    /// this method and silently skipped the teardown. Merging L2 and L3 onto one
    /// commit-at-the-end screen (§6.2) made that bypass the ordinary path, so the guard moved
    /// upstream to where the state actually enters the store.
    ///
    /// The guard keys on the **transition**, not the value. `blanketConsentGranted == false` is
    /// true for a user who never granted it and for a user who just revoked it; only the second
    /// is a withdrawal. Testing the value instead would tear down analytics on every
    /// category-only edit — the 2026-06-16 regression inverted.
    func updateResearchConsent(_ state: ResearchConsentState) {
        let wasBlanketGranted = researchConsent.blanketConsentGranted
        researchConsent = state
        save()

        if wasBlanketGranted && !state.blanketConsentGranted {
            revokeResearchAnalytics()
        }
    }

    func withdrawBlanketResearchConsent() {
        researchConsent.blanketConsentGranted = false
        researchConsent.blanketConsentGrantedAt = nil
        save()
        // Withdrawal immediately prevents future study descriptor processing.
        // Already-published extracts are unchanged (irreversibility notice given at L3).
        //
        // Blanket withdrawal also revokes research analytics: the user is signaling they
        // do not want any data collection beyond basic device function.
        // (Partial withdrawals — specific study or category — do not revoke research analytics
        // because the user remains a research participant in other scopes.)
        revokeResearchAnalytics()
    }

    /// Revoke research analytics — clears the research analytics gate key and tears
    /// down the SDK so it cannot collect passively after withdrawal.
    ///
    /// Called from: (1) dedicated analytics opt-out toggle in Settings;
    ///              (2) `withdrawBlanketResearchConsent()` — blanket research withdrawal
    ///                   implies full data-collection opt-out;
    ///              (3) `updateResearchConsent(_:)` on a blanket true→false transition, which
    ///                   is how the UI commit path reaches the same rule (§6.2.5).
    /// Idempotent: clearing an already-cleared key is a no-op and `ResearchAnalyticsGate.reset()`
    /// guards on `isConfigured`, so (2) and (3) overlapping is harmless.
    ///
    /// Does NOT affect `WarrantyAnalyticsGate` or SHDR fleet uploads.
    func revokeResearchAnalytics() {
        UserDefaults.standard.removeObject(forKey: ResearchAnalyticsGate.researchAnalyticsKey)
        ResearchAnalyticsGate.reset()
    }

    func setCategoryConsent(_ category: ResearchCategory, granted: Bool) {
        researchConsent.categoryConsents[category] = granted
        save()
    }

    // MARK: - Study invitations (§6.3 per-project workflow)
    //
    // Ingestion used to be `addInvitation(_ invitation: StudyInvitation)`: a finished invitation,
    // plain-language "what they cannot see" list and irreversibility notice included, taken on
    // trust from whatever called it, held in memory only, and called from nothing but a unit test
    // (`OI-CONSENT-03`). Like `expandClinicianAccess` before it, that was not merely an
    // unimplemented workflow — it took decisions silently and took them all in the affirmative:
    // that the descriptor's signature need not be checked, that §5.3's anonymisation floors need
    // not be met, that the user's own L1/L2/L3 state need not be consulted, and that the party
    // asking for access may write the sentence describing what it cannot see.
    //
    // What enters the device now is the signed descriptor. The invitation is derived from it.

    /// Ingest a signed study descriptor (§5.3 step 1) and, if it is admissible, put the invitation
    /// derived from it in front of the user.
    ///
    /// **No UI caller, and none is expected**: descriptors arrive from the NeurOne study service,
    /// which does not exist yet (`OI-CONSENT-07`). With no verifier injected this refuses every
    /// descriptor with `.verifierUnavailable`, which is the honest description of a device that
    /// has no signing key — not a silent no-op.
    ///
    /// Returns the admission so the caller can tell a refusal from a delivery. Nothing is
    /// persisted on a refusal: a descriptor the device would not show the user is not a record of
    /// anything the user did.
    @discardableResult
    func ingestStudyDescriptor(_ descriptor: StudyDescriptor) -> StudyDescriptorAdmission {
        let admission = ConsentEngine.admit(
            descriptor: descriptor,
            verification: descriptorVerifier.verify(descriptor),
            consent: researchConsent,
            studyAlreadyDecided: hasDecided(studyID: descriptor.studyID)
        )
        guard case let .admitted(posture, descriptorHash) = admission else { return admission }

        let now = Date()
        pendingInvitations.removeAll { $0.studyID == descriptor.studyID }
        pendingInvitations.append(
            ConsentEngine.invitation(
                from: descriptor, posture: posture, descriptorHash: descriptorHash, receivedAt: now
            )
        )

        // An engagement notification is not a question (§6.2 L3): the user pre-approved this
        // study, so they are in it from the moment it arrives, and the audit trail has to say so.
        // Recording participation only on an explicit acceptance would leave an L3 user
        // participating in studies their own dashboard did not list.
        if posture == .engagementNotification {
            recordParticipation(studyID: descriptor.studyID, descriptorHash: descriptorHash, at: now)
        }
        save()
        return admission
    }

    /// Whether this study already has an answer on this device — decided, joined, or withdrawn
    /// from. §5.3 and §6.3 step 6 make withdrawal block future descriptor processing, and a
    /// device that re-presented a withdrawn study would be forgetting an answer the user gave.
    private func hasDecided(studyID: String) -> Bool {
        pendingInvitations.contains { $0.studyID == studyID && !$0.isOpen }
            || studyParticipations.contains { $0.studyID == studyID }
    }

    /// §6.3 step 4, *Yes*. Only a consent request can be accepted: an engagement notification was
    /// never a question, and accepting it would record a second participation for a study the
    /// user is already in.
    func acceptInvitation(studyID: String) {
        guard let idx = pendingInvitations.firstIndex(where: { $0.studyID == studyID }),
              pendingInvitations[idx].posture == .consentRequest,
              pendingInvitations[idx].isOpen
        else { return }
        let now = Date()
        pendingInvitations[idx].decision = .accepted(at: now)
        recordParticipation(
            studyID: studyID,
            descriptorHash: pendingInvitations[idx].descriptorHash,
            at: now
        )
        save()
    }

    /// §6.3 step 4, *No* — and the L3 per-study opt-out, which is the same intent reached from
    /// the other posture. For an engagement notification the user is already in the study, so
    /// declining has to withdraw the participation ingestion recorded; for a consent request
    /// there is nothing to withdraw.
    func declineInvitation(studyID: String) {
        guard let idx = pendingInvitations.firstIndex(where: { $0.studyID == studyID }),
              pendingInvitations[idx].isOpen
        else { return }
        pendingInvitations[idx].decision = .declined(at: Date())
        if pendingInvitations[idx].posture == .engagementNotification {
            withdrawFromStudy(studyID: studyID)
        }
        save()
    }

    /// §6.3 step 4's third response — *Ask a question* (secure message to a NeurOne liaison,
    /// 2 business day response).
    ///
    /// The invitation stays open, exactly as `askQuestionAboutExpansion` leaves an expansion
    /// request pending: asking is not deciding, and the invitation must not clear as though the
    /// user had answered. Delivering the question needs the same absent channel as the descriptor
    /// fetch (`OI-CONSENT-07`); until it exists the UI says so plainly rather than implying a
    /// message was sent.
    func askQuestionAboutInvitation(studyID: String) {
        guard let idx = pendingInvitations.firstIndex(where: { $0.studyID == studyID }),
              pendingInvitations[idx].posture == .consentRequest,
              pendingInvitations[idx].isOpen
        else { return }
        pendingInvitations[idx].decision = .questionSent(at: Date())
        save()
    }

    // MARK: - Participation audit (SHDR)

    /// Append the §5.3 audit-trail record for a study the device has joined.
    ///
    /// Idempotent per study: a second record for a study already in the trail would make the
    /// dashboard list one study twice and give `withdrawFromStudy` two rows to find. Before
    /// invitations were persisted this was reachable by ordinary use — an app restart emptied
    /// `pendingInvitations` while the trail survived, so the same descriptor could be ingested
    /// and accepted again.
    private func recordParticipation(studyID: String, descriptorHash: String, at date: Date) {
        guard !studyParticipations.contains(where: { $0.studyID == studyID }) else { return }
        let record = StudyParticipationRecord(
            id: UUID(), studyID: studyID,
            descriptorHash: descriptorHash,
            transmittedAt: date, extractBytes: 0
        )
        studyParticipations.append(record)
        save()
    }

    func withdrawFromStudy(studyID: String) {
        if let idx = studyParticipations.firstIndex(where: { $0.studyID == studyID && $0.isActive }) {
            studyParticipations[idx].withdrawnAt = Date()
            save()
        }
    }

    // MARK: - Persistence

    private func load() {
        if let data = UserDefaults.standard.data(forKey: grantsKey),
           let decoded = try? JSONDecoder().decode([ClinicianConsentGrant].self, from: data) {
            clinicianGrants = decoded
        }
        if let data = UserDefaults.standard.data(forKey: researchKey),
           let decoded = try? JSONDecoder().decode(ResearchConsentState.self, from: data) {
            researchConsent = decoded
        }
        if let data = UserDefaults.standard.data(forKey: participationKey),
           let decoded = try? JSONDecoder().decode([StudyParticipationRecord].self, from: data) {
            studyParticipations = decoded
        }
        // Expansion requests persist because §6.1 calls for a *persistent* notification: one the
        // user can dismiss by backgrounding the app is not one.
        if let data = UserDefaults.standard.data(forKey: expansionsKey),
           let decoded = try? JSONDecoder().decode([ClinicianAccessExpansionRequest].self, from: data) {
            expansionRequests = decoded
        }
        // Invitations persist for the same reason expansion requests do, and for one more: §6.3
        // step 4 gives the user two business days for an answer to a question, which an inbox
        // emptied by the next app launch cannot hold. While they were in-memory only, an accepted
        // study's participation record outlived the invitation that recorded the acceptance, so
        // the trail and the inbox disagreed after every restart.
        if let data = UserDefaults.standard.data(forKey: invitationsKey),
           let decoded = try? JSONDecoder().decode([StudyInvitation].self, from: data) {
            pendingInvitations = decoded
        }
    }

    private func save() {
        if let data = try? JSONEncoder().encode(clinicianGrants) {
            UserDefaults.standard.set(data, forKey: grantsKey)
        }
        if let data = try? JSONEncoder().encode(researchConsent) {
            UserDefaults.standard.set(data, forKey: researchKey)
        }
        if let data = try? JSONEncoder().encode(studyParticipations) {
            UserDefaults.standard.set(data, forKey: participationKey)
        }
        if let data = try? JSONEncoder().encode(expansionRequests) {
            UserDefaults.standard.set(data, forKey: expansionsKey)
        }
        if let data = try? JSONEncoder().encode(pendingInvitations) {
            UserDefaults.standard.set(data, forKey: invitationsKey)
        }
    }
}
