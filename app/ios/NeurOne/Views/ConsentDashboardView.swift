import SwiftUI

// Consent management dashboard — CLAUDE.md §6.
// Surfaces: active clinician grants · research consent status ·
// study participation audit trail · pending study invitations.

struct ConsentDashboardView: View {

    @EnvironmentObject private var consentStore: ConsentStore
    @State private var showOnboarding = false
    @State private var showNewClinicianGrant = false
    @State private var selectedInvitation: StudyInvitation?
    @State private var selectedExpansionRequest: ClinicianAccessExpansionRequest?
    @State private var grantPendingRevoke: ClinicianConsentGrant?
    @State private var participationPendingWithdraw: StudyParticipationRecord?
    @State private var showBlanketWithdrawConfirmation = false

    var body: some View {
        NavigationStack {
            List {
                clinicianSection
                accessRequestsSection
                researchSection
                studyHistorySection
                pendingInvitationsSection
            }
            .listStyle(.insetGrouped)
            .navigationTitle("DASHBOARD_TITLE")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Menu {
                        Button("DASHBOARD_RESEARCH_PREFS_BUTTON") { showOnboarding = true }
                        NavigationLink("DASHBOARD_RESEARCH_PORTAL_LINK") {
                            ResearchSuggestionPortalView()
                        }
                    } label: {
                        Image(systemName: "ellipsis.circle")
                    }
                }
            }
            .sheet(isPresented: $showOnboarding) {
                ConsentOnboardingView(isPresented: $showOnboarding)
                    .environmentObject(consentStore)
            }
            .sheet(isPresented: $showNewClinicianGrant) {
                NewClinicianGrantView()
                    .environmentObject(consentStore)
            }
            .sheet(item: $selectedInvitation) { invitation in
                StudyInvitationView(invitation: invitation)
                    .environmentObject(consentStore)
            }
            .sheet(item: $selectedExpansionRequest) { request in
                ClinicianExpansionRequestView(request: request)
                    .environmentObject(consentStore)
            }
            // ISC-76: confirmation before revoking clinician access
            .confirmationDialog(
                "DASHBOARD_REVOKE_DIALOG_TITLE",
                isPresented: Binding(
                    get: { grantPendingRevoke != nil },
                    set: { if !$0 { grantPendingRevoke = nil } }
                ),
                titleVisibility: .visible
            ) {
                if let grant = grantPendingRevoke {
                    Button(String(format: String(localized: "DASHBOARD_REVOKE_ACCESS_FORMAT"),
                                  grant.clinicianName), role: .destructive) {
                        consentStore.revokeClinicianAccess(grantID: grant.id)
                        grantPendingRevoke = nil
                    }
                }
                Button("COMMON_CANCEL", role: .cancel) { grantPendingRevoke = nil }
            } message: {
                if let grant = grantPendingRevoke {
                    Text(String(format: String(localized: "DASHBOARD_REVOKE_MESSAGE_FORMAT"),
                                grant.clinicianName, grant.clinicianOrganization))
                }
            }
            // ISC-77: confirmation before withdrawing from a study
            .confirmationDialog(
                "DASHBOARD_WITHDRAW_DIALOG_TITLE",
                isPresented: Binding(
                    get: { participationPendingWithdraw != nil },
                    set: { if !$0 { participationPendingWithdraw = nil } }
                ),
                titleVisibility: .visible
            ) {
                if let record = participationPendingWithdraw {
                    Button(String(format: String(localized: "DASHBOARD_WITHDRAW_FROM_FORMAT"),
                                  record.studyID), role: .destructive) {
                        consentStore.withdrawFromStudy(studyID: record.studyID)
                        participationPendingWithdraw = nil
                    }
                }
                Button("COMMON_CANCEL", role: .cancel) { participationPendingWithdraw = nil }
            } message: {
                Text("DASHBOARD_WITHDRAW_DIALOG_MESSAGE")
            }
            // Blanket (L3) withdrawal — the named path, matching Android's ConsentDashboardScreen.
            // Routed through `withdrawBlanketResearchConsent()` rather than an edit-and-commit,
            // so the analytics teardown is explicit at the call site as well as guarded at the
            // store's ingestion point (CLAUDE.md §6.2.5).
            .confirmationDialog(
                "DASHBOARD_STOP_PREAPPROVING_DIALOG_TITLE",
                isPresented: $showBlanketWithdrawConfirmation,
                titleVisibility: .visible
            ) {
                Button("DASHBOARD_STOP_PREAPPROVING_CONFIRM", role: .destructive) {
                    consentStore.withdrawBlanketResearchConsent()
                }
                Button("COMMON_CANCEL", role: .cancel) {}
            } message: {
                Text("DASHBOARD_STOP_PREAPPROVING_MESSAGE")
            }
        }
    }

    // MARK: - Sections

    private var clinicianSection: some View {
        Section {
            if consentStore.clinicianGrants.isEmpty {
                Text("DASHBOARD_NO_CLINICIANS")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            } else {
                ForEach(consentStore.clinicianGrants) { grant in
                    ClinicianGrantRow(grant: grant) {
                        grantPendingRevoke = grant  // ISC-76 — triggers confirmation dialog
                    }
                }
            }
            Button {
                showNewClinicianGrant = true
            } label: {
                Label("DASHBOARD_ADD_CLINICIAN", systemImage: "plus")
            }
        } header: {
            Text("DASHBOARD_SECTION_CLINICIANS")
        } footer: {
            Text("DASHBOARD_CLINICIANS_FOOTER")
                .font(.caption)
        }
    }

    // §6.1's persistent user notification. It is a section on the consent dashboard rather than a
    // transient banner because the request outlives any one launch of the app, and because the
    // place the user manages clinician access is where a change to it should appear.
    private var accessRequestsSection: some View {
        Section {
            let pending = consentStore.expansionRequests.filter(\.isPending)
            if pending.isEmpty {
                Text("EXPANSION_NONE")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            } else {
                ForEach(pending) { request in
                    if let grant = consentStore.clinicianGrants.first(where: { $0.id == request.grantID }),
                       let differential = ConsentEngine.accessDifferential(for: grant, expandingTo: request.toTier) {
                        Button {
                            selectedExpansionRequest = request
                        } label: {
                            VStack(alignment: .leading, spacing: 4) {
                                Text(String(format: String(localized: "EXPANSION_HEADER_FORMAT"),
                                            differential.clinicianName, differential.organization))
                                    .font(.subheadline.bold())
                                Text(String(format: String(localized: "EXPANSION_TIER_CHANGE_FORMAT"),
                                            differential.fromTier.rawValue, differential.toTier.rawValue))
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                                if let asked = request.questionSentAt {
                                    Text(String(format: String(localized: "EXPANSION_ASKED_LABEL"),
                                                asked.formatted(.dateTime.month().day().year())))
                                        .font(.caption2)
                                        .foregroundColor(.orange)
                                } else {
                                    Text("EXPANSION_PENDING_BADGE")
                                        .font(.caption2)
                                        .foregroundColor(.orange)
                                }
                            }
                        }
                        .foregroundColor(.primary)
                    }
                    // A pending request whose differential no longer holds — the grant was revoked
                    // or already widened — is not rendered. `approveExpansion` fails closed on the
                    // same condition, so there is nothing here the user could usefully answer.
                }
            }
        } header: {
            Text("DASHBOARD_SECTION_ACCESS_REQUESTS")
        }
    }

    private var researchSection: some View {
        Section {
            researchConsentSummaryRow
            // L3 posture is a separate row from the L2 category list below, because they are
            // separate axes (§6.2.2): scope versus ask-me-each-time. Withdrawal of one must
            // stay reachable without touching the other.
            if consentStore.researchConsent.blanketConsentGranted {
                Button("DASHBOARD_STOP_PREAPPROVING_BUTTON", role: .destructive) {
                    showBlanketWithdrawConfirmation = true
                }
                .font(.subheadline)
            } else {
                Text("DASHBOARD_POSTURE_ASKED")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            ForEach(ResearchCategory.allCases, id: \.self) { category in
                let granted = consentStore.researchConsent.categoryConsents[category] ?? false
                HStack {
                    Text(category.displayName).font(.subheadline)
                    Spacer()
                    Image(systemName: granted ? "checkmark.circle.fill" : "circle")
                        .foregroundColor(granted ? .green : .secondary)
                }
            }
        } header: {
            Text("DASHBOARD_SECTION_RESEARCH")
        }
    }

    private var researchConsentSummaryRow: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(consentStore.researchConsent.blanketConsentGranted
                     ? "DASHBOARD_BLANKET_APPROVED"
                     : "DASHBOARD_PER_CATEGORY")
                    .font(.subheadline.bold())
                Text(consentStore.researchConsent.contactConsentGranted
                     ? String(format: String(localized: "DASHBOARD_CONTACT_FORMAT"),
                               redacted(consentStore.researchConsent.contactMethod))
                     : String(localized: "DASHBOARD_NO_CONTACT"))
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            Spacer()
            if consentStore.researchConsent.blanketConsentGranted {
                Image(systemName: "checkmark.seal.fill").foregroundColor(.green)
            }
        }
    }

    // Summary row shows redacted contact only; full value is in Research Preferences.
    // Email → "•••@domain.com". Phone → "••• ••• ••\(last2)".
    private func redacted(_ contact: String) -> String {
        if let atIdx = contact.firstIndex(of: "@") {
            let domain = String(contact[contact.index(after: atIdx)...])
            return "•••@\(domain)"
        }
        let digits = contact.filter(\.isNumber)
        let suffix = digits.count >= 2 ? String(digits.suffix(2)) : digits
        return "••• ••• ••\(suffix)"
    }

    private var studyHistorySection: some View {
        Section("DASHBOARD_SECTION_STUDY_HISTORY") {
            if consentStore.studyParticipations.isEmpty {
                Text("DASHBOARD_NO_STUDIES")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            } else {
                ForEach(consentStore.studyParticipations) { record in
                    // ISC-77: Withdraw button on active participation rows
                    StudyParticipationRow(record: record) {
                        if record.isActive { participationPendingWithdraw = record }
                    }
                }
            }
        }
    }

    /// The invitation inbox. Rows carry their posture, because the two are not the same object to
    /// the user: a consent request is waiting on them, an engagement notification is telling them
    /// about a study they are already in under L3 (§6.2). A row that looked the same either way
    /// would leave the user to guess which.
    private var pendingInvitationsSection: some View {
        Section("DASHBOARD_SECTION_PENDING") {
            let openInvitations = consentStore.pendingInvitations.filter(\.isOpen)
            if openInvitations.isEmpty {
                Text("DASHBOARD_NO_INVITATIONS")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            } else {
                ForEach(openInvitations) { invitation in
                    Button {
                        selectedInvitation = invitation
                    } label: {
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(invitation.studyTitle).font(.subheadline.bold())
                                Text(invitation.studyID).font(.caption).foregroundColor(.secondary)
                                let isNotification = invitation.posture == .engagementNotification
                                let badge: LocalizedStringKey = isNotification
                                    ? "INVITATION_ENGAGEMENT_BADGE"
                                    : "INVITATION_REQUEST_BADGE"
                                Text(badge)
                                    .font(.caption2)
                                    .foregroundColor(isNotification ? .secondary : .orange)
                            }
                            Spacer()
                            Image(systemName: "chevron.right").foregroundColor(.secondary)
                        }
                    }
                    .foregroundColor(.primary)
                }
            }
        }
    }
}

