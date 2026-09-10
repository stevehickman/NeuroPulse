//  ConsentStoreTests.swift
//
//  Satisfies: ISC-80 (withdrawBlanketResearchConsent persists), ISC-81 (badge count never negative),
//             ISC-74 (updateResearchConsent persists), and store revoke/withdraw logic.
//
//  Subject under test: ConsentStore (app/ios/NeurOne/Consent/ConsentStore.swift)

import XCTest
@testable import NeurOne

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
        let keys = ["np.consent.clinician-grants", "np.consent.research", "np.consent.study-participations",
                    "np.consent.clinician-expansions", "np.consent.study-invitations"]
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
        let count = store.pendingInvitations.filter(\.isOpen).count
        XCTAssertGreaterThanOrEqual(count, 0)
    }

    func testPendingInvitationBadgeNeverNegative_afterDeclineNonExistent() {
        // Declining a study that was never ingested must be a no-op, not a crash or underflow.
        store.declineInvitation(studyID: "no-such-study-id")
        XCTAssertGreaterThanOrEqual(store.pendingInvitations.filter(\.isOpen).count, 0)
    }

    func testPendingInvitationBadgeNeverNegative_afterFullCycle() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: categoryConsent)
        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor()),
                       .admitted(posture: .consentRequest, descriptorHash: hash))
        XCTAssertEqual(store.pendingInvitations.filter(\.isOpen).count, 1)

        store.declineInvitation(studyID: studyID)
        XCTAssertEqual(store.pendingInvitations.filter(\.isOpen).count, 0)

        // Declining again must not go negative.
        store.declineInvitation(studyID: studyID)
        XCTAssertEqual(store.pendingInvitations.filter(\.isOpen).count, 0)
    }

    // MARK: - Study descriptor ingestion (§6.3 / OI-CONSENT-03)
    //
    // The gate the old `addInvitation` did not have. Each refusal below is a decision that method
    // took silently and took in the affirmative.

    private let studyID = "TEST-001"
    private let hash = "sha256:abc"

    private var categoryConsent: ResearchConsentState {
        var state = ResearchConsentState()
        state.contactConsentGranted = true
        state.categoryConsents[.depression] = true
        return state
    }

    private var blanketConsent: ResearchConsentState {
        var state = ResearchConsentState()
        state.contactConsentGranted = true
        state.blanketConsentGranted = true
        return state
    }

    private struct StubVerifier: StudyDescriptorVerifier {
        let result: StudyDescriptorVerification
        func verify(_ descriptor: StudyDescriptor) -> StudyDescriptorVerification { result }
    }

    private func makeStore(
        verification: StudyDescriptorVerification,
        consent: ResearchConsentState
    ) -> ConsentStore {
        let store = ConsentStore(descriptorVerifier: StubVerifier(result: verification))
        store.updateResearchConsent(consent)
        return store
    }

    private func makeDescriptor(
        categories: [ResearchCategory] = [.depression],
        elements: Set<UHDRElement> = [.sessionTimestamps, .eegWaveforms],
        k: Int = 10,
        rounding: Int = 7
    ) -> StudyDescriptor {
        StudyDescriptor(
            studyID: studyID,
            studyTitle: "Test Study",
            researchCategories: categories,
            requestedElements: elements,
            kAnonymity: k,
            dateRoundingDays: rounding,
            issuedAt: Date(),
            signature: "sig"
        )
    }

    /// The state of every device today, and the reason the verifier is injected rather than
    /// assumed: with no signing key nothing can be checked, so nothing is shown.
    func testWithNoVerifierConfiguredEveryDescriptorIsRefused() {
        let store = ConsentStore()            // production default
        store.updateResearchConsent(blanketConsent)

        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor()), .refused(.verifierUnavailable))
        XCTAssertTrue(store.pendingInvitations.isEmpty)
        XCTAssertTrue(store.studyParticipations.isEmpty)
    }

    func testAForgedDescriptorNeverReachesTheUser() {
        let store = makeStore(verification: .rejected, consent: blanketConsent)
        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor()), .refused(.signatureInvalid))
        XCTAssertTrue(store.pendingInvitations.isEmpty)
    }

    /// §5.3 locks k≥10 and ≥1-week date rounding, and the device is the only place they can be
    /// checked. A study below the floor is not one the user may be *asked* about.
    func testAnonymisationBelowTheLockedFloorIsRefused() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: blanketConsent)
        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor(k: 9)),
                       .refused(.anonymisationBelowFloor))
        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor(rounding: 6)),
                       .refused(.anonymisationBelowFloor))
        XCTAssertTrue(store.pendingInvitations.isEmpty)
    }

    /// The signature is checked before anything the descriptor *claims*, so a forged descriptor
    /// asking for impossible anonymisation is refused as forged, not as out-of-range.
    func testSignatureIsCheckedBeforeTheDescriptorsOwnClaims() {
        let store = makeStore(verification: .rejected, consent: blanketConsent)
        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor(k: 1)),
                       .refused(.signatureInvalid))
    }

    func testACategoryTheUserDidNotOptIntoIsRefused() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: categoryConsent)
        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor(categories: [.sleep])),
                       .refused(.categoryNotConsented))
    }

    /// §6.2.1: a contact method is the shared precondition for all three delivery paths, so L1
    /// gates the engagement notification too — not only the invitation that asks a question.
    func testL1GatesBothPostures() {
        var noContact = blanketConsent
        noContact.contactConsentGranted = false
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: noContact)
        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor()), .refused(.noContactConsent))
    }

    func testADeviceWithNoResearchConsentAtAllIsRefused() {
        let store = makeStore(verification: .verified(descriptorHash: hash),
                              consent: ResearchConsentState())
        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor()), .refused(.noResearchConsent))
    }

    // MARK: - The two postures (§6.2 L2 vs L3)

    /// L2 consent means the user is *asked*: they are not in the study until they answer.
    func testACategoryConsentUserIsAskedAndIsNotInTheStudyUntilTheyAnswer() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: categoryConsent)
        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor()),
                       .admitted(posture: .consentRequest, descriptorHash: hash))

        XCTAssertEqual(store.pendingInvitations[0].posture, .consentRequest)
        XCTAssertFalse(store.pendingInvitations[0].participatesWithoutAnswer)
        XCTAssertTrue(store.studyParticipations.isEmpty,
                      "An unanswered consent request must not enrol the user.")

        store.acceptInvitation(studyID: studyID)
        XCTAssertEqual(store.studyParticipations.count, 1)
    }

    /// L3 blanket consent means the user is *told*: §6.2 locks that they "still receive per-study
    /// engagement notifications, not consent requests", so they are in the study on arrival.
    func testABlanketConsentUserIsToldAndIsInTheStudyOnArrival() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: blanketConsent)
        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor(categories: [.sleep])),
                       .admitted(posture: .engagementNotification, descriptorHash: hash))

        XCTAssertEqual(store.pendingInvitations[0].posture, .engagementNotification)
        XCTAssertTrue(store.pendingInvitations[0].participatesWithoutAnswer)
        XCTAssertEqual(store.studyParticipations.count, 1,
                       "L3 pre-approved the study, so the audit trail must say the user is in it.")
        XCTAssertTrue(store.studyParticipations[0].isActive)
    }

    /// The L3 opt-out. "Decline" from an engagement notification is a withdrawal, because the user
    /// is already enrolled — the one place the two postures must not share behaviour.
    func testLeavingAnEngagementNotificationWithdrawsTheParticipation() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: blanketConsent)
        store.ingestStudyDescriptor(makeDescriptor())

        store.declineInvitation(studyID: studyID)

        XCTAssertFalse(store.studyParticipations[0].isActive)
        XCTAssertFalse(store.pendingInvitations[0].isOpen)
    }

    /// An engagement notification was never a question, so it cannot be answered "yes" — that
    /// would file a second participation for a study the user is already in.
    func testAnEngagementNotificationCannotBeAccepted() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: blanketConsent)
        store.ingestStudyDescriptor(makeDescriptor())

        store.acceptInvitation(studyID: studyID)

        XCTAssertEqual(store.studyParticipations.count, 1)
        XCTAssertNil(store.pendingInvitations[0].decision)
    }

    // MARK: - §6.3 step 4's third response

    func testAskingAQuestionAboutAStudyIsNotDeciding() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: categoryConsent)
        store.ingestStudyDescriptor(makeDescriptor())

        store.askQuestionAboutInvitation(studyID: studyID)

        XCTAssertNotNil(store.pendingInvitations[0].questionSentAt)
        XCTAssertTrue(store.pendingInvitations[0].isOpen,
                      "Asking is not answering: the invitation must stay in the inbox.")
        XCTAssertTrue(store.studyParticipations.isEmpty)

        // And it can still be answered afterwards.
        store.acceptInvitation(studyID: studyID)
        XCTAssertEqual(store.studyParticipations.count, 1)
    }

    // MARK: - Persistence and re-ingestion

    /// Invitations were in-memory only. An accepted study's participation record outlived the
    /// invitation that recorded the acceptance, so the trail and the inbox disagreed after every
    /// restart — and the same descriptor could be ingested and accepted a second time.
    func testInvitationsAndTheirDecisionsSurviveAReload() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: categoryConsent)
        store.ingestStudyDescriptor(makeDescriptor())
        store.askQuestionAboutInvitation(studyID: studyID)

        let reloaded = ConsentStore()
        XCTAssertEqual(reloaded.pendingInvitations.count, 1)
        XCTAssertNotNil(reloaded.pendingInvitations[0].questionSentAt)
        XCTAssertEqual(reloaded.pendingInvitations[0].posture, .consentRequest)
    }

    /// §5.3 and §6.3 step 6: withdrawal blocks future descriptor processing. Re-presenting a study
    /// the user already answered would be the device forgetting the answer.
    func testAStudyAlreadyAnsweredIsNotPresentedAgain() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: categoryConsent)
        store.ingestStudyDescriptor(makeDescriptor())
        store.declineInvitation(studyID: studyID)

        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor()),
                       .refused(.studyAlreadyDecided))
        XCTAssertEqual(store.pendingInvitations.count, 1)
    }

    func testAWithdrawnStudyIsNotPresentedAgain() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: categoryConsent)
        store.ingestStudyDescriptor(makeDescriptor())
        store.acceptInvitation(studyID: studyID)
        store.withdrawFromStudy(studyID: studyID)

        XCTAssertEqual(store.ingestStudyDescriptor(makeDescriptor()),
                       .refused(.studyAlreadyDecided))
        XCTAssertEqual(store.studyParticipations.count, 1)
    }

    // MARK: - What the consent surface says

    /// §6.3 step 3 requires the invitation to be explicit about what researchers CANNOT see.
    /// It is the complement of the approved set, computed here — not prose supplied by the party
    /// asking for access.
    func testTheCannotSeeListIsDerivedFromTheApprovedSet() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: categoryConsent)
        store.ingestStudyDescriptor(makeDescriptor(elements: [.sessionTimestamps]))

        let invitation = store.pendingInvitations[0]
        XCTAssertEqual(invitation.approvedElements, [.sessionTimestamps])
        XCTAssertEqual(invitation.cannotLearn,
                       Set(UHDRElement.allCases).subtracting([.sessionTimestamps]))
        XCTAssertFalse(invitation.cannotLearn.contains(.sessionTimestamps))
    }

    /// The §5.3 audit trail names a descriptor. It used to record the literal string "pending",
    /// because there was no descriptor to hash.
    func testTheParticipationRecordCarriesTheDescriptorHash() {
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: categoryConsent)
        store.ingestStudyDescriptor(makeDescriptor())
        store.acceptInvitation(studyID: studyID)

        XCTAssertEqual(store.studyParticipations[0].descriptorHash, hash)
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
        let store = makeStore(verification: .verified(descriptorHash: hash), consent: categoryConsent)
        store.ingestStudyDescriptor(makeDescriptor())
        store.acceptInvitation(studyID: studyID)
        XCTAssertFalse(store.studyParticipations.isEmpty)

        store.withdrawFromStudy(studyID: studyID)

        let record = store.studyParticipations.first { $0.studyID == studyID }
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

    // MARK: - L2 and L3 are orthogonal axes (CLAUDE.md §6.2.2 / §6.2.3)
    //
    // These assert the *behaviour* the merged S2 screen must preserve, not the number of
    // screens it takes to express it. A screen-count assertion would pass for a design that
    // fused the axes; these do not.

    func testSelectAllCategoriesDoesNotEnableBlanketConsent() {
        var state = ResearchConsentState()
        state.setAllCategories(true)
        store.updateResearchConsent(state)

        XCTAssertTrue(store.researchConsent.allCategoriesSelected)
        XCTAssertFalse(store.researchConsent.blanketConsentGranted,
                       "Selecting all nine categories expresses scope, not posture — it must "
                       + "never imply blanket consent (§6.2.3).")
    }

    func testEverythingButAskMeIsRepresentable() {
        // The position the merge must not destroy: all nine areas, still asked each time.
        var state = ResearchConsentState()
        state.setAllCategories(true)
        state.blanketConsentGranted = false
        store.updateResearchConsent(state)

        XCTAssertTrue(store.researchConsent.allCategoriesSelected)
        XCTAssertFalse(store.researchConsent.blanketConsentGranted)
        XCTAssertTrue(store.researchConsent.hasAnyResearchConsent)
    }

    func testBlanketConsentWithoutAnyCategoryIsRepresentable() {
        // The mirror position: pre-approved, no category preference expressed.
        var state = ResearchConsentState()
        state.blanketConsentGranted = true
        store.updateResearchConsent(state)

        XCTAssertFalse(store.researchConsent.allCategoriesSelected)
        XCTAssertTrue(store.researchConsent.blanketConsentGranted)
    }

    func testSetAllCategoriesFalseClearsEveryCategory() {
        var state = ResearchConsentState()
        state.setAllCategories(true)
        state.setAllCategories(false)
        store.updateResearchConsent(state)

        XCTAssertFalse(store.researchConsent.allCategoriesSelected)
        XCTAssertFalse(store.researchConsent.categoryConsents.values.contains(true))
    }

    // MARK: - Onboarding presents two screens, four layers

    func testOnboardingStepCountIsTwo() {
        // Deliberately paired with the behavioural tests above. On its own a step count says
        // nothing about whether any consent axis survived the merge — that is what the
        // orthogonality tests are for. This one only pins the presentation change.
        XCTAssertEqual(ConsentOnboardingView.ConsentScreen.stepCount, 2)
    }

    func testAllFourConsentLayersRemainIndependentlySettable() {
        // Four layers, two screens. Each layer still has its own field and can be set without
        // touching the others.
        var state = ResearchConsentState()
        state.contactConsentGranted = true            // L1
        state.categoryConsents[.ptsd] = true          // L2
        state.blanketConsentGranted = true            // L3
        state.resultsOptIn = true                     // L4
        state.suggestionPortalOptIn = false           // L4
        store.updateResearchConsent(state)

        let reloaded = ConsentStore()
        XCTAssertTrue(reloaded.researchConsent.contactConsentGranted)
        XCTAssertTrue(reloaded.researchConsent.categoryConsents[.ptsd] == true)
        XCTAssertTrue(reloaded.researchConsent.blanketConsentGranted)
        XCTAssertTrue(reloaded.researchConsent.resultsOptIn)
        XCTAssertFalse(reloaded.researchConsent.suggestionPortalOptIn)
    }

    // MARK: - Clinician access expansion (§6.1, OI-CONSENT-02)
    //
    // The property under test throughout is that the two decisions §6.1 requires to be asked
    // separately are also RECORDED separately. Before this workflow existed,
    // `expandClinicianAccess` set `tier` and nothing else, and because `approvedElements` derives
    // from a timeless tier, that handed the clinician the new elements over every session ever
    // recorded — the retroactive decision taken silently, and taken yes.

    private var yesterday: Date { Date().addingTimeInterval(-86_400) }

    private func makeGrant(_ tier: ClinicianUseCaseTier) -> ClinicianConsentGrant {
        ClinicianConsentGrant(
            id: grantID, clinicianName: "Dr. Test", clinicianOrganization: "Test Hospital",
            tier: tier, grantedAt: yesterday
        )
    }

    private func makeRequest(_ id: UUID, to tier: ClinicianUseCaseTier) -> ClinicianAccessExpansionRequest {
        ClinicianAccessExpansionRequest(
            id: id, grantID: grantID, fromTier: .monitor, toTier: tier,
            requestedAt: Date(), decision: nil
        )
    }

    private var grantID: UUID { UUID(uuidString: "00000000-0000-0000-0000-0000000000A1")! }
    private var requestID: UUID { UUID(uuidString: "00000000-0000-0000-0000-0000000000B1")! }

    func testApprovingForwardOnlyLeavesEarlierSessionsOutOfReach() {
        store.grantClinicianAccess(makeGrant(.monitor))
        store.addExpansionRequest(makeRequest(requestID, to: .assess))

        store.approveExpansion(requestID: requestID, includePriorData: false)

        let grant = store.clinicianGrants[0]
        // The tier moves — it is what the subscription is keyed to — and today's data is in reach.
        XCTAssertEqual(grant.tier, .assess)
        XCTAssertEqual(grant.approvedElements, ClinicianUseCaseTier.assess.uhdrElements)
        XCTAssertEqual(grant.elementsVisible(forDataRecordedAt: Date()),
                       ClinicianUseCaseTier.assess.uhdrElements)

        // ...but yesterday's sessions still show only what Monitor ever reached. This is the
        // assertion the old implementation could not have passed.
        XCTAssertEqual(grant.elementsVisible(forDataRecordedAt: yesterday),
                       ClinicianUseCaseTier.monitor.uhdrElements)
        XCTAssertFalse(grant.elementsVisible(forDataRecordedAt: yesterday).contains(.eegWaveforms))
        XCTAssertEqual(
            grant.forwardOnlyElements,
            ClinicianUseCaseTier.assess.uhdrElements
                .subtracting(ClinicianUseCaseTier.monitor.uhdrElements)
        )
    }

    func testApprovingWithPriorDataCoversEarlierSessions() {
        store.grantClinicianAccess(makeGrant(.monitor))
        store.addExpansionRequest(makeRequest(requestID, to: .assess))

        store.approveExpansion(requestID: requestID, includePriorData: true)

        let grant = store.clinicianGrants[0]
        XCTAssertEqual(grant.elementsVisible(forDataRecordedAt: yesterday),
                       ClinicianUseCaseTier.assess.uhdrElements)
        XCTAssertTrue(grant.forwardOnlyElements.isEmpty)
    }

    func testTheTwoDecisionsAreRecordedSeparately() {
        store.grantClinicianAccess(makeGrant(.monitor))
        store.addExpansionRequest(makeRequest(requestID, to: .assess))

        store.approveExpansion(requestID: requestID, includePriorData: false)

        // "Approved, and not over history" must be readable back off the record as two answers,
        // not inferred from a single flag.
        guard case let .some(.approved(_, includesPriorData)) = store.expansionRequests[0].decision else {
            return XCTFail("Expected an approved decision.")
        }
        XCTAssertFalse(includesPriorData)
        XCTAssertFalse(store.expansionRequests[0].isPending)
    }

    func testDenyingChangesNothingAboutTheGrant() {
        store.grantClinicianAccess(makeGrant(.monitor))
        store.addExpansionRequest(makeRequest(requestID, to: .assess))

        store.denyExpansion(requestID: requestID)

        XCTAssertEqual(store.clinicianGrants[0].tier, .monitor)
        XCTAssertEqual(store.clinicianGrants[0].approvedElements,
                       ClinicianUseCaseTier.monitor.uhdrElements)
        XCTAssertFalse(store.expansionRequests[0].isPending)
    }

    func testAskingAQuestionIsNotDeciding() {
        store.grantClinicianAccess(makeGrant(.monitor))
        store.addExpansionRequest(makeRequest(requestID, to: .assess))

        store.askQuestionAboutExpansion(requestID: requestID)

        // The notification must survive the question, or the user loses the request by asking
        // about it — and nothing may change about the grant in the meantime.
        XCTAssertTrue(store.expansionRequests[0].isPending)
        XCTAssertNotNil(store.expansionRequests[0].questionSentAt)
        XCTAssertEqual(store.clinicianGrants[0].tier, .monitor)

        // A question does not spend the request: it can still be approved afterwards.
        store.approveExpansion(requestID: requestID, includePriorData: false)
        XCTAssertEqual(store.clinicianGrants[0].tier, .assess)
    }

    func testRevokingAGrantWithdrawsItsPendingRequest() {
        store.grantClinicianAccess(makeGrant(.monitor))
        store.addExpansionRequest(makeRequest(requestID, to: .assess))

        store.revokeClinicianAccess(grantID: grantID)

        XCTAssertTrue(store.expansionRequests.isEmpty)
        // And approving it afterwards must not resurrect anything.
        store.approveExpansion(requestID: requestID, includePriorData: true)
        XCTAssertTrue(store.clinicianGrants.isEmpty)
    }

    func testApprovingAStaleRequestIsRefused() {
        let secondID = UUID(uuidString: "00000000-0000-0000-0000-0000000000B2")!
        store.grantClinicianAccess(makeGrant(.monitor))
        store.addExpansionRequest(makeRequest(requestID, to: .assess))
        // The grant is widened past the request's target by a second, later request.
        store.addExpansionRequest(makeRequest(secondID, to: .fullClinical))
        store.approveExpansion(requestID: secondID, includePriorData: false)

        // The first was superseded when the second arrived, so there is nothing to approve; and
        // even if it were reachable, Assess no longer adds anything to a Full Clinical grant.
        store.approveExpansion(requestID: requestID, includePriorData: true)
        XCTAssertEqual(store.clinicianGrants[0].tier, .fullClinical)
        XCTAssertEqual(
            store.clinicianGrants[0].forwardOnlyElements,
            ClinicianUseCaseTier.fullClinical.uhdrElements
                .subtracting(ClinicianUseCaseTier.monitor.uhdrElements)
        )
    }

    func testANewRequestSupersedesTheOutstandingOneButNotDecidedHistory() {
        let secondID = UUID(uuidString: "00000000-0000-0000-0000-0000000000B2")!
        store.grantClinicianAccess(makeGrant(.monitor))
        store.addExpansionRequest(makeRequest(requestID, to: .assess))
        store.denyExpansion(requestID: requestID)

        store.addExpansionRequest(makeRequest(secondID, to: .assess))

        // The denial stays on the record; only the outstanding request is superseded.
        XCTAssertEqual(store.expansionRequests.count, 2)
        XCTAssertEqual(store.expansionRequests.filter(\.isPending).count, 1)
    }

    func testExpansionRequestsAndScopesSurviveAReload() {
        store.grantClinicianAccess(makeGrant(.monitor))
        store.addExpansionRequest(makeRequest(requestID, to: .assess))
        store.approveExpansion(requestID: requestID, includePriorData: false)

        // §6.1 asks for a *persistent* notification, and a retroactive answer that only lives in
        // memory is not an answer.
        let reloaded = ConsentStore()
        guard case let .some(.approved(_, includesPriorData)) = reloaded.expansionRequests[0].decision else {
            return XCTFail("Expected the approval to survive a reload.")
        }
        XCTAssertFalse(includesPriorData)
        XCTAssertEqual(reloaded.clinicianGrants[0].elementsVisible(forDataRecordedAt: yesterday),
                       ClinicianUseCaseTier.monitor.uhdrElements)
    }

    func testAGrantWithNoScopesKeepsItsPreWorkflowMeaning() {
        // A grant written before the workflow existed has no scopes. Introducing the workflow must
        // not silently re-scope it: a bare tier has always meant "these elements, all history".
        let grant = makeGrant(.assess)
        XCTAssertNil(grant.accessScopes)
        XCTAssertEqual(grant.approvedElements, ClinicianUseCaseTier.assess.uhdrElements)
        XCTAssertEqual(grant.elementsVisible(forDataRecordedAt: Date(timeIntervalSince1970: 0)),
                       ClinicianUseCaseTier.assess.uhdrElements)
        XCTAssertTrue(grant.forwardOnlyElements.isEmpty)
    }

    func testAChangeThatIsNotAnExpansionHasNoDifferential() {
        let monitor = makeGrant(.monitor)
        // Nothing new.
        XCTAssertNil(ConsentEngine.accessDifferential(for: monitor, expandingTo: .monitor))
        // Something lost — a re-scope, not a widening.
        XCTAssertNil(ConsentEngine.accessDifferential(for: makeGrant(.fullClinical), expandingTo: .assess))
        // Either side Research: its element set is IRB-defined per study descriptor, not derivable
        // from the tier, so the emptiness must never be read as a set.
        XCTAssertNil(ConsentEngine.accessDifferential(for: monitor, expandingTo: .research))
        XCTAssertNil(ConsentEngine.accessDifferential(for: makeGrant(.research), expandingTo: .fullClinical))
    }

    func testTheDifferentialSplitsElementsIntoNewAlreadyVisibleAndWithheld() {
        guard let d = ConsentEngine.accessDifferential(for: makeGrant(.monitor), expandingTo: .assess) else {
            return XCTFail("Monitor → Assess is an expansion.")
        }
        XCTAssertEqual(d.newlyVisibleElements,
                       ClinicianUseCaseTier.assess.uhdrElements
                           .subtracting(ClinicianUseCaseTier.monitor.uhdrElements))
        XCTAssertEqual(d.alreadyVisibleElements, ClinicianUseCaseTier.monitor.uhdrElements)
        XCTAssertEqual(d.stillNotAccessibleElements,
                       Set(UHDRElement.allCases).subtracting(ClinicianUseCaseTier.assess.uhdrElements))

        // The three sets partition the element roster: nothing is in two of them, nothing is lost.
        let union = d.newlyVisibleElements.union(d.alreadyVisibleElements)
            .union(d.stillNotAccessibleElements)
        XCTAssertEqual(union, Set(UHDRElement.allCases))
        XCTAssertEqual(
            d.newlyVisibleElements.count + d.alreadyVisibleElements.count
                + d.stillNotAccessibleElements.count,
            UHDRElement.allCases.count
        )
    }

}
