import SwiftUI

// Consent management dashboard — CLAUDE.md §6.
// Surfaces: active clinician grants · research consent status ·
// study participation audit trail · pending study invitations.

struct ConsentDashboardView: View {

    @EnvironmentObject private var consentStore: ConsentStore
    @State private var showOnboarding = false
    @State private var showNewClinicianGrant = false
    @State private var selectedInvitation: StudyInvitation?
    @State private var grantPendingRevoke: ClinicianConsentGrant?
    @State private var participationPendingWithdraw: StudyParticipationRecord?

    var body: some View {
        NavigationStack {
            List {
                clinicianSection
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
                                grant.clinicianName, grant.clinicianOrganisation))
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

    private var researchSection: some View {
        Section {
            researchConsentSummaryRow
            ForEach(ResearchCategory.allCases, id: \.self) { category in
                let granted = consentStore.researchConsent.categoryConsents[category] ?? false
                HStack {
                    Text(category.rawValue).font(.subheadline)
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

    private var pendingInvitationsSection: some View {
        Section("DASHBOARD_SECTION_PENDING") {
            if consentStore.pendingInvitations.filter({ $0.decision == nil }).isEmpty {
                Text("DASHBOARD_NO_INVITATIONS")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            } else {
                ForEach(consentStore.pendingInvitations.filter { $0.hasNoDecision }) { invitation in
                    Button {
                        selectedInvitation = invitation
                    } label: {
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(invitation.studyTitle).font(.subheadline.bold())
                                Text(invitation.studyID).font(.caption).foregroundColor(.secondary)
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
                    Text(grant.clinicianOrganisation).font(.caption).foregroundColor(.secondary)
                    Text(grant.tier.monthlyPrice).font(.caption2).foregroundColor(.secondary)
                }
                Spacer()
                Button("DASHBOARD_REVOKE_BUTTON", role: .destructive, action: onRevoke)
                    .font(.caption)
                    .buttonStyle(.borderless)
            }
            // Granted elements (ISC-75)
            Text(String(format: String(localized: "DASHBOARD_ACCESS_FORMAT"),
                        grant.approvedElements.map(\.rawValue).sorted().joined(separator: ", ")))
                .font(.caption2)
                .foregroundColor(.secondary)
                .lineLimit(nil)
                .fixedSize(horizontal: false, vertical: true)
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

struct StudyInvitationView: View {
    let invitation: StudyInvitation
    @EnvironmentObject private var consentStore: ConsentStore
    @Environment(\.dismiss) private var dismiss
    @State private var showParticipateConfirmation = false   // ISC-79

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    Text(invitation.studyTitle).font(.title2.bold())
                    Text(String(format: String(localized: "INVITATION_STUDY_ID_FORMAT"),
                                invitation.studyID))
                        .font(.caption).foregroundColor(.secondary)

                    Divider()

                    Text("INVITATION_CAN_SEE_HEADING").font(.headline)
                    ForEach(Array(invitation.approvedElements), id: \.rawValue) { element in
                        Label(element.rawValue, systemImage: "checkmark.circle.fill")
                            .font(.subheadline)
                            .foregroundColor(.green)
                    }

                    Text("INVITATION_CANNOT_SEE_HEADING").font(.headline)
                    ForEach(invitation.cannotLearn, id: \.self) { item in
                        Label(item, systemImage: "xmark.circle.fill")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }

                    Divider()

                    VStack(alignment: .leading, spacing: 8) {
                        Label("INVITATION_IMPORTANT_LABEL", systemImage: "info.circle.fill")
                            .font(.subheadline.bold()).foregroundColor(.orange)
                        Text(invitation.irreversibilityNotice)
                            .font(.caption).foregroundColor(.secondary)
                    }
                    .padding(12)
                    .background(Color.orange.opacity(0.08))
                    .clipShape(RoundedRectangle(cornerRadius: 10))
                }
                .padding()
            }
            .navigationTitle("INVITATION_TITLE")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("COMMON_CANCEL") { dismiss() }
                }
            }
            .safeAreaInset(edge: .bottom) {
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
                .padding()
                .background(.regularMaterial)
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
                Text(invitation.irreversibilityNotice)
            }
        }
    }
}

struct NewClinicianGrantView: View {
    @EnvironmentObject private var consentStore: ConsentStore
    @Environment(\.dismiss) private var dismiss
    @State private var name = ""
    @State private var organisation = ""
    @State private var selectedTier: ClinicianUseCaseTier = .monitor
    @State private var selectedUseCases = Set<String>()

    var body: some View {
        NavigationStack {
            Form {
                Section("CLINICIAN_GRANT_SECTION_DETAILS") {
                    TextField("CLINICIAN_GRANT_NAME_PLACEHOLDER", text: $name)
                    TextField("CLINICIAN_GRANT_ORG_PLACEHOLDER", text: $organisation)
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
                            id: UUID(), clinicianName: name, clinicianOrganisation: organisation,
                            tier: selectedTier, grantedAt: Date(), expiresAt: nil, isActive: true
                        )
                        consentStore.grantClinicianAccess(grant)
                        dismiss()
                    }
                    .disabled(name.isEmpty || organisation.isEmpty)
                }
            }
        }
    }
}
