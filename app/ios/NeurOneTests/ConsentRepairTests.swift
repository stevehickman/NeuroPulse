//  ConsentRepairTests.swift
//
//  `OI-PRIV-06`: consent state that kept or lost things it should not. Each test pins one repair:
//  a withdrawal that reaches the participations it stops (`REQ-CONSENT-19`), clinician access that
//  ends (`REQ-CONSENT-18`), a stored record that cannot be read (`REQ-CONSENT-20`), and a
//  participation record that says only what happened (`REQ-CONSENT-21`).
//
//  Then the rest of that item: a descriptor that expires (`REQ-CONSENT-09` i), a typed contact method
//  (`REQ-CONSENT-23`), a withdrawal marked with a day and a count (`REQ-CONSENT-24`), and the marks
//  recovered from an unreadable record (`REQ-CONSENT-20` e).
//
//  The twin of common/core/src/jvmTest/.../consent/ConsentRepairTests.kt. Where Kotlin compares
//  "yyyy-MM-dd" day strings, these compare calendar days of a `Date`, except for a withdrawal mark,
//  which is the same text on both.
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
    private let invitationsKey = "np.consent.study-invitations"
    private let studyWithdrawalsKey = "np.consent.study-withdrawals"

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
        state.contactMethod = "person@example.org"
        granted.forEach { state.categoryConsents[$0] = true }
        return state
    }

    private var blanket: ResearchConsentState {
        var state = ResearchConsentState()
        state.contactConsentGranted = true
        state.contactMethod = "person@example.org"
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
            expiresAt: Date().addingTimeInterval(30 * 86_400),
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

        try store.applyWithdrawals(["blanketConsentWithdrawnAt": "2099-01-01#000001"])

        XCTAssertTrue(active(store).isEmpty)
    }

    // @verifies REQ-CONSENT-19#c
    func testAParticipationLeftActiveByAnEarlierWithdrawalIsStoppedWhenTheStoreIsRead() throws {
        let store = storeWith(blanket)
        store.ingestStudyDescriptor(study("A", .depression))
        // What the store wrote before the repair: consent withdrawn, participation untouched.
        var withdrawn = ResearchConsentState()
        withdrawn.contactConsentGranted = true
        withdrawn.blanketConsentWithdrawnAt = "2026-01-01#000001"
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

    // MARK: - REQ-CONSENT-09 i: a descriptor stands only until the day its issuer signed

    /// Noon UTC on this device's calendar day, moved by `offset` days: a `Date` whose signed (UTC) day is
    /// that day whatever zone the test runs in. Kotlin writes the day as text; here it has to be built.
    private func signedDay(_ offset: Int = 0) -> Date {
        var device = Calendar(identifier: .gregorian)
        device.timeZone = .current
        let local = device.dateComponents([.year, .month, .day], from: Date())
        var utc = Calendar(identifier: .gregorian)
        utc.timeZone = TimeZone(secondsFromGMT: 0)!
        let noon = utc.date(from: DateComponents(year: local.year, month: local.month, day: local.day, hour: 12))!
        return utc.date(byAdding: .day, value: offset, to: noon)!
    }

    // Kotlin also refuses an expiry that is not a real "yyyy-MM-dd" day. A `Date` cannot be malformed, so
    // that case has no twin here.
    // @verifies REQ-CONSENT-09#i
    func testADescriptorIsRefusedAfterItsExpiryDayOrWithAnExpiryThatCannotBeHeldTo() {
        let store = storeWith(blanket)

        var longOver = study("A")
        longOver.issuedAt = day(2026, 1, 1)
        longOver.expiresAt = signedDay(-1)
        XCTAssertEqual(store.ingestStudyDescriptor(longOver), .refused(.descriptorExpired))

        var endsBeforeItStarts = study("C")
        endsBeforeItStarts.issuedAt = signedDay()
        endsBeforeItStarts.expiresAt = signedDay(-1)
        XCTAssertEqual(store.ingestStudyDescriptor(endsBeforeItStarts), .refused(.descriptorExpired))

        var lastDay = study("D")
        lastDay.issuedAt = signedDay()
        lastDay.expiresAt = signedDay()
        XCTAssertTrue(isAdmitted(store.ingestStudyDescriptor(lastDay)), "the expiry day is the last day")
    }

    // @verifies REQ-CONSENT-09#i
    func testARequestCannotBeAcceptedAfterItsDescriptorExpired() throws {
        let store = storeWith(categories(.depression))
        store.ingestStudyDescriptor(study("A", .depression))
        // The invitation as it will be once its expiry day has passed.
        var stored = store.pendingInvitations
        XCTAssertEqual(stored.count, 1)
        XCTAssertNotNil(stored.first?.expiresAt, "the invitation carries the descriptor's expiry")
        for idx in stored.indices { stored[idx].expiresAt = day(2020, 1, 1) }
        ProtectedDefaults.standard.set(try JSONEncoder().encode(stored), forKey: invitationsKey)

        let reopened = open()
        reopened.acceptInvitation(studyID: "A")

        XCTAssertTrue(reopened.studyParticipations.isEmpty)
        XCTAssertEqual(reopened.pendingInvitations.count, 1)
        XCTAssertEqual(reopened.pendingInvitations.first?.isOpen, true)
    }

    // MARK: - REQ-CONSENT-23: the contact method is typed, needed for L1, and erased with it

    // @verifies REQ-CONSENT-23#a
    func testAContactIsAnEmailAddressOrAPhoneNumberAndNothingElse() {
        XCTAssertEqual(ResearchContact.parse("  a.b@example.org "),
                       ResearchContact(channel: .email, value: "a.b@example.org"))
        XCTAssertEqual(ResearchContact.parse("+1 (555) 123-4567"),
                       ResearchContact(channel: .phone, value: "+15551234567"))
        let notOne = ["", "call me after my EEG results", "a@b", "a@@example.org", "a b@example.org", "@example.org",
                      "+1234567890123456", "555-CALL"]
        for text in notOne {
            XCTAssertNil(ResearchContact.parse(text), text)
        }
    }

    // @verifies REQ-CONSENT-23#b
    func testContactConsentIsNotTakenWithoutAUsableMethod() {
        let store = open()

        var asked = ResearchConsentState()
        asked.contactConsentGranted = true
        asked.contactMethod = "ask my sister"
        store.updateResearchConsent(asked)
        XCTAssertFalse(store.researchConsent.contactConsentGranted)
        XCTAssertEqual(store.researchConsent.contactMethod, "", "and what was typed is not kept")
        XCTAssertNil(store.researchConsent.contactConsentWithdrawnAt, "nothing was granted, so nothing was withdrawn")

        asked.contactMethod = "+44 20 7946 0000"
        store.updateResearchConsent(asked)
        XCTAssertTrue(store.researchConsent.contactConsentGranted)
        XCTAssertEqual(store.researchConsent.contactMethod, "+442079460000")
        XCTAssertEqual(store.researchConsent.contactChannel, .phone)

        // An unusable replacement does not displace the method held, and does not read as a withdrawal.
        var replaced = store.researchConsent
        replaced.contactMethod = "nope"
        store.updateResearchConsent(replaced)
        XCTAssertTrue(store.researchConsent.contactConsentGranted)
        XCTAssertEqual(store.researchConsent.contactMethod, "+442079460000")
        XCTAssertNil(store.researchConsent.contactConsentWithdrawnAt)
    }

    // @verifies REQ-CONSENT-23#c
    func testWithdrawingContactConsentErasesTheMethod() throws {
        let store = storeWith(blanket)
        XCTAssertEqual(store.researchConsent.contactMethod, "person@example.org")

        var withdrawn = store.researchConsent
        withdrawn.contactConsentGranted = false
        store.updateResearchConsent(withdrawn)

        XCTAssertEqual(store.researchConsent.contactMethod, "")
        XCTAssertNil(store.researchConsent.contactChannel)
        let stored = try XCTUnwrap(ProtectedDefaults.standard.data(forKey: researchKey))
        XCTAssertFalse(String(decoding: stored, as: UTF8.self).contains("person@example.org"))
    }

    // @verifies REQ-CONSENT-23#c
    func testARestoredContactWithdrawalErasesTheMethodHereToo() throws {
        let store = storeWith(blanket)

        try store.applyWithdrawals(["contactConsentWithdrawnAt": "2099-01-01#000001"])

        XCTAssertFalse(store.researchConsent.contactConsentGranted)
        XCTAssertEqual(store.researchConsent.contactMethod, "")
    }

    // MARK: - REQ-CONSENT-24: a withdrawal is marked with a day and a count

    /// A store whose withdrawals fall on `days`, in order, and on the device's day once they run out.
    private func onDays(_ days: String...) -> ConsentStore {
        var remaining = days
        return ConsentStore(
            descriptorVerifier: AcceptingVerifier(),
            today: { remaining.isEmpty ? ConsentDay.today() : remaining.removeFirst() }
        )
    }

    private var blanketOff: ResearchConsentState {
        var state = blanket
        state.blanketConsentGranted = false
        return state
    }

    // @verifies REQ-CONSENT-24#a
    func testAMarkHoldsADayAndACountAndNoTimeOfDay() {
        let store = onDays("2026-03-01")
        store.updateResearchConsent(blanket)

        store.updateResearchConsent(blanketOff)

        XCTAssertEqual(store.researchConsent.blanketConsentWithdrawnAt, "2026-03-01#000001")
        XCTAssertTrue(WithdrawalMark.isValid("2026-03-01#000001"))
        XCTAssertFalse(WithdrawalMark.isValid("2026-03-01T10:15:30Z"), "an instant is not a mark")
    }

    // @verifies REQ-CONSENT-24#a
    func testARestoreTakesOnlyWellFormedMarks() throws {
        let store = onDays()
        store.updateResearchConsent(blanket)

        try store.applyWithdrawals([
            "blanketConsentWithdrawnAt": "2099-01-01T00:00:00Z",
            "studyWithdrawals": ["S-1": "yesterday", "S-2": "2026-01-20#000001"],
        ])

        XCTAssertTrue(store.researchConsent.blanketConsentGranted, "an instant is not a mark, so it withdraws nothing")
        XCTAssertEqual(store.studyWithdrawals, ["S-2": "2026-01-20#000001"])
    }

    // @verifies REQ-CONSENT-24#b
    func testTwoWithdrawalsOfOneDecisionOnOneDayAreOrderedByTheirCount() throws {
        let store = onDays("2026-03-01", "2026-03-01", "2026-03-05")
        store.updateResearchConsent(blanket)
        store.updateResearchConsent(blanketOff)
        let first = try XCTUnwrap(store.researchConsent.blanketConsentWithdrawnAt)
        store.updateResearchConsent(blanket)
        store.updateResearchConsent(blanketOff)
        let second = try XCTUnwrap(store.researchConsent.blanketConsentWithdrawnAt)
        store.updateResearchConsent(blanket)
        store.updateResearchConsent(blanketOff)
        let third = try XCTUnwrap(store.researchConsent.blanketConsentWithdrawnAt)

        XCTAssertEqual(second, "2026-03-01#000002")
        XCTAssertEqual(third, "2026-03-05#000003")
        XCTAssertTrue(first < second && second < third, "the text orders them")
    }

    // @verifies REQ-CONSENT-24#c
    func testASecondWithdrawalOnTheOtherPhoneTheSameDayIsLaterAndIsApplied() throws {
        // This phone holds the first withdrawal and consent was granted again here. The other phone
        // withdrew again the same day: its count is higher, so the restore switches the grant off.
        let store = onDays("2026-03-01")
        store.updateResearchConsent(blanket)
        store.updateResearchConsent(blanketOff)
        store.updateResearchConsent(blanket)

        try store.applyWithdrawals(["blanketConsentWithdrawnAt": "2026-03-01#000001"])
        XCTAssertTrue(store.researchConsent.blanketConsentGranted, "the same withdrawal again changes nothing")

        try store.applyWithdrawals(["blanketConsentWithdrawnAt": "2026-03-01#000002"])
        XCTAssertFalse(store.researchConsent.blanketConsentGranted)
        XCTAssertEqual(store.researchConsent.blanketConsentWithdrawnAt, "2026-03-01#000002")
    }

    // MARK: - REQ-CONSENT-20 e: marks are read out of a record that did not decode

    private let unreadableWithdrawalList = Data(#"{"S-1":"2026-01-20#000001","S-2":17}"#.utf8)

    // A category is stored under `ResearchCategory.rawValue` here, where Kotlin stores the enum's name.
    // @verifies REQ-CONSENT-20#e
    func testWithdrawalMarksAreRecoveredFromAnUnreadableRecordAndGrantsNeverAre() {
        let record = #"{"blanketConsentGranted":"perhaps","contactConsentGranted":true,"#
            + #""contactMethod":"person@example.org","blanketConsentWithdrawnAt":"2026-02-01#000002","#
            + #""resultsOptInWithdrawnAt":"2026-02-01T10:00:00Z","#
            + #""categoryWithdrawnAt":{"Sleep Disorders":"2026-01-15#000001","NOT_A_CATEGORY":"2026-01-15#000001"}}"#
        ProtectedDefaults.standard.set(Data(record.utf8), forKey: researchKey)
        ProtectedDefaults.standard.set(unreadableWithdrawalList, forKey: studyWithdrawalsKey)

        let store = open()

        XCTAssertTrue(store.consentRecordUnreadable)
        XCTAssertTrue(store.researchConsent.blanketConsentWithdrawn, "said stop is held from the start")
        XCTAssertEqual(store.researchConsent.blanketConsentWithdrawnAt, "2026-02-01#000002")
        XCTAssertEqual(store.researchConsent.categoryWithdrawnAt, [ResearchCategory.sleep.rawValue: "2026-01-15#000001"])
        XCTAssertNil(store.researchConsent.resultsOptInWithdrawnAt, "a value that is not a mark is left behind")
        XCTAssertFalse(store.researchConsent.contactConsentGranted, "a grant is never read out of an unreadable record")
        XCTAssertEqual(store.researchConsent.contactMethod, "")
        XCTAssertEqual(store.exportWithdrawals()["blanketConsentWithdrawnAt"] as? String, "2026-02-01#000002")

        // After the person decides again the recovered marks are still the record.
        store.updateResearchConsent(categories(.depression))
        XCTAssertFalse(store.consentRecordUnreadable)
        XCTAssertEqual(open().researchConsent.blanketConsentWithdrawnAt, "2026-02-01#000002")
    }

    // @verifies REQ-CONSENT-20#e
    func testStudyWithdrawalsAreRecoveredFromAnUnreadableList() {
        ProtectedDefaults.standard.set(unreadableWithdrawalList, forKey: studyWithdrawalsKey)

        let store = open()

        XCTAssertEqual(store.studyWithdrawals, ["S-1": "2026-01-20#000001"])
        XCTAssertTrue(store.consentRecordUnreadable)
    }
}