// MARK: - Supporting views

struct ClinicianGrantRow: View {
    let grant: ClinicianConsentGrant
    let onRevoke: () -> Void   // ISC-76

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text(grant.clinicianName).font(.subheadline.bold())
                    Text(grant.clinicianOrganization).font(.caption).foregroundColor(.secondary)
                    Text(grant.tier.monthlyPrice).font(.caption2).foregroundColor(.secondary)
                }
                Spacer()
                Button("DASHBOARD_REVOKE_BUTTON", role: .destructive, action: onRevoke)
                    .font(.caption)
                    .buttonStyle(.borderless)
            }
            // Granted elements (ISC-75)
            Text(String(format: String(localized: "DASHBOARD_ACCESS_FORMAT"),
                        grant.approvedElements.map(\.displayName).sorted().joined(separator: ", ")))
                .font(.caption2)
                .foregroundColor(.secondary)
                .lineLimit(nil)
                .fixedSize(horizontal: false, vertical: true)
            // A "from today onwards only" answer to §6.1's retroactive question has to stay
            // visible after the decision, or the user cannot tell which of the two answers they
            // gave. Absent when every scope covers prior data, which is the ordinary case.
            if !grant.forwardOnlyElements.isEmpty {
                Text(String(format: String(localized: "DASHBOARD_FORWARD_ONLY_FORMAT"),
                            grant.forwardOnlyElements.map(\.displayName).sorted().joined(separator: ", ")))
                    .font(.caption2)
                    .foregroundColor(.secondary)
                    .lineLimit(nil)
                    .fixedSize(horizontal: false, vertical: true)
            }
            HStack {
                Label(String(format: String(localized: "DASHBOARD_GRANTED_FORMAT"),
                             grant.grantedAt.formatted(.dateTime.month().day().year())),
                      systemImage: "checkmark.shield")
                    .font(.caption2).foregroundColor(.green)
                Spacer()
                if let exp = grant.expiresAt {
                    Text(String(format: String(localized: "DASHBOARD_EXPIRES_FORMAT"),
                                exp.formatted(.dateTime.month().day())))
                        .font(.caption2).foregroundColor(.orange)
                }
            }
        }
    }
}

