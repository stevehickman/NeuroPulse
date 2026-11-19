//  ConsentRepairTests.swift
//
//  `OI-PRIV-06`: consent state that kept or lost things it should not. Each test pins one repair:
//  a withdrawal that reaches the participations it stops (`REQ-CONSENT-19`), clinician access that
//  ends (`REQ-CONSENT-18`), a stored record that cannot be read (`REQ-CONSENT-20`), and a
//  participation record that says only what happened (`REQ-CONSENT-21`).
//
//  The twin of common/core/src/jvmTest/.../consent/ConsentRepairTests.kt. Where Kotlin compares
//  "yyyy-MM-dd" day strings, these compare calendar days of a `Date`.
//
//  Subject under test: ConsentStore, ConsentEngine, ClinicianConsentGrant, StudyParticipationRecord

import XCTest
@testable import NeurOne

/// A verifier that accepts every descriptor, so the only thing between a study and the person is
/// the consent state under test.
private struct AcceptingVerifier: StudyDescriptorVerifier {
    func verify(_ descriptor: StudyDescriptor) -> StudyDescriptorVerification {
        .verified(descriptorHash: "sha256:abc")
    }
}

/// Stands in for the clinician-portal transport that does not exist (`OI-CONSENT-05`).
private struct VerifiedPortalChannel: ClinicianPortalChannel {
    func verify(_ request: ClinicianAccessExpansionRequest) -> ClinicianRequestVerification { .verified }
}

@MainActor
final class ConsentRepairTests: XCTestCase {

    private let grantsKey = "np.consent.clinician-grants"
    private let researchKey = "np.consent.research"
    private let quarantineKey = "np.consent.quarantine"

    override func setUp() {
        super.setUp()
        clearConsentDefaults()
    }

    override func tearDown() {
        clearConsentDefaults()
        super.tearDown()
    }

    private func clearConsentDefaults() {
        let keys = ["np.consent.clinician-grants", "np.consent.research", "np.consent.study-participations",
                    "np.consent.clinician-expansions", "np.consent.study-invitations", "np.consent.study-withdrawals",
                    "np.consent.quarantine"]
        keys.forEach { ProtectedDefaults.standard.removeObject(forKey: $0) }
    }

    /// A store over whatever is persisted now. Calling it again is the app starting again.
    private func open() -> ConsentStore {
        ConsentStore(descriptorVerifier: AcceptingVerifier())
    }

    private func storeWith(_ consent: ResearchConsentState) -> ConsentStore {
        let store = open()
        store.updateResearchConsent(consent)
        return store
    }

    private func categories(_ granted: ResearchCategory...) -> ResearchConsentState {
        var state = ResearchConsentState()
        state.contactConsentGranted = true
        granted.forEach { state.categoryConsents[$0] = true }
        return state
    }

    private var blanket: ResearchConsentState {
        var state = ResearchConsentState()
        state.contactConsentGranted = true
        state.blanketConsentGranted = true
        return state
    }

    private func study(_ id: String, _ inCategories: ResearchCategory...) -> StudyDescriptor {
        StudyDescriptor(
            studyID: id,
            studyTitle: "Study \(id)",
            researchCategories: inCategories,
            requestedElements: [.sessionDuration],
            kAnonymity: 10,
            dateRoundingDays: 7,
            issuedAt: Date(),
            signature: "sig"
        )
    }

    private func isAdmitted(_ admission: StudyDescriptorAdmission) -> Bool {
        if case .admitted = admission { return true }
        return false
    }

    private func join(_ store: ConsentStore, _ descriptor: StudyDescriptor) {
        XCTAssertTrue(isAdmitted(store.ingestStudyDescriptor(descriptor)))
        store.acceptInvitation(studyID: descriptor.studyID)
    }

    private func active(_ store: ConsentStore) -> Set<String> {
        Set(store.studyParticipations.filter(\.isActive).map(\.studyID))
    }

    /// Noon on a calendar day of this device, or another hour of it.
    private func day(_ year: Int, _ month: Int, _ day: Int, hour: Int = 12) -> Date {
        Calendar.current.date(from: DateComponents(year: year, month: month, day: day, hour: hour))!
    }

    // MARK: - REQ-CONSENT-19: a withdrawal stops the participations it no longer covers

