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

    private let grantsKey        = "np.consent.clinician-grants"
    private let researchKey      = "np.consent.research"
    private let participationKey = "np.consent.study-participations"

    init() { load() }

    // MARK: - Clinician consent

    func grantClinicianAccess(_ grant: ClinicianConsentGrant) {
        clinicianGrants.removeAll { $0.id == grant.id }
        clinicianGrants.append(grant)
        save()
    }

    func revokeClinicianAccess(grantID: UUID) {
        clinicianGrants.removeAll { $0.id == grantID }
        save()
    }

    func expandClinicianAccess(grantID: UUID, to newTier: ClinicianUseCaseTier) {
        guard let idx = clinicianGrants.firstIndex(where: { $0.id == grantID }) else { return }
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

    // MARK: - Study invitations

    func addInvitation(_ invitation: StudyInvitation) {
        pendingInvitations.removeAll { $0.studyID == invitation.studyID }
        pendingInvitations.append(invitation)
    }

    func acceptInvitation(studyID: String) {
        guard let idx = pendingInvitations.firstIndex(where: { $0.studyID == studyID }) else { return }
        pendingInvitations[idx].decision = .accepted(at: Date())
        recordParticipation(studyID: studyID, descriptorHash: "pending")
    }

    func declineInvitation(studyID: String) {
        guard let idx = pendingInvitations.firstIndex(where: { $0.studyID == studyID }) else { return }
        pendingInvitations[idx].decision = .declined(at: Date())
    }

    // MARK: - Participation audit (SHDR)

    private func recordParticipation(studyID: String, descriptorHash: String) {
        let record = StudyParticipationRecord(
            id: UUID(), studyID: studyID,
            descriptorHash: descriptorHash,
            transmittedAt: Date(), extractBytes: 0
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
    }
}
