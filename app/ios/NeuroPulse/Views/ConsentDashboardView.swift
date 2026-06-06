// TODO(localisation): strings below should use NSLocalizedString — see en.lproj/Localizable.strings
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
            .navigationTitle("Privacy & Consent")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Menu {
                        Button("Research Preferences") { showOnboarding = true }
                        NavigationLink("Research Portal") {
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
                "Revoke clinician access?",
                isPresented: Binding(
                    get: { grantPendingRevoke != nil },
                    set: { if !$0 { grantPendingRevoke = nil } }
                ),
                titleVisibility: .visible
            ) {
                if let grant = grantPendingRevoke {
                    Button("Revoke access for \(grant.clinicianName)", role: .destructive) {
                        consentStore.revokeClinicianAccess(grantID: grant.id)
                        grantPendingRevoke = nil
                    }
                }
                Button("Cancel", role: .cancel) { grantPendingRevoke = nil }
            } message: {
                if let grant = grantPendingRevoke {
                    Text("\(grant.clinicianName) at \(grant.clinicianOrganisation) will immediately lose access to your data.")
                }
            }
            // ISC-77: confirmation before withdrawing from a study
            .confirmationDialog(
                "Withdraw from study?",
                isPresented: Binding(
                    get: { participationPendingWithdraw != nil },
                    set: { if !$0 { participationPendingWithdraw = nil } }
                ),
                titleVisibility: .visible
            ) {
                if let record = participationPendingWithdraw {
                    Button("Withdraw from \(record.studyID)", role: .destructive) {
                        consentStore.withdrawFromStudy(studyID: record.studyID)
                        participationPendingWithdraw = nil
                    }
                }
                Button("Cancel", role: .cancel) { participationPendingWithdraw = nil }
            } message: {
                Text("No further data will flow to this study. Data already included in published results cannot be individually removed.")
            }
        }
    }

    // MARK: - Sections

    private var clinicianSection: some View {
        Section {
            if consentStore.clinicianGrants.isEmpty {
                Text("No clinicians have access to your data.")
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
                Label("Add Clinician Access", systemImage: "plus")
            }
        } header: {
            Text("Clinician Access")
        } footer: {
            Text("Clinicians access only the data elements required for their specific use case. You can revoke access at any time.")
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
            Text("Research Consent")
        }
    }

    private var researchConsentSummaryRow: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(consentStore.researchConsent.blanketConsentGranted ? "Pre-approved" : "Per-category")
                    .font(.subheadline.bold())
                Text(consentStore.researchConsent.contactConsentGranted
                     ? "Contact: \(redacted(consentStore.researchConsent.contactMethod))"
                     : "No research contact")
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
        Section("Study Participation History") {
            if consentStore.studyParticipations.isEmpty {
                Text("You have not participated in any studies yet.")
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
        Section("Pending Invitations") {
            if consentStore.pendingInvitations.filter({ $0.decision == nil }).isEmpty {
                Text("No pending study invitations.")
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
                Button("Revoke", role: .destructive, action: onRevoke)
                    .font(.caption)
                    .buttonStyle(.borderless)
            }
            // Granted elements (ISC-75)
            Text("Access: \(grant.approvedElements.map(\.rawValue).sorted().joined(separator: ", "))")
                .font(.caption2)
                .foregroundColor(.secondary)
                .lineLimit(nil)
                .fixedSize(horizontal: false, vertical: true)
            HStack {
                Label("Granted \(grant.grantedAt.formatted(.dateTime.month().day().year()))",
                      systemImage: "checkmark.shield")
                    .font(.caption2).foregroundColor(.green)
                Spacer()
                if let exp = grant.expiresAt {
                    Text("Expires \(exp.formatted(.dateTime.month().day()))").font(.caption2).foregroundColor(.orange)
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
                Text("Data shared: \(record.transmittedAt.formatted(.dateTime.month().day().year()))")
                    .font(.caption).foregroundColor(.secondary)
                if !record.isActive {
                    Label("Withdrawn", systemImage: "xmark.circle")
                        .font(.caption2).foregroundColor(.orange)
                }
            }
            Spacer()
            if record.isActive {
                Button("Withdraw", role: .destructive, action: onWithdraw)
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
                    Text("Study ID: \(invitation.studyID)").font(.caption).foregroundColor(.secondary)

                    Divider()

                    Text("What the researchers CAN see").font(.headline)
                    ForEach(Array(invitation.approvedElements), id: \.rawValue) { element in
                        Label(element.rawValue, systemImage: "checkmark.circle.fill")
                            .font(.subheadline)
                            .foregroundColor(.green)
                    }

                    Text("What they CANNOT see").font(.headline)
                    ForEach(invitation.cannotLearn, id: \.self) { item in
                        Label(item, systemImage: "xmark.circle.fill")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }

                    Divider()

                    VStack(alignment: .leading, spacing: 8) {
                        Label("Important", systemImage: "info.circle.fill")
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
            .navigationTitle("Study Invitation")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { dismiss() }
                }
            }
            .safeAreaInset(edge: .bottom) {
                HStack(spacing: 12) {
                    Button("Decline") {
                        consentStore.declineInvitation(studyID: invitation.studyID)
                        dismiss()
                    }
                    .buttonStyle(.bordered)
                    .foregroundColor(.red)

                    // ISC-79: confirmation must include the irreversibility notice.
                    Button("Participate") {
                        showParticipateConfirmation = true
                    }
                    .buttonStyle(.borderedProminent)
                    .frame(maxWidth: .infinity)
                }
                .padding()
                .background(.regularMaterial)
            }
            .confirmationDialog(
                "Confirm participation",
                isPresented: $showParticipateConfirmation,
                titleVisibility: .visible
            ) {
                Button("Confirm — participate in study") {
                    consentStore.acceptInvitation(studyID: invitation.studyID)
                    dismiss()
                }
                Button("Cancel", role: .cancel) {}
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
                Section("Clinician Details") {
                    TextField("Name", text: $name)
                    TextField("Organisation", text: $organisation)
                }
                Section("Access Level") {
                    Picker("Tier", selection: $selectedTier) {
                        ForEach(ClinicianUseCaseTier.allCases, id: \.self) { tier in
                            Text("\(tier.rawValue) — \(tier.monthlyPrice)").tag(tier)
                        }
                    }
                    .pickerStyle(.inline)
                }
                Section("Use Cases") {
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
            .navigationTitle("Add Clinician")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Grant") {
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