struct StudyParticipationRow: View {
    let record: StudyParticipationRecord
    let onWithdraw: () -> Void  // ISC-77

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(record.studyID).font(.subheadline.bold())
                Text(String(format: String(localized: "DASHBOARD_DATA_SHARED_FORMAT"),
                            record.transmittedAt.formatted(.dateTime.month().day().year())))
                    .font(.caption).foregroundColor(.secondary)
                if !record.isActive {
                    Label("DASHBOARD_WITHDRAWN_LABEL", systemImage: "xmark.circle")
                        .font(.caption2).foregroundColor(.orange)
                }
            }
            Spacer()
            if record.isActive {
                Button("DASHBOARD_WITHDRAW_BUTTON", role: .destructive, action: onWithdraw)
                    .font(.caption)
                    .buttonStyle(.borderless)
            }
        }
    }
}

/// §6.3's per-project consent surface — and, under L3, its engagement notification.
///
/// Both postures render from the same invitation, and the screen says which one it is rather than
/// leaving it to be inferred from the buttons. §6.2 locks that an L3 user "still receives per-study
/// *engagement* notifications, **not consent requests**": they are in the study already, so this
/// screen tells them so and offers the per-study opt-out, and there is nothing here to accept.
///
/// The "what they CANNOT see" list is the complement of the approved elements, computed on the
/// invitation, and the irreversibility notice is a locale key. Neither travels with the descriptor:
/// the party asking for access supplies the study's facts, not the sentences describing them.
struct StudyInvitationView: View {
    let invitation: StudyInvitation
    @EnvironmentObject private var consentStore: ConsentStore
    @Environment(\.dismiss) private var dismiss
    @State private var showParticipateConfirmation = false   // ISC-79
    @State private var showLeaveConfirmation = false

