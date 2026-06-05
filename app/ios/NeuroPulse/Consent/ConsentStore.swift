import Foundation
import Combine

// Persisted consent state — clinician grants, research consent, study participation audit trail.
// All state serialised to UserDefaults (encrypted by iOS file system protection).
// UHDR/SHDR boundary: study audit trail written to SHDR (study ID, hash, timestamp, byte count).

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

    func withdrawBlanketConsent() {
        researchConsent.blanketConsentGranted = false
        researchConsent.blanketConsentGrantedAt = nil
        save()
        // Withdrawal immediately prevents future study descriptor processing.
        // Already-published extracts are unchanged (irreversibility notice given at L3).
        revokeAnalyticsConsent()
    }

    /// Revoke analytics consent entirely — clears the consent gate key and tears
    /// down the analytics SDK so it cannot collect passively after withdrawal.
    func revokeAnalyticsConsent() {
        UserDefaults.standard.removeObject(forKey: AnalyticsGate.consentAcceptedKey)
        AnalyticsGate.reset()
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