    // @verifies REQ-CONSENT-19#a
    func testBlanketWithdrawalStopsEveryParticipation() {
        let store = storeWith(blanket)
        store.ingestStudyDescriptor(study("A", .depression))
        store.ingestStudyDescriptor(study("B", .sleep))
        XCTAssertEqual(active(store), ["A", "B"], "an L3 user is in a study from the moment it arrives")

        store.withdrawBlanketResearchConsent()

        XCTAssertTrue(active(store).isEmpty)
        XCTAssertTrue(store.studyParticipations.allSatisfy { record in
            record.withdrawnAt.map { Calendar.current.isDateInToday($0) } ?? false
        })
    }

    // @verifies REQ-CONSENT-19#a
    func testBlanketWithdrawalThroughTheCommitPathStopsEveryParticipation() {
        var consent = blanket
        consent.setAllCategories(true)
        let store = storeWith(consent)
        store.ingestStudyDescriptor(study("A", .depression))

        // The nine ticks are still on. They do not keep the study going: withdrawal outranks them (§6.0).
        var withdrawn = store.researchConsent
        withdrawn.blanketConsentGranted = false
        store.updateResearchConsent(withdrawn)

        XCTAssertTrue(active(store).isEmpty)
    }

    // @verifies REQ-CONSENT-19#b
    func testCategoryWithdrawalStopsOnlyTheStudiesLeftWithNoConsentedCategory() {
        let store = storeWith(categories(.depression, .sleep))
        join(store, study("A", .depression))
        join(store, study("B", .sleep))
        join(store, study("C", .depression, .sleep))

        store.updateResearchConsent(categories(.sleep))
        XCTAssertEqual(active(store), ["B", "C"], "C still has a consented category")

        store.setCategoryConsent(.sleep, granted: false)
        XCTAssertTrue(active(store).isEmpty)
    }

    // @verifies REQ-CONSENT-19#b
    func testACategoryWithdrawalUnderBlanketConsentStopsNothing() {
        var consent = blanket
        consent.setAllCategories(true)
        let store = storeWith(consent)
        store.ingestStudyDescriptor(study("A", .depression))

        store.setCategoryConsent(.depression, granted: false)

        XCTAssertEqual(active(store), ["A"], "blanket consent is what covers it")
    }

    // @verifies REQ-CONSENT-19#c
    func testAStoppedParticipationIsNotRecordedAsAStudyThePersonNamed() {
        let store = storeWith(blanket)
        store.ingestStudyDescriptor(study("A", .depression))

        store.withdrawBlanketResearchConsent()

        XCTAssertTrue(store.studyWithdrawals.isEmpty, "the person said stop to everything, not to study A by name")
        XCTAssertEqual(store.pendingInvitations.count, 1)
        XCTAssertEqual(store.pendingInvitations.first?.isOpen, false,
                       "the notice that they are in the study is closed with it")
    }

    // @verifies REQ-CONSENT-19#c
    func testARestoredBlanketWithdrawalStopsParticipationsHereToo() throws {
        let store = storeWith(blanket)
        store.ingestStudyDescriptor(study("A", .depression))

        try store.applyWithdrawals(["blanketConsentWithdrawnAt": "2099-01-01T00:00:00Z"])

        XCTAssertTrue(active(store).isEmpty)
    }

    // @verifies REQ-CONSENT-19#c
    func testAParticipationLeftActiveByAnEarlierWithdrawalIsStoppedWhenTheStoreIsRead() throws {
        let store = storeWith(blanket)
        store.ingestStudyDescriptor(study("A", .depression))
        // What the store wrote before the repair: consent withdrawn, participation untouched.
        var withdrawn = ResearchConsentState()
        withdrawn.contactConsentGranted = true
        withdrawn.blanketConsentWithdrawnAt = day(2026, 1, 1)
        ProtectedDefaults.standard.set(try JSONEncoder().encode(withdrawn), forKey: researchKey)

        let reopened = open()

        XCTAssertEqual(reopened.studyParticipations.count, 1)
        XCTAssertTrue(active(reopened).isEmpty)
        XCTAssertTrue(active(open()).isEmpty, "and the repair is stored")
    }

    // @verifies REQ-CONSENT-19#d
    func testARequestCannotBeAcceptedAfterItsConsentWasWithdrawn() {
        let store = storeWith(categories(.depression))
        store.ingestStudyDescriptor(study("A", .depression))

        store.updateResearchConsent(categories())
        store.acceptInvitation(studyID: "A")
        XCTAssertTrue(store.studyParticipations.isEmpty, "accepting would start a flow the withdrawal stopped")
        XCTAssertEqual(store.pendingInvitations.first?.isOpen, true, "and the request is not answered for the person")

        // Consenting again is a fresh decision, and the request can then be answered.
        store.updateResearchConsent(categories(.depression))
        store.acceptInvitation(studyID: "A")
        XCTAssertEqual(active(store), ["A"])
    }