    private var isEngagementNotification: Bool {
        invitation.posture == .engagementNotification
    }

    private var screenTitle: LocalizedStringKey {
        isEngagementNotification ? "INVITATION_ENGAGEMENT_TITLE" : "INVITATION_TITLE"
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    Text(invitation.studyTitle).font(.title2.bold())
                    Text(String(format: String(localized: "INVITATION_STUDY_ID_FORMAT"),
                                invitation.studyID))
                        .font(.caption).foregroundColor(.secondary)

                    if isEngagementNotification {
                        VStack(alignment: .leading, spacing: 6) {
                            Label("INVITATION_ENGAGEMENT_HEADING", systemImage: "checkmark.seal.fill")
                                .font(.subheadline.bold())
                            Text("INVITATION_ENGAGEMENT_BODY")
                                .font(.caption).foregroundColor(.secondary)
                        }
                        .padding(12)
                        .background(Color.accentColor.opacity(0.08))
                        .clipShape(RoundedRectangle(cornerRadius: 10))
                    }

                    Divider()

                    Text("INVITATION_CAN_SEE_HEADING").font(.headline)
                    ForEach(invitation.approvedElements.sorted { $0.rawValue < $1.rawValue },
                            id: \.rawValue) { element in
                        Label(element.displayName, systemImage: "checkmark.circle.fill")
                            .font(.subheadline)
                            .foregroundColor(.green)
                    }

                    Text("INVITATION_CANNOT_SEE_HEADING").font(.headline)
                    ForEach(invitation.cannotLearn.sorted { $0.rawValue < $1.rawValue },
                            id: \.rawValue) { element in
                        Label(element.displayName, systemImage: "xmark.circle.fill")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }

                    // The §5.3 parameters the ingestion gate checked, shown rather than merely
                    // enforced: they are what "anonymised" means for this study.
                    Text(String(format: String(localized: "INVITATION_ANONYMISATION_FORMAT"),
                                String(invitation.kAnonymity),
                                String(invitation.dateRoundingDays)))
                        .font(.caption).foregroundColor(.secondary)

                    Divider()

                    VStack(alignment: .leading, spacing: 8) {
                        Label("INVITATION_IMPORTANT_LABEL", systemImage: "info.circle.fill")
                            .font(.subheadline.bold()).foregroundColor(.orange)
                        Text(ConsentEngine.irreversibilityNotice)
                            .font(.caption).foregroundColor(.secondary)
                    }
                    .padding(12)
                    .background(Color.orange.opacity(0.08))
                    .clipShape(RoundedRectangle(cornerRadius: 10))

                    if let askedAt = invitation.questionSentAt {
                        Text(String(format: String(localized: "INVITATION_ASKED_LABEL"),
                                    askedAt.formatted(.dateTime.month().day().year())))
                            .font(.caption).foregroundColor(.secondary)
                    }
                }
                .padding()
            }
            .navigationTitle(screenTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("COMMON_CANCEL") { dismiss() }
                }
            }
            .safeAreaInset(edge: .bottom) {
                if isEngagementNotification {
                    // No accept: L3 already answered. The only decision left is to leave, which
                    // withdraws the participation ingestion recorded.
                    Button("INVITATION_LEAVE_BUTTON") { showLeaveConfirmation = true }
                        .buttonStyle(.bordered)
                        .foregroundColor(.red)
                        .frame(maxWidth: .infinity)
                        .padding()
                        .background(.regularMaterial)
                } else {
                    VStack(spacing: 8) {
                        HStack(spacing: 12) {
                            Button("INVITATION_DECLINE_BUTTON") {
                                consentStore.declineInvitation(studyID: invitation.studyID)
                                dismiss()
                            }
                            .buttonStyle(.bordered)
                            .foregroundColor(.red)

                            // ISC-79: confirmation must include the irreversibility notice.
                            Button("INVITATION_PARTICIPATE_BUTTON") {
                                showParticipateConfirmation = true
                            }
                            .buttonStyle(.borderedProminent)
                            .frame(maxWidth: .infinity)
                        }
                        // §6.3 step 4's third response. It decides nothing, so it is not one of
                        // the two answer buttons: the invitation stays open behind it.
                        Button("INVITATION_ASK_BUTTON") {
                            consentStore.askQuestionAboutInvitation(studyID: invitation.studyID)
                            dismiss()
                        }
                        .buttonStyle(.borderless)
                        .font(.subheadline)
                        Text("INVITATION_ASK_EXPLAINER")
                            .font(.caption2).foregroundColor(.secondary)
                            .multilineTextAlignment(.center)
                    }
                    .padding()
                    .background(.regularMaterial)
                }
            }
            .confirmationDialog(
                "INVITATION_CONFIRM_DIALOG_TITLE",
                isPresented: $showParticipateConfirmation,
                titleVisibility: .visible
            ) {
                Button("INVITATION_CONFIRM_BUTTON") {
                    consentStore.acceptInvitation(studyID: invitation.studyID)
                    dismiss()
                }
                Button("COMMON_CANCEL", role: .cancel) {}
            } message: {
                Text(ConsentEngine.irreversibilityNotice)
            }
            .confirmationDialog(
                "INVITATION_LEAVE_DIALOG_TITLE",
                isPresented: $showLeaveConfirmation,
                titleVisibility: .visible
            ) {
                Button("INVITATION_LEAVE_CONFIRM_BUTTON", role: .destructive) {
                    consentStore.declineInvitation(studyID: invitation.studyID)
                    dismiss()
                }
                Button("COMMON_CANCEL", role: .cancel) {}
            } message: {
                Text(ConsentEngine.irreversibilityNotice)
            }
        }
    }
}

