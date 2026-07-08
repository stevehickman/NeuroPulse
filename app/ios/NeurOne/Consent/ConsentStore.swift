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
//   • User — consent for UHDR research data flows. Managed here. Scoped to:
//       L1 contact consent → L2 category consent → L3 blanket consent.
//     Revoking research consent at ANY scope immediately stops data flows for that scope.
//     Revoking blanket research consent (L3) also tears down app analytics (PostHog),
//     because blanket withdrawal signals the user does not want any data collection.

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

    func updateResearchConsent(_ state: ResearchConsentState) {
        researchConsent = state
        save()
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
    ///                   implies full data-collection opt-out.
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
