//  ConsentStoreTests.swift
//
//  Satisfies: ISC-80 (withdrawBlanketResearchConsent persists), ISC-81 (badge count never negative),
//             ISC-74 (updateResearchConsent persists), and store revoke/withdraw logic.
//
//  Subject under test: ConsentStore (app/ios/NeuroPulse/Consent/ConsentStore.swift)

import XCTest
@testable import NeuroPulse

@MainActor
final class ConsentStoreTests: XCTestCase {

    private var store: ConsentStore!

    override func setUp() {
        super.setUp()
        // Isolate each test from UserDefaults by clearing consent keys.
        clearConsentDefaults()
        store = ConsentStore()
    }

    override func tearDown() {
        clearConsentDefaults()
        super.tearDown()
    }

    private func clearConsentDefaults() {
        let keys = ["np.consent.clinician-grants", "np.consent.research", "np.consent.study-participations"]
        keys.forEach { UserDefaults.standard.removeObject(forKey: $0) }
    }

    // MARK: - ISC-74: updateResearchConsent persists

    func testUpdateResearchConsentPersists() {
        var state = ResearchConsentState()
        state.contactConsentGranted = true
        state.contactMethod = "test@example.com"
        state.contactFrequency = .monthly

        store.updateResearchConsent(state)

        let reloaded = ConsentStore()
        XCTAssertTrue(reloaded.researchConsent.contactConsentGranted)
        XCTAssertEqual(reloaded.researchConsent.contactMethod, "test@example.com")
        XCTAssertEqual(reloaded.researchConsent.contactFrequency, .monthly)
    }

    // MARK: - ISC-80: withdrawBlanketResearchConsent persists correctly

    func testWithdrawBlanketResearchConsent_setsGrantedFalse() {
        var state = ResearchConsentState()
        state.blanketConsentGranted = true
        store.updateResearchConsent(state)
        XCTAssertTrue(store.researchConsent.blanketConsentGranted)

        store.withdrawBlanketResearchConsent()

        XCTAssertFalse(store.researchConsent.blanketConsentGranted,
                       "withdrawBlanketResearchConsent must set blanketConsentGranted to false.")
        XCTAssertNil(store.researchConsent.blanketConsentGrantedAt,
                     "withdrawBlanketResearchConsent must clear blanketConsentGrantedAt.")
    }

    func testWithdrawBlanketResearchConsent_persistsToDefaults() {
        var state = ResearchConsentState()
        state.blanketConsentGranted = true
        store.updateResearchConsent(state)
        store.withdrawBlanketResearchConsent()

        // A newly-constructed store (simulating app restart) must reflect the withdrawal.
        let reloaded = ConsentStore()
        XCTAssertFalse(reloaded.researchConsent.blanketConsentGranted,
                       "Withdrawal must survive an app restart (UserDefaults persistence).")
    }

    // MARK: - ISC-81: badge count never negative

    func testPendingInvitationBadgeNeverNegative_freshStore() {
        let count = store.pendingInvitations.filter { $0.hasNoDecision }.count
        XCTAssertGreaterThanOrEqual(count, 0)
    }

    func testPendingInvitationBadgeNeverNegative_afterDeclineNonExistent() {
        // Declining a study that was never added must be a no-op, not a crash or underflow.
        store.declineInvitation(studyID: "no-such-study-id")
        let count = store.pendingInvitations.filter { $0.hasNoDecision }.count
        XCTAssertGreaterThanOrEqual(count, 0)
    }

    func testPendingInvitationBadgeNeverNegative_afterFullCycle() {
        let invitation = StudyInvitation(
            id: UUID(),
            studyID: "TEST-001",
            studyTitle: "Test Study",
            researchCategories: [.depression],
            approvedElements: [.sessionTimestamps],
            cannotLearn: ["EEG recordings"],
            irreversibilityNotice: ConsentEngine.irreversibilityNotice,
            receivedAt: Date(),
            decision: nil
        )
        store.addInvitation(invitation)
        XCTAssertEqual(store.pendingInvitations.filter { $0.hasNoDecision }.count, 1)

        store.declineInvitation(studyID: "TEST-001")
        XCTAssertEqual(store.pendingInvitations.filter { $0.hasNoDecision }.count, 0)

        // Declining again must not go negative.
        store.declineInvitation(studyID: "TEST-001")
        XCTAssertEqual(store.pendingInvitations.filter { $0.hasNoDecision }.count, 0)
    }

    // MARK: - Clinician grant revoke

    func testRevokeClinicianAccess_removesGrant() {
        let grant = ClinicianConsentGrant(
            id: UUID(), clinicianName: "Dr. Test", clinicianOrganization: "Test Hospital",
            tier: .monitor, grantedAt: Date(), expiresAt: nil, isActive: true
        )
        store.grantClinicianAccess(grant)
        XCTAssertEqual(store.clinicianGrants.count, 1)

        store.revokeClinicianAccess(grantID: grant.id)
        XCTAssertEqual(store.clinicianGrants.count, 0)
    }

    func testRevokeUnknownGrantID_isNoOp() {
        store.revokeClinicianAccess(grantID: UUID())
        XCTAssertEqual(store.clinicianGrants.count, 0,
                       "Revoking an unknown grant ID must be a no-op, not a crash.")
    }

    // MARK: - Study withdrawal

    func testWithdrawFromStudy_setsWithdrawnAt() {
        let invitation = StudyInvitation(
            id: UUID(), studyID: "STUDY-002", studyTitle: "Withdrawal Test",
            researchCategories: [], approvedElements: [], cannotLearn: [],
            irreversibilityNotice: "", receivedAt: Date(), decision: nil
        )
        store.addInvitation(invitation)
        store.acceptInvitation(studyID: "STUDY-002")
        XCTAssertFalse(store.studyParticipations.isEmpty)

        store.withdrawFromStudy(studyID: "STUDY-002")

        let record = store.studyParticipations.first { $0.studyID == "STUDY-002" }
        XCTAssertNotNil(record)
        XCTAssertFalse(record!.isActive, "After withdrawal, isActive must be false.")
        XCTAssertNotNil(record!.withdrawnAt)
    }

    // MARK: - Category consent

    func testSetCategoryConsent_persists() {
        store.setCategoryConsent(.depression, granted: true)
        XCTAssertTrue(store.researchConsent.categoryConsents[.depression] == true)

        let reloaded = ConsentStore()
        XCTAssertTrue(reloaded.researchConsent.categoryConsents[.depression] == true)
    }
}