    // @verifies REQ-CONSENT-19#d
    func testARequestCannotBeAcceptedWhileBlanketConsentIsWithdrawn() {
        let store = storeWith(categories(.depression))
        store.ingestStudyDescriptor(study("A", .depression))
        var granted = store.researchConsent
        granted.blanketConsentGranted = true
        store.updateResearchConsent(granted)
        store.withdrawBlanketResearchConsent()

        store.acceptInvitation(studyID: "A")

        XCTAssertTrue(store.studyParticipations.isEmpty, "the category is still ticked, and withdrawal outranks it")
    }

    // MARK: - REQ-CONSENT-18: clinician access is time-limited

    private var grantID: UUID { UUID(uuidString: "00000000-0000-0000-0000-0000000000A1")! }

    private func grant(expiresAt: Date?, tier: ClinicianUseCaseTier = .monitor) -> ClinicianConsentGrant {
        ClinicianConsentGrant(
            id: grantID, clinicianName: "Dr. Test", clinicianOrganization: "Test Hospital",
            tier: tier, grantedAt: day(2026, 1, 1), expiresAt: expiresAt
        )
    }

    /// A store that finds `stored` already on the device. Written under the store's key, because the public
    /// API now refuses a grant that is not in force.
    private func storeHolding(_ stored: ClinicianConsentGrant) throws -> ConsentStore {
        ProtectedDefaults.standard.set(try JSONEncoder().encode([stored]), forKey: grantsKey)
        return open()
    }

    // Kotlin also refuses an expiry that is not a real "yyyy-MM-dd" day. A `Date` cannot be malformed, so
    // those two cases have no twin here.
    // @verifies REQ-CONSENT-18#a
    func testAGrantWithNoUsableExpiryIsRefused() {
        let store = open()
        let yesterday = Calendar.current.date(byAdding: .day, value: -1, to: Date())!

        XCTAssertFalse(store.grantClinicianAccess(grant(expiresAt: nil)), "no expiry is not time-limited")
        XCTAssertFalse(store.grantClinicianAccess(grant(expiresAt: yesterday)), "already over")
        XCTAssertTrue(store.clinicianGrants.isEmpty)

        XCTAssertTrue(store.grantClinicianAccess(grant(expiresAt: Calendar.current.startOfDay(for: Date()))),
                      "the expiry day is the last day in force")
        XCTAssertEqual(store.clinicianGrants.count, 1)
    }

    // @verifies REQ-CONSENT-18#b
    func testAGrantIsInForceThroughItsExpiryDayAndNotAfter() {
        let g = grant(expiresAt: day(2026, 12, 1, hour: 9))

        XCTAssertTrue(g.isInForce(on: day(2026, 11, 30)))
        XCTAssertTrue(g.isInForce(on: day(2026, 12, 1, hour: 23)), "the whole expiry day, not up to its instant")
        XCTAssertFalse(g.isInForce(on: day(2026, 12, 2, hour: 0)))
        var inactive = g
        inactive.isActive = false
        XCTAssertFalse(inactive.isInForce(on: day(2026, 11, 30)))
    }

    // @verifies REQ-CONSENT-18#c
    func testALapsedGrantShowsTheClinicianNothing() {
        let g = grant(expiresAt: day(2026, 12, 1))

        XCTAssertEqual(g.elementsVisible(forDataRecordedAt: day(2026, 6, 1), on: day(2026, 12, 1)), g.approvedElements)
        XCTAssertTrue(g.elementsVisible(forDataRecordedAt: day(2026, 6, 1), on: day(2026, 12, 2)).isEmpty)
        XCTAssertFalse(g.approvedElements.isEmpty, "the record of what was granted stays readable")
    }

    // @verifies REQ-CONSENT-18#c
    func testALapsedGrantCannotBeWidened() throws {
        let lapsed = grant(expiresAt: day(2020, 1, 1))
        XCTAssertNil(ConsentEngine.accessDifferential(for: lapsed, expandingTo: .assess, on: Date()))

        let store = try storeHolding(lapsed)
        let request = ClinicianAccessExpansionRequest(
            id: UUID(uuidString: "00000000-0000-0000-0000-0000000000B1")!, grantID: lapsed.id,
            fromTier: .monitor, toTier: .assess, requestedAt: Date(), decision: nil
        )
        XCTAssertEqual(
            ClinicianPortalSync(store: store, channel: VerifiedPortalChannel()).ingest(request),
            .refusedNotAnExpansion
        )

        // A request that was already waiting when the grant lapsed cannot be approved either.
        store.addExpansionRequest(request)
        store.approveExpansion(requestID: request.id, includePriorData: true)
        XCTAssertEqual(store.clinicianGrants.first?.tier, .monitor)
        XCTAssertEqual(store.expansionRequests.count, 1)
        XCTAssertEqual(store.expansionRequests.first?.isPending, true)
    }