/// §6.1's access-expansion review: the differential consent document, then the two decisions.
///
/// The decisions are on separate steps, not two controls on one screen. §6.1 requires retroactive
/// and prospective access to be "presented as separate consent decisions even if made
/// simultaneously", and a single screen carrying an Approve button and a history checkbox presents
/// one decision with a modifier on it. Approving here answers only the forward-looking question;
/// the history question is asked afterwards, on its own, and says in as many words that the user
/// may answer it either way.
///
/// There is no "deny" on the second step. By then the user has approved the expansion going
/// forward, and the two answers to what remains — earlier sessions in, or earlier sessions out —
/// are both there.
struct ClinicianExpansionRequestView: View {
    let request: ClinicianAccessExpansionRequest
    @EnvironmentObject private var consentStore: ConsentStore
    @Environment(\.dismiss) private var dismiss

    private enum Step { case review, history, asked }
    @State private var step: Step = .review

    private var differential: ClinicianAccessDifferential? {
        guard let grant = consentStore.clinicianGrants.first(where: { $0.id == request.grantID })
        else { return nil }
        return ConsentEngine.accessDifferential(for: grant, expandingTo: request.toTier)
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                if let differential {
                    VStack(alignment: .leading, spacing: 20) {
                        header(differential)
                        Divider()
                        switch step {
                        case .review:  reviewStep(differential)
                        case .history: historyStep(differential)
                        case .asked:   askedStep
                        }
                    }
                    .padding()
                }
            }
            .navigationTitle("EXPANSION_TITLE")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("COMMON_CANCEL") { dismiss() }
                }
            }
        }
    }

    private func header(_ d: ClinicianAccessDifferential) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(String(format: String(localized: "EXPANSION_HEADER_FORMAT"),
                        d.clinicianName, d.organization))
                .font(.title3.bold())
            Text(String(format: String(localized: "EXPANSION_TIER_CHANGE_FORMAT"),
                        d.fromTier.rawValue, d.toTier.rawValue))
                .font(.subheadline)
                .foregroundColor(.secondary)
            Text(String(format: String(localized: "EXPANSION_PRICE_FORMAT"), d.toTier.monthlyPrice))
                .font(.caption)
                .foregroundColor(.secondary)
        }
    }

    /// The differential consent document. What is new is listed first and on its own, because the
    /// change is the thing being consented to; the unchanged sets are context.
    private func reviewStep(_ d: ClinicianAccessDifferential) -> some View {
        VStack(alignment: .leading, spacing: 20) {
            elementList("EXPANSION_NEWLY_VISIBLE_HEADING", d.newlyVisibleElements,
                        icon: "plus.circle.fill", color: .orange)
            elementList("EXPANSION_ALREADY_VISIBLE_HEADING", d.alreadyVisibleElements,
                        icon: "checkmark.circle.fill", color: .green)
            elementList("EXPANSION_STILL_WITHHELD_HEADING", d.stillNotAccessibleElements,
                        icon: "xmark.circle.fill", color: .secondary)

            Divider()

            VStack(alignment: .leading, spacing: 8) {
                Text("EXPANSION_PROSPECTIVE_HEADING").font(.headline)
                Text(String(format: String(localized: "EXPANSION_PROSPECTIVE_BODY"), d.clinicianName))
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }

            VStack(spacing: 12) {
                Button("EXPANSION_APPROVE_BUTTON") { step = .history }
                    .buttonStyle(.borderedProminent)
                    .frame(maxWidth: .infinity)
                Button("EXPANSION_DENY_BUTTON") {
                    consentStore.denyExpansion(requestID: request.id)
                    dismiss()
                }
                .buttonStyle(.bordered)
                .foregroundColor(.red)
                .frame(maxWidth: .infinity)
                Button("EXPANSION_ASK_BUTTON") {
                    consentStore.askQuestionAboutExpansion(requestID: request.id)
                    step = .asked
                }
                .buttonStyle(.bordered)
                .frame(maxWidth: .infinity)
            }
        }
    }

    /// The retroactive decision, asked separately and after the fact — never as a checkbox on the
    /// approval above. Both answers are buttons of equal weight: neither is the default.
    private func historyStep(_ d: ClinicianAccessDifferential) -> some View {
        VStack(alignment: .leading, spacing: 20) {
            elementList("EXPANSION_NEWLY_VISIBLE_HEADING", d.newlyVisibleElements,
                        icon: "plus.circle.fill", color: .orange)

            VStack(alignment: .leading, spacing: 8) {
                Text("EXPANSION_HISTORY_HEADING").font(.headline)
                Text(String(format: String(localized: "EXPANSION_HISTORY_BODY"), d.clinicianName))
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }

            VStack(spacing: 12) {
                Button("EXPANSION_HISTORY_FORWARD_ONLY_BUTTON") {
                    consentStore.approveExpansion(requestID: request.id, includePriorData: false)
                    dismiss()
                }
                .buttonStyle(.bordered)
                .frame(maxWidth: .infinity)
                Button("EXPANSION_HISTORY_INCLUDE_BUTTON") {
                    consentStore.approveExpansion(requestID: request.id, includePriorData: true)
                    dismiss()
                }
                .buttonStyle(.bordered)
                .frame(maxWidth: .infinity)
            }
        }
    }

    /// Asking is not deciding: the request stays pending, and the copy says so rather than
    /// implying a message was delivered — there is no outbound clinician channel yet
    /// (`OI-CONSENT-05`).
    private var askedStep: some View {
        VStack(alignment: .leading, spacing: 16) {
            Label("EXPANSION_ASK_BUTTON", systemImage: "questionmark.circle.fill")
                .font(.headline)
                .foregroundColor(.orange)
            Text("EXPANSION_ASK_EXPLAINER")
                .font(.subheadline)
                .foregroundColor(.secondary)
            Button("COMMON_CANCEL") { dismiss() }
                .buttonStyle(.bordered)
                .frame(maxWidth: .infinity)
        }
    }

    private func elementList(
        _ heading: LocalizedStringKey,
        _ elements: Set<UHDRElement>,
        icon: String,
        color: Color
    ) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(heading).font(.headline)
            ForEach(elements.map(\.displayName).sorted(), id: \.self) { name in
                Label(name, systemImage: icon)
                    .font(.subheadline)
                    .foregroundColor(color)
            }
        }
    }
}

