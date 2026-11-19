import Foundation
import Combine

// Persisted consent state — clinician grants, research consent, study participation audit trail.
// All state serialized to ProtectedDefaults: backup-excluded, file-protected files — NOT UserDefaults,
// whose plist is included in iCloud/iTunes backups and MDM-readable. Consent state reveals health
// conditions by inference (research categories, clinician grants). Consent is deliberately
// re-established on a new device, as on Android.
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
    private let expansionsKey    = "np.consent.clinician-expansions"
    private let invitationsKey   = "np.consent.study-invitations"
    private let studyWithdrawalsKey = "np.consent.study-withdrawals"
    /// Stored records that could not be read, as found (`REQ-CONSENT-20`). Never moved in a migration.
    private let quarantineKey    = "np.consent.quarantine"

    /// The records research admission depends on. One of them unreadable means the device cannot tell
    /// "never asked" from "said stop". The clinician keys are not here: an unreadable grant list reads as
    /// no grants, which already gives a clinician nothing.
    private var researchRecordKeys: Set<String> {
        [researchKey, studyWithdrawalsKey, PersistedKeys.PARTICIPATION_KEY, invitationsKey, quarantineKey]
    }

    /// Studies the user withdrew from, and when. UHDR-class and kept apart from `studyParticipations`
    /// (SHDR-class). A withdrawal stays here for good, so a restore or a re-presented descriptor cannot put
    /// the user back in a study they left (`SWR-MG-04`).
    @Published private(set) var studyWithdrawals: [String: Date] = [:]

    /// Stored consent records that could not be read, kept as they were found (`REQ-CONSENT-20`). Kept for good:
    /// a later version of the app may be able to read what this one could not.
    @Published private(set) var quarantinedBlobs: [QuarantinedConsentBlob] = []

    /// A record research admission depends on could not be read, and the person has not decided again since.
    /// While this holds no study is admitted, no consent request can be accepted, and the dashboard says so.
    /// It clears when the person commits a research-consent decision (`updateResearchConsent`,
    /// `withdrawBlanketResearchConsent`), which is then the record.
    var consentRecordUnreadable: Bool {
        quarantinedBlobs.contains { $0.redecidedAt == nil && researchRecordKeys.contains($0.key) }
    }

    /// The clock for withdrawal times; injectable so a test can order two withdrawals.
    private let now: () -> Date

    /// Checks the signature on every study descriptor before anything derived from it reaches the
    /// user (§5.3 step 1). The default refuses everything: see `RefusingStudyDescriptorVerifier`.
    private let descriptorVerifier: StudyDescriptorVerifier

    init(
        descriptorVerifier: StudyDescriptorVerifier = RefusingStudyDescriptorVerifier(),
        now: @escaping () -> Date = { Date() }
    ) {
        self.descriptorVerifier = descriptorVerifier
        self.now = now
        load()
    }

    // MARK: - Clinician consent

    /// Record a grant. Refused, and false returned, unless the grant is time-limited (§5.1, `REQ-CONSENT-18`):
    /// it carries an expiry and that day has not passed. (Kotlin also checks the expiry is a real "yyyy-MM-dd"
    /// day; here it is a `Date`, which cannot be anything else.)
    @discardableResult
    func grantClinicianAccess(_ grant: ClinicianConsentGrant) -> Bool {
        guard grant.isInForce(on: Date()) else { return false }
        clinicianGrants.removeAll { $0.id == grant.id }
        clinicianGrants.append(grant)
        save()
        return true
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
        // The differential is also nil for a grant that is no longer in force (`REQ-CONSENT-18`).
        let now = Date()
        guard let grant = clinicianGrants.first(where: { $0.id == request.grantID }),
              ConsentEngine.accessDifferential(for: grant, expandingTo: request.toTier, on: now) != nil
        else { return }

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
        let isWithdrawal = wasBlanketGranted && !state.blanketConsentGranted

        // `blanketConsentWithdrawnAt` is the store's to set, never the screen's. The UI commits a
        // whole state and cannot see the transition this method is built around; carrying the
        // committed value through would let a stale screen erase the record of the user having
        // said stop, which is the one thing §6.0 needs to survive.
        var next = withTimes(old: researchConsent, new: state)
        next.blanketConsentWithdrawnAt = isWithdrawal
            ? now()
            : researchConsent.blanketConsentWithdrawnAt
        researchConsent = next
        // A commit from the consent UI is the person deciding, so it is what ends a quarantine.
        markRedecided()
        withdrawUncoveredParticipations()
        save()

        if isWithdrawal {
            revokeResearchAnalytics()
        }
    }

    func withdrawBlanketResearchConsent() {
        let wasGranted = researchConsent.blanketConsentGranted
        // While the stored record is unreadable the device cannot know blanket consent was never granted, and
        // the person is saying stop now. That is recorded as a withdrawal (`REQ-CONSENT-20`).
        let mayHaveBeenGranted = !wasGranted && consentRecordUnreadable
        researchConsent.blanketConsentGranted = false
        researchConsent.blanketConsentGrantedAt = nil
        // Guarded on the transition, like the teardown below and like `updateResearchConsent`:
        // calling this when blanket consent was never granted stops nothing, and marking it would
        // silently bar an L2 participant from studies they are still consenting to.
        if wasGranted || mayHaveBeenGranted { researchConsent.blanketConsentWithdrawnAt = now() }
        markRedecided()
        withdrawUncoveredParticipations()
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
        ProtectedDefaults.standard.removeObject(forKey: PersistedKeys.RESEARCH_ANALYTICS_KEY)
        ResearchAnalyticsGate.reset()
    }

    func setCategoryConsent(_ category: ResearchCategory, granted: Bool) {
        var next = researchConsent
        next.categoryConsents[category] = granted
        researchConsent = withTimes(old: researchConsent, new: next)
        withdrawUncoveredParticipations()
        save()
    }

    /// Stop every participation the research consent no longer covers (`REQ-CONSENT-19`).
    ///
    /// A withdrawal used to change the consent state and leave `studyParticipations` alone, so a study joined
    /// under blanket consent stayed active after blanket consent was withdrawn, and anything that sends extracts
    /// for active participations would have kept sending (§5.3, `OI-PRIV-06` point 1). Run after every change to
    /// `researchConsent`, and when the store is read, so a participation is active only while
    /// `ConsentEngine.covers` holds for it: blanket withdrawal stops all of them, a category withdrawal stops the
    /// studies left with no consented category, and a study whose categories are not on record is covered by
    /// blanket consent alone.
    ///
    /// Only `StudyParticipationRecord.withdrawnAt` is written. `studyWithdrawals` is not: that is the list of
    /// studies the person named, and it bars a study for good, which a blanket or category withdrawal did not
    /// ask for. An engagement notification for a stopped study is closed with it, because it tells the person
    /// they are in a study they are no longer in.
    ///
    /// Skipped while the stored record is unreadable: the state in memory is then defaults, not the person's.
    private func withdrawUncoveredParticipations() {
        guard !consentRecordUnreadable else { return }
        let consent = researchConsent
        let invitations = pendingInvitations
        let stopped = Set(
            studyParticipations
                .filter { record in
                    // The categories of a study this device holds an invitation for, or nil if it holds none.
                    let categories = invitations.first { $0.studyID == record.studyID }?.researchCategories
                    return record.isActive && !ConsentEngine.covers(consent, categories: categories)
                }
                .map(\.studyID)
        )
        guard !stopped.isEmpty else { return }
        let at = Date()
        for idx in studyParticipations.indices
        where stopped.contains(studyParticipations[idx].studyID) && studyParticipations[idx].isActive {
            studyParticipations[idx].withdrawnAt = at
        }
        for idx in pendingInvitations.indices
        where stopped.contains(pendingInvitations[idx].studyID) && pendingInvitations[idx].isOpen
            && pendingInvitations[idx].posture == .engagementNotification {
            pendingInvitations[idx].decision = .declined(at: at)
        }
    }

    /// The person has committed a research-consent decision, which ends a quarantine (`REQ-CONSENT-20`).
    private func markRedecided() {
        guard consentRecordUnreadable else { return }
        let at = Date()
        let keys = researchRecordKeys
        for idx in quarantinedBlobs.indices
        where quarantinedBlobs[idx].redecidedAt == nil && keys.contains(quarantinedBlobs[idx].key) {
            quarantinedBlobs[idx].redecidedAt = at
        }
    }

    /// `new` with every withdrawal time the store owns taken from `old`, and stamped now on a true→false
    /// transition, so a screen that commits a whole state can neither set nor erase one (`SWR-MG-04`).
    /// The blanket time is handled by the callers, which also tear down analytics on that transition.
    private func withTimes(old: ResearchConsentState, new: ResearchConsentState) -> ResearchConsentState {
        var next = new
        // Read the clock only when something was withdrawn (as the Kotlin store does), and once for the whole commit, so
        // every decision withdrawn together carries one time and a plain grant never consumes a reading.
        var stamp: Date?
        func t() -> Date {
            if let stamp { return stamp }
            let read = now()
            stamp = read
            return read
        }
        func at(_ was: Bool, _ will: Bool, _ previous: Date?) -> Date? { was && !will ? t() : previous }
        next.contactConsentWithdrawnAt = at(old.contactConsentGranted, new.contactConsentGranted, old.contactConsentWithdrawnAt)
        next.resultsOptInWithdrawnAt = at(old.resultsOptIn, new.resultsOptIn, old.resultsOptInWithdrawnAt)
        next.suggestionPortalOptInWithdrawnAt = at(
            old.suggestionPortalOptIn, new.suggestionPortalOptIn, old.suggestionPortalOptInWithdrawnAt
        )
        var categories = old.categoryWithdrawnAt ?? [:]
        for category in ResearchCategory.allCases
        where old.categoryConsents[category] == true && new.categoryConsents[category] != true {
            categories[category.rawValue] = t()
        }
        next.categoryWithdrawnAt = categories.isEmpty ? nil : categories
        return next
    }

    // MARK: - Withdrawal records in a migration (SWR-MG-04)

    /// The withdrawal records in the shape the shared core merges (`mergeWithdrawal`): UTC instants, a category
    /// by its wire name, a study by its id. Grants are not here and never travel: a restore can only reduce
    /// what a phone is consented to.
    func exportWithdrawals() -> [String: Any] {
        let r = researchConsent
        var out: [String: Any] = [:]
        if let t = r.blanketConsentWithdrawnAt { out["blanketConsentWithdrawnAt"] = ConsentTime.string(t) }
        if let t = r.contactConsentWithdrawnAt { out["contactConsentWithdrawnAt"] = ConsentTime.string(t) }
        if let t = r.resultsOptInWithdrawnAt { out["resultsOptInWithdrawnAt"] = ConsentTime.string(t) }
        if let t = r.suggestionPortalOptInWithdrawnAt { out["suggestionPortalOptInWithdrawnAt"] = ConsentTime.string(t) }
        out["categoryWithdrawals"] = (r.categoryWithdrawnAt ?? [:]).mapValues(ConsentTime.string)
        out["studyWithdrawals"] = studyWithdrawals.mapValues(ConsentTime.string)
        return out
    }

    /// Apply merged withdrawal records. Which of them are later than the ones held here is the core's to say
    /// (`applyWithdrawal`), so every platform applies a restore the same way; this switches the matching grants
    /// off and records the times. A grant made here after the withdrawal stands, and nothing here turns one on.
    func applyWithdrawals(_ incoming: [String: Any]) throws {
        let verdict = try NppsCore.migration([
            "op": "applyWithdrawal", "held": exportWithdrawals(), "incoming": incoming,
        ])
        let applied = verdict["applied"] as? [String: Any] ?? [:]
        let keys = Set(applied["times"] as? [String] ?? [])
        func time(_ key: String) -> Date? {
            guard keys.contains(key), let text = incoming[key] as? String else { return nil }
            return ConsentTime.date(text)
        }
        var r = researchConsent
        if let t = time("blanketConsentWithdrawnAt") {
            r.blanketConsentGranted = false
            r.blanketConsentGrantedAt = nil
            r.blanketConsentWithdrawnAt = t
        }
        if let t = time("contactConsentWithdrawnAt") {
            r.contactConsentGranted = false
            r.contactConsentWithdrawnAt = t
        }
        if let t = time("resultsOptInWithdrawnAt") {
            r.resultsOptIn = false
            r.resultsOptInWithdrawnAt = t
        }
        if let t = time("suggestionPortalOptInWithdrawnAt") {
            r.suggestionPortalOptIn = false
            r.suggestionPortalOptInWithdrawnAt = t
        }
        let theirCategories = incoming["categoryWithdrawals"] as? [String: String] ?? [:]
        var categories = r.categoryWithdrawnAt ?? [:]
        for name in applied["categoryWithdrawals"] as? [String] ?? [] {
            guard let category = ResearchCategory(rawValue: name), let t = theirCategories[name].flatMap(ConsentTime.date) else {
                continue
            }
            r.categoryConsents[category] = false
            categories[name] = t
        }
        r.categoryWithdrawnAt = categories.isEmpty ? nil : categories
        let theirStudies = incoming["studyWithdrawals"] as? [String: String] ?? [:]
        for id in applied["studyWithdrawals"] as? [String] ?? [] {
            guard let t = theirStudies[id].flatMap(ConsentTime.date) else { continue }
            studyWithdrawals[id] = t
            for idx in studyParticipations.indices where studyParticipations[idx].studyID == id && studyParticipations[idx].isActive {
                studyParticipations[idx].withdrawnAt = t
            }
        }
        let blanketApplied = time("blanketConsentWithdrawnAt") != nil
        researchConsent = r
        // A restored blanket or category withdrawal stops what it stops here too (`REQ-CONSENT-19`).
        withdrawUncoveredParticipations()
        save()
        if blanketApplied { revokeResearchAnalytics() }
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
            studyAlreadyDecided: hasDecided(studyID: descriptor.studyID),
            consentRecordReadable: !consentRecordUnreadable
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
            || studyWithdrawals[studyID] != nil
    }

    /// §6.3 step 4, *Yes*. Only a consent request can be accepted: an engagement notification was
    /// never a question, and accepting it would record a second participation for a study the
    /// user is already in.
    func acceptInvitation(studyID: String) {
        guard let idx = pendingInvitations.firstIndex(where: { $0.studyID == studyID }),
              pendingInvitations[idx].posture == .consentRequest,
              pendingInvitations[idx].isOpen
        else { return }
        // A request that arrived under a consent since withdrawn, or while the stored record is unreadable,
        // cannot be accepted: accepting would start a flow the withdrawal stopped (`REQ-CONSENT-19` d). The
        // invitation stays open, and is answerable again if the person consents again.
        guard !consentRecordUnreadable,
              ConsentEngine.covers(researchConsent, categories: pendingInvitations[idx].researchCategories)
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
            // Joined, and nothing sent: the transmission fields stay empty until an extract goes out.
            enrolledAt: date
        )
        studyParticipations.append(record)
        save()
    }

    func withdrawFromStudy(studyID: String) {
        if let idx = studyParticipations.firstIndex(where: { $0.studyID == studyID && $0.isActive }) {
            let at = now()
            studyParticipations[idx].withdrawnAt = at
            studyWithdrawals[studyID] = at
            save()
        }
    }

    // MARK: - Persistence

    /// Read one stored record. A record that is there and cannot be read is quarantined, never dropped: the
    /// caller keeps its default, and `quarantine` keeps the bytes.
    private func read<T: Decodable>(_ type: T.Type, forKey key: String) -> T? {
        guard let data = ProtectedDefaults.standard.data(forKey: key) else { return nil }
        if let decoded = try? JSONDecoder().decode(type, from: data) { return decoded }
        quarantine(key: key, blob: data)
        return nil
    }

    /// Keep `blob` as found, and write the quarantine list at once, before any `save` can replace the record
    /// under `key` with defaults (`REQ-CONSENT-20`).
    private func quarantine(key: String, blob: Data) {
        guard !quarantinedBlobs.contains(where: { $0.key == key && $0.blob == blob && $0.redecidedAt == nil })
        else { return }
        quarantinedBlobs.append(QuarantinedConsentBlob(key: key, blob: blob, quarantinedAt: Date()))
        saveQuarantine()
    }

    private func saveQuarantine() {
        if let data = try? JSONEncoder().encode(quarantinedBlobs) {
            ProtectedDefaults.standard.set(data, forKey: quarantineKey)
        }
    }

    private func load() {
        // First, so a record quarantined below is added to what is already held. If the list itself cannot be
        // read, its bytes become the first entry of a new one.
        if let decoded = read([QuarantinedConsentBlob].self, forKey: quarantineKey) { quarantinedBlobs = decoded }
        if let decoded = read([ClinicianConsentGrant].self, forKey: grantsKey) { clinicianGrants = decoded }
        if let decoded = read(ResearchConsentState.self, forKey: researchKey) { researchConsent = decoded }
        let storedParticipations = read([StudyParticipationRecord].self, forKey: PersistedKeys.PARTICIPATION_KEY) ?? []
        if let decoded = read([String: Date].self, forKey: studyWithdrawalsKey) { studyWithdrawals = decoded }
        // Expansion requests persist because §6.1 calls for a *persistent* notification: one the
        // user can dismiss by backgrounding the app is not one.
        if let decoded = read([ClinicianAccessExpansionRequest].self, forKey: expansionsKey) {
            expansionRequests = decoded
        }
        // Invitations persist for the same reason expansion requests do, and for one more: §6.3
        // step 4 gives the user two business days for an answer to a question, which an inbox
        // emptied by the next app launch cannot hold. While they were in-memory only, an accepted
        // study's participation record outlived the invitation that recorded the acceptance, so
        // the trail and the inbox disagreed after every restart.
        if let decoded = read([StudyInvitation].self, forKey: invitationsKey) { pendingInvitations = decoded }

        // Two repairs to what was stored before `OI-PRIV-06`: a record that called its enrolment time a
        // transmission, and a participation left active by a withdrawal that should have stopped it.
        studyParticipations = storedParticipations.map { $0.normalised() }
        withdrawUncoveredParticipations()
        if studyParticipations != storedParticipations { save() }
    }

    private func save() {
        if let data = try? JSONEncoder().encode(clinicianGrants) {
            ProtectedDefaults.standard.set(data, forKey: grantsKey)
        }
        if let data = try? JSONEncoder().encode(researchConsent) {
            ProtectedDefaults.standard.set(data, forKey: researchKey)
        }
        if let data = try? JSONEncoder().encode(studyParticipations) {
            ProtectedDefaults.standard.set(data, forKey: PersistedKeys.PARTICIPATION_KEY)
        }
        if let data = try? JSONEncoder().encode(studyWithdrawals) {
            ProtectedDefaults.standard.set(data, forKey: studyWithdrawalsKey)
        }
        if let data = try? JSONEncoder().encode(expansionRequests) {
            ProtectedDefaults.standard.set(data, forKey: expansionsKey)
        }
        if let data = try? JSONEncoder().encode(pendingInvitations) {
            ProtectedDefaults.standard.set(data, forKey: invitationsKey)
        }
        if !quarantinedBlobs.isEmpty { saveQuarantine() }
    }
}

/// The form a withdrawal time takes in a bundle: a UTC instant to the second (`2026-03-01T00:00:00Z`), which orders
/// as text, so the core can say which of two is later without parsing (`SWR-MG-04`).
enum ConsentTime {
    private static let formatter: DateFormatter = {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.timeZone = TimeZone(identifier: "UTC")
        f.dateFormat = "yyyy-MM-dd'T'HH:mm:ss'Z'"
        return f
    }()

    static func string(_ date: Date) -> String { formatter.string(from: date) }
    static func date(_ text: String) -> Date? { formatter.date(from: text) }
}