    // @verifies REQ-CONSENT-18#d
    func testAStoredGrantWithNoExpiryIsNotInForce() throws {
        let store = try storeHolding(grant(expiresAt: nil))
        let stored = try XCTUnwrap(store.clinicianGrants.first)

        XCTAssertFalse(stored.isInForce(on: Date()))
        XCTAssertTrue(stored.elementsVisible(forDataRecordedAt: Date(), on: Date()).isEmpty)
    }

    // MARK: - REQ-CONSENT-20: a stored record that cannot be read

    private let unreadable = Data(#"{"blanketConsentGranted":"perhaps"}"#.utf8)

    /// Persisted state whose research record was good, held a withdrawal, and then stopped decoding.
    private func persistUnreadableResearchRecord() {
        let store = storeWith(blanket)
        store.withdrawBlanketResearchConsent()
        ProtectedDefaults.standard.set(unreadable, forKey: researchKey)
    }

    // @verifies REQ-CONSENT-20#a
    func testAnUnreadableRecordIsKeptAsFoundAndSurvivesLaterSaves() {
        persistUnreadableResearchRecord()

        let store = open()
        XCTAssertEqual(store.quarantinedBlobs.map(\.blob), [unreadable])
        XCTAssertEqual(store.quarantinedBlobs.map(\.key), [researchKey])

        // Any save now writes defaults over the record itself. The bytes that were there are still held.
        XCTAssertTrue(store.grantClinicianAccess(grant(expiresAt: Date())))
        XCTAssertNotEqual(ProtectedDefaults.standard.data(forKey: researchKey), unreadable)
        XCTAssertEqual(open().quarantinedBlobs.map(\.blob), [unreadable])
    }

    // @verifies REQ-CONSENT-20#b
    func testNothingIsAdmittedWhileTheRecordIsUnreadable() {
        persistUnreadableResearchRecord()
        let store = open()

        XCTAssertTrue(store.consentRecordUnreadable)
        XCTAssertEqual(store.ingestStudyDescriptor(study("A", .depression)), .refused(.consentRecordUnreadable))
        // It still holds after a save has replaced the unreadable bytes with defaults, and after a restart.
        store.grantClinicianAccess(grant(expiresAt: Date()))
        XCTAssertTrue(open().consentRecordUnreadable)
    }

    // @verifies REQ-CONSENT-20#b
    func testARequestCannotBeAcceptedAndNoParticipationIsStoppedWhileTheRecordIsUnreadable() {
        let first = storeWith(categories(.depression, .sleep))
        join(first, study("A", .depression))
        first.ingestStudyDescriptor(study("B", .sleep))
        ProtectedDefaults.standard.set(unreadable, forKey: researchKey)

        let store = open()
        store.acceptInvitation(studyID: "B")

        XCTAssertEqual(active(store), ["A"], "defaults are not the person's decision, so nothing is stopped on them")
        XCTAssertEqual(store.studyParticipations.count, 1, "and nothing is started")
    }

    // @verifies REQ-CONSENT-20#c
    func testADecisionByThePersonEndsTheQuarantineAndKeepsTheBytes() {
        persistUnreadableResearchRecord()
        let store = open()

        store.updateResearchConsent(categories(.depression))

        XCTAssertFalse(store.consentRecordUnreadable)
        XCTAssertTrue(isAdmitted(store.ingestStudyDescriptor(study("A", .depression))))
        let reopened = open()
        XCTAssertFalse(reopened.consentRecordUnreadable)
        XCTAssertEqual(reopened.quarantinedBlobs.map(\.blob), [unreadable])
        XCTAssertEqual(reopened.quarantinedBlobs.first?.redecidedAt.map { Calendar.current.isDateInToday($0) }, true)
    }

    // @verifies REQ-CONSENT-20#c
    func testSayingStopWhileTheRecordIsUnreadableIsRecordedAsAWithdrawal() {
        persistUnreadableResearchRecord()
        let store = open()
        XCTAssertFalse(store.researchConsent.blanketConsentWithdrawn, "the defaults read as never asked")

        store.withdrawBlanketResearchConsent()

        XCTAssertTrue(store.researchConsent.blanketConsentWithdrawn)
        XCTAssertNotNil(store.researchConsent.blanketConsentWithdrawnAt)
        XCTAssertFalse(store.consentRecordUnreadable)
    }

    // @verifies REQ-CONSENT-20#d
    func testAnUnreadableQuarantineListIsItselfKeptAndBlocksAdmission() {
        let notAList = Data("not a list".utf8)
        ProtectedDefaults.standard.set(notAList, forKey: quarantineKey)

        let store = open()

        XCTAssertTrue(store.consentRecordUnreadable, "what it held may have been an unanswered quarantine")
        XCTAssertEqual(store.quarantinedBlobs.map(\.blob), [notAList])
        XCTAssertEqual(open().quarantinedBlobs.map(\.blob), [notAList])
    }

    // @verifies REQ-CONSENT-20#d
    func testAnUnreadableClinicianRecordIsKeptAndDoesNotBlockResearch() {
        let truncated = Data("[{".utf8)
        ProtectedDefaults.standard.set(truncated, forKey: grantsKey)

        let store = open()

        XCTAssertTrue(store.clinicianGrants.isEmpty, "which gives a clinician nothing")
        XCTAssertEqual(store.quarantinedBlobs.map(\.blob), [truncated])
        XCTAssertFalse(store.consentRecordUnreadable)
    }

    // MARK: - REQ-CONSENT-21: the participation record says only what happened

    // @verifies REQ-CONSENT-21#a
    func testEnrolmentRecordsWhenJoinedAndNoTransmission() throws {
        let store = storeWith(categories(.depression))
        join(store, study("A", .depression))

        XCTAssertEqual(store.studyParticipations.count, 1)
        let record = try XCTUnwrap(store.studyParticipations.first)
        let enrolledAt = try XCTUnwrap(record.enrolledAt)
        XCTAssertTrue(Calendar.current.isDateInToday(enrolledAt))
        XCTAssertNil(record.transmittedAt, "nothing has been sent")
        XCTAssertEqual(record.extractBytes, 0)
    }

    // @verifies REQ-CONSENT-21#b
    func testARecordStoredBeforeTheRepairIsReadAsEnrolledWithNothingSent() throws {
        ProtectedDefaults.standard.set(try JSONEncoder().encode(blanket), forKey: researchKey)
        // The record as the store wrote it before the repair. A `Date` is stored as seconds since the
        // reference date, which is what `JSONEncoder` writes by default.
        let stored = #"[{"id":"00000000-0000-0000-0000-0000000000C1","studyID":"A","descriptorHash":"sha256:abc","#
            + #""transmittedAt":790000000,"extractBytes":0}]"#
        ProtectedDefaults.standard.set(Data(stored.utf8), forKey: PersistedKeys.PARTICIPATION_KEY)

        let store = open()

        XCTAssertTrue(store.quarantinedBlobs.isEmpty, "a record without the new keys still decodes")
        XCTAssertEqual(store.studyParticipations.count, 1)
        let record = try XCTUnwrap(store.studyParticipations.first)
        XCTAssertEqual(record.enrolledAt, Date(timeIntervalSinceReferenceDate: 790_000_000))
        XCTAssertNil(record.transmittedAt)
        XCTAssertTrue(record.isActive)
        let rewritten = try XCTUnwrap(ProtectedDefaults.standard.data(forKey: PersistedKeys.PARTICIPATION_KEY))
        XCTAssertTrue(String(decoding: rewritten, as: UTF8.self).contains("enrolledAt"), "and it is stored that way")
    }

    // MARK: - REQ-CONSENT-09 h: what a study may ask for

    // @verifies REQ-CONSENT-09#h
    func testAStudyAskingForARecordedSignalIsRefusedWhateverKItDeclares() {
        let store = storeWith(blanket)
        // EEG, PPG and HRV alike: the three recorded signals are handled the same way.
        let signals: [UHDRElement] = [.eegWaveforms, .ppgOpticalSignal, .hrvTimeSeries]
        for signal in signals {
            var asksForSignal = study("A-\(signal.rawValue)", .depression)
            asksForSignal.requestedElements = [.sessionDuration, signal]
            asksForSignal.kAnonymity = 1000
            XCTAssertEqual(
                store.ingestStudyDescriptor(asksForSignal), .refused(.requestsElementNoExtractMayCarry),
                signal.rawValue
            )
        }
        XCTAssertTrue(store.pendingInvitations.isEmpty)
        XCTAssertEqual(store.ingestStudyDescriptor(study("B")),
                       .admitted(posture: .engagementNotification, descriptorHash: "sha256:abc"))
    }
}