struct NewClinicianGrantView: View {
    @EnvironmentObject private var consentStore: ConsentStore
    @Environment(\.dismiss) private var dismiss
    @State private var name = ""
    @State private var organization = ""
    @State private var selectedTier: ClinicianUseCaseTier = .monitor
    @State private var selectedUseCases = Set<String>()

    var body: some View {
        NavigationStack {
            Form {
                Section("CLINICIAN_GRANT_SECTION_DETAILS") {
                    TextField("CLINICIAN_GRANT_NAME_PLACEHOLDER", text: $name)
                    TextField("CLINICIAN_GRANT_ORG_PLACEHOLDER", text: $organization)
                }
                Section("CLINICIAN_GRANT_SECTION_ACCESS") {
                    Picker("CLINICIAN_GRANT_TIER_PICKER", selection: $selectedTier) {
                        ForEach(ClinicianUseCaseTier.allCases, id: \.self) { tier in
                            Text(String(format: String(localized: "CLINICIAN_GRANT_TIER_FORMAT"),
                                        tier.rawValue, tier.monthlyPrice)).tag(tier)
                        }
                    }
                    .pickerStyle(.inline)
                }
                Section("CLINICIAN_GRANT_SECTION_USE_CASES") {
                    ForEach(ConsentEngine.useCaseLibrary) { useCase in
                        Toggle(isOn: Binding(
                            get: { selectedUseCases.contains(useCase.id) },
                            set: { if $0 { selectedUseCases.insert(useCase.id) } else { selectedUseCases.remove(useCase.id) } }
                        )) {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(useCase.title).font(.subheadline)
                                Text(useCase.description).font(.caption).foregroundColor(.secondary)
                            }
                        }
                    }
                }
            }
            .navigationTitle("CLINICIAN_GRANT_TITLE")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("COMMON_CANCEL") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("CLINICIAN_GRANT_BUTTON") {
                        let grant = ClinicianConsentGrant(
                            id: UUID(), clinicianName: name, clinicianOrganization: organization,
                            tier: selectedTier, grantedAt: Date(), expiresAt: nil, isActive: true
                        )
                        consentStore.grantClinicianAccess(grant)
                        dismiss()
                    }
                    .disabled(name.isEmpty || organization.isEmpty)
                }
            }
        }
    }
}
