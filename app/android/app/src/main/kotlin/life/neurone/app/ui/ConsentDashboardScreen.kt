package life.neurone.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Checkbox
import androidx.compose.material3.Divider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import life.neurone.app.NeurOneApplication
import life.neurone.app.R
import life.neurone.core.consent.ConsentStore
import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianUseCaseTier
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.StudyInvitation
import life.neurone.core.models.StudyParticipationRecord
import life.neurone.core.models.UHDRElement
import java.time.LocalDate
import java.util.UUID

// Port of iOS ConsentDashboardView (app/ios/NeurOne/Views/ConsentDashboardView.swift) —
// the consent management surface of CLAUDE.md §6: active clinician grants · research consent
// status · study participation audit trail · pending study invitations.
//
// This screen closes OI-CONSENT-01. Before it, Android's only consent UI was
// ConsentOnboardingScreen plus a blanket-consent toggle, so grantClinicianAccess,
// revokeClinicianAccess, acceptInvitation, declineInvitation and withdrawFromStudy were
// defined and unit-tested on Android and callable by nothing — the CLAUDE.md Rev 37 shape
// (correct, tested, unreachable) at five methods instead of one. The five per-platform
// waivers in scripts/check-consent-reachability.ts came out when this screen landed; that
// gate fails if a waiver outlives the gap it records, so they could not have been left.
//
// TWO DELIBERATE DIFFERENCES FROM iOS, both noted rather than silently absorbed:
//  1. L3 posture is a switch here, not iOS's one-way "Stop pre-approving studies" button.
//     Android could already re-grant blanket consent from this tab and losing that would be
//     a regression; turning it OFF now goes through the same confirmation iOS shows, and
//     through the same named withdrawBlanketResearchConsent() path (§6.2.5), so the
//     analytics teardown is explicit at the call site as well as guarded at the store.
//  2. The grant form shows the tier's derived UHDR elements (can-see / cannot-see, §6.1
//     minimum necessary) where iOS shows a use-case picker. iOS discards that picker's
//     selection when it builds the grant — approvedElements comes from the tier either way
//     — so porting the picker would port a control that decides nothing.
//
// Consent element, category and tier labels come from :core's `displayName`, which is
// English-only: :core is a pure-JVM module by design (no Android plugin, ISC-2..4) so it
// cannot name R.string. That is the standing app/android/core/ backlog item in
// check-locale-strings.ts PENDING_PATHS, and it is how every other Android consent screen
// renders these same enums today.

private sealed interface DashboardRoute {
    data object Dashboard : DashboardRoute
    data object ResearchPreferences : DashboardRoute
    data object Portal : DashboardRoute
    data object NewClinicianGrant : DashboardRoute
    data class Invitation(val invitation: StudyInvitation) : DashboardRoute
}

@Composable
fun ConsentDashboardScreen(app: NeurOneApplication, modifier: Modifier = Modifier) {
    val store = app.consentStore
    var route by remember { mutableStateOf<DashboardRoute>(DashboardRoute.Dashboard) }
    // ConsentStore is not observable (parity with iOS's @Published is a follow-up); bump to
    // re-read it after a mutation, as ResearchPortalScreen does.
    var version by remember { mutableIntStateOf(0) }

    when (val current = route) {
        is DashboardRoute.Portal -> ResearchPortalScreen(
            store = app.researchSuggestionStore,
            onBack = { route = DashboardRoute.Dashboard },
            modifier = modifier,
        )

        is DashboardRoute.ResearchPreferences -> ConsentOnboardingScreen(
            store = store,
            onComplete = {
                version++
                route = DashboardRoute.Dashboard
            },
            modifier = modifier,
        )

        is DashboardRoute.NewClinicianGrant -> NewClinicianGrantScreen(
            onGrant = { grant ->
                store.grantClinicianAccess(grant)
                version++
                route = DashboardRoute.Dashboard
            },
            onCancel = { route = DashboardRoute.Dashboard },
            modifier = modifier,
        )

        is DashboardRoute.Invitation -> StudyInvitationScreen(
            invitation = current.invitation,
            onAccept = {
                store.acceptInvitation(current.invitation.studyId)
                version++
                route = DashboardRoute.Dashboard
            },
            onDecline = {
                store.declineInvitation(current.invitation.studyId)
                version++
                route = DashboardRoute.Dashboard
            },
            onBack = { route = DashboardRoute.Dashboard },
            modifier = modifier,
        )

        is DashboardRoute.Dashboard -> DashboardContent(
            store = store,
            version = version,
            onChanged = { version++ },
            onNavigate = { route = it },
            modifier = modifier,
        )
    }
}

@Composable
private fun DashboardContent(
    store: ConsentStore,
    version: Int,
    onChanged: () -> Unit,
    onNavigate: (DashboardRoute) -> Unit,
    modifier: Modifier = Modifier,
) {
    // `version` is read through these remembers, so a mutation recomposes the whole screen.
    val grants = remember(version) { store.clinicianGrants }
    val research = remember(version) { store.researchConsent }
    val participations = remember(version) { store.studyParticipations }
    val invitations = remember(version) { store.pendingInvitations.filter { it.hasNoDecision } }

    var grantPendingRevoke by remember { mutableStateOf<ClinicianConsentGrant?>(null) }
    var participationPendingWithdraw by remember { mutableStateOf<StudyParticipationRecord?>(null) }
    var showBlanketWithdrawConfirmation by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
    ) {
        Text(stringResource(R.string.dashboard_title), style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(20.dp))

        // ── Clinician access (§6.1) ──────────────────────────────────────
        SectionHeader(stringResource(R.string.dashboard_section_clinicians))
        if (grants.isEmpty()) {
            Text(
                stringResource(R.string.dashboard_no_clinicians),
                style = MaterialTheme.typography.bodyMedium,
            )
        } else {
            for (grant in grants) {
                ClinicianGrantCard(grant = grant, onRevoke = { grantPendingRevoke = grant })
            }
        }
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = { onNavigate(DashboardRoute.NewClinicianGrant) }) {
            Text(stringResource(R.string.dashboard_add_clinician))
        }
        Spacer(Modifier.height(8.dp))
        Text(
            stringResource(R.string.dashboard_clinicians_footer),
            style = MaterialTheme.typography.bodySmall,
        )

        Spacer(Modifier.height(20.dp))
        Divider()
        Spacer(Modifier.height(20.dp))

        // ── Research consent (§6.2) ──────────────────────────────────────
        SectionHeader(stringResource(R.string.dashboard_section_research))
        Text(
            if (research.blanketConsentGranted) stringResource(R.string.dashboard_blanket_approved)
            else stringResource(R.string.dashboard_per_category),
            style = MaterialTheme.typography.titleSmall,
        )
        Text(
            if (research.contactConsentGranted) {
                stringResource(R.string.dashboard_contact_format, redactedContact(research.contactMethod))
            } else {
                stringResource(R.string.dashboard_no_contact)
            },
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(12.dp))

        // L3 posture is its own control, separate from the L2 category list below, because
        // they are separate axes (§6.2.2): scope versus ask-me-each-time.
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(stringResource(R.string.and_ui_blanket_research_consent), Modifier.weight(1f))
            Switch(
                checked = research.blanketConsentGranted,
                onCheckedChange = { on ->
                    if (on) {
                        store.updateResearchConsent(research.copy(blanketConsentGranted = true))
                        onChanged()
                    } else {
                        // Confirm first — the teardown it triggers is not obvious from the
                        // control (§6.0), so the dialog states it before anything happens.
                        showBlanketWithdrawConfirmation = true
                    }
                },
            )
        }
        if (!research.blanketConsentGranted) {
            Text(
                stringResource(R.string.dashboard_posture_asked),
                style = MaterialTheme.typography.bodySmall,
            )
        } else {
            // §6.2: L3 carries the irreversibility notice wherever its control is on.
            Spacer(Modifier.height(8.dp))
            Text(stringResource(R.string.consent_important_label), fontWeight = FontWeight.Medium)
            Text(
                stringResource(R.string.consent_irreversibility_notice),
                style = MaterialTheme.typography.bodySmall,
            )
        }

        Spacer(Modifier.height(12.dp))
        for (category in ResearchCategory.entries) {
            val granted = research.categoryConsents[category] ?: false
            // Read-only status, not an editing control: L2 scope is edited in Research
            // Preferences and committed through updateResearchConsent, which is what carries
            // the §6.2.5 transition guard. A second write path here would bypass it.
            Row(
                Modifier.fillMaxWidth().padding(vertical = 2.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(category.displayName, style = MaterialTheme.typography.bodyMedium)
                Checkbox(checked = granted, onCheckedChange = null, enabled = false)
            }
        }

        Spacer(Modifier.height(20.dp))
        Divider()
        Spacer(Modifier.height(20.dp))

        // ── Study participation audit trail (§5.3) ───────────────────────
        SectionHeader(stringResource(R.string.dashboard_section_study_history))
        if (participations.isEmpty()) {
            Text(
                stringResource(R.string.dashboard_no_studies),
                style = MaterialTheme.typography.bodyMedium,
            )
        } else {
            for (record in participations) {
                StudyParticipationCard(
                    record = record,
                    onWithdraw = { participationPendingWithdraw = record },
                )
            }
        }

        Spacer(Modifier.height(20.dp))
        Divider()
        Spacer(Modifier.height(20.dp))

        // ── Pending study invitations (§6.3) ─────────────────────────────
        SectionHeader(stringResource(R.string.dashboard_section_pending))
        if (invitations.isEmpty()) {
            Text(
                stringResource(R.string.dashboard_no_invitations),
                style = MaterialTheme.typography.bodyMedium,
            )
        } else {
            for (invitation in invitations) {
                Card(
                    Modifier.fillMaxWidth().padding(vertical = 4.dp),
                ) {
                    Column(Modifier.padding(16.dp)) {
                        Text(invitation.studyTitle, style = MaterialTheme.typography.titleMedium)
                        Text(
                            stringResource(R.string.invitation_study_id_format, invitation.studyId),
                            style = MaterialTheme.typography.bodySmall,
                        )
                        Spacer(Modifier.height(8.dp))
                        OutlinedButton(onClick = { onNavigate(DashboardRoute.Invitation(invitation)) }) {
                            Text(stringResource(R.string.invitation_title))
                        }
                    }
                }
            }
        }

        Spacer(Modifier.height(24.dp))

        // ── Research preferences + portal ────────────────────────────────
        Text(stringResource(R.string.portal_research_ideas), style = MaterialTheme.typography.titleMedium)
        Text(
            stringResource(R.string.and_ui_suggest_studies_vote_and_register_interest_i),
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(8.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { onNavigate(DashboardRoute.ResearchPreferences) }) {
                Text(stringResource(R.string.dashboard_research_prefs_button))
            }
            OutlinedButton(onClick = { onNavigate(DashboardRoute.Portal) }) {
                Text(stringResource(R.string.and_ui_open_research_portal))
            }
        }
    }

    // ── Confirmations ────────────────────────────────────────────────────

    grantPendingRevoke?.let { grant ->
        AlertDialog(
            onDismissRequest = { grantPendingRevoke = null },
            title = { Text(stringResource(R.string.dashboard_revoke_dialog_title)) },
            text = {
                Text(
                    stringResource(
                        R.string.dashboard_revoke_message_format,
                        grant.clinicianName,
                        grant.clinicianOrganization,
                    ),
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    store.revokeClinicianAccess(grant.id)
                    grantPendingRevoke = null
                    onChanged()
                }) {
                    Text(stringResource(R.string.dashboard_revoke_access_format, grant.clinicianName))
                }
            },
            dismissButton = {
                TextButton(onClick = { grantPendingRevoke = null }) {
                    Text(stringResource(R.string.common_cancel))
                }
            },
        )
    }

    participationPendingWithdraw?.let { record ->
        AlertDialog(
            onDismissRequest = { participationPendingWithdraw = null },
            title = { Text(stringResource(R.string.dashboard_withdraw_dialog_title)) },
            text = { Text(stringResource(R.string.dashboard_withdraw_dialog_message)) },
            confirmButton = {
                TextButton(onClick = {
                    store.withdrawFromStudy(record.studyId)
                    participationPendingWithdraw = null
                    onChanged()
                }) {
                    Text(stringResource(R.string.dashboard_withdraw_from_format, record.studyId))
                }
            },
            dismissButton = {
                TextButton(onClick = { participationPendingWithdraw = null }) {
                    Text(stringResource(R.string.common_cancel))
                }
            },
        )
    }

    if (showBlanketWithdrawConfirmation) {
        AlertDialog(
            onDismissRequest = { showBlanketWithdrawConfirmation = false },
            title = { Text(stringResource(R.string.dashboard_stop_preapproving_dialog_title)) },
            text = { Text(stringResource(R.string.dashboard_stop_preapproving_message)) },
            confirmButton = {
                TextButton(onClick = {
                    // The named path, not an edit-and-commit: the §6.0 analytics teardown is
                    // explicit at the call site as well as guarded at the store's ingestion
                    // point (§6.2.5).
                    store.withdrawBlanketResearchConsent()
                    showBlanketWithdrawConfirmation = false
                    onChanged()
                }) { Text(stringResource(R.string.dashboard_stop_preapproving_confirm)) }
            },
            dismissButton = {
                TextButton(onClick = { showBlanketWithdrawConfirmation = false }) {
                    Text(stringResource(R.string.common_cancel))
                }
            },
        )
    }
}

@Composable
private fun SectionHeader(text: String) {
    Text(text, style = MaterialTheme.typography.titleMedium)
    Spacer(Modifier.height(8.dp))
}

@Composable
private fun ClinicianGrantCard(grant: ClinicianConsentGrant, onRevoke: () -> Unit) {
    Card(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Column(Modifier.padding(16.dp)) {
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(Modifier.weight(1f)) {
                    Text(grant.clinicianName, style = MaterialTheme.typography.titleSmall)
                    Text(grant.clinicianOrganization, style = MaterialTheme.typography.bodySmall)
                    Text(grant.tier.monthlyPrice, style = MaterialTheme.typography.labelSmall)
                }
                TextButton(onClick = onRevoke) {
                    Text(stringResource(R.string.dashboard_revoke_button))
                }
            }
            // What this grant actually reaches — the §6.1 minimum-necessary set for its tier.
            Text(
                stringResource(R.string.dashboard_access_format, elementList(grant.approvedElements)),
                style = MaterialTheme.typography.labelSmall,
            )
            Text(
                stringResource(R.string.dashboard_granted_format, grant.grantedAtDay),
                style = MaterialTheme.typography.labelSmall,
            )
            grant.expiresAtDay?.let { expiry ->
                Text(
                    stringResource(R.string.dashboard_expires_format, expiry),
                    style = MaterialTheme.typography.labelSmall,
                )
            }
        }
    }
}

@Composable
private fun StudyParticipationCard(
    record: StudyParticipationRecord,
    onWithdraw: () -> Unit,
) {
    Card(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Row(
            Modifier.fillMaxWidth().padding(16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(Modifier.weight(1f)) {
                Text(record.studyId, style = MaterialTheme.typography.titleSmall)
                Text(
                    stringResource(R.string.dashboard_data_shared_format, record.transmittedAtDay),
                    style = MaterialTheme.typography.bodySmall,
                )
                if (!record.isActive) {
                    Text(
                        stringResource(R.string.dashboard_withdrawn_label),
                        style = MaterialTheme.typography.labelSmall,
                    )
                }
            }
            if (record.isActive) {
                TextButton(onClick = onWithdraw) {
                    Text(stringResource(R.string.dashboard_withdraw_button))
                }
            }
        }
    }
}

/**
 * Study invitation detail — port of iOS StudyInvitationView.
 *
 * Both halves of §6.3's per-project decision are here: what the researchers can see, what
 * they cannot, and the irreversibility notice the invitation itself carries. Accepting goes
 * through a confirmation that repeats that notice (iOS ISC-79); declining does not, because
 * declining is the reversible direction.
 */
@Composable
private fun StudyInvitationScreen(
    invitation: StudyInvitation,
    onAccept: () -> Unit,
    onDecline: () -> Unit,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var showParticipateConfirmation by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
    ) {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onBack) { Text(stringResource(R.string.consent_back_button)) }
            Text(stringResource(R.string.invitation_title), style = MaterialTheme.typography.titleMedium)
        }
        Spacer(Modifier.height(12.dp))

        Text(invitation.studyTitle, style = MaterialTheme.typography.headlineSmall)
        Text(
            stringResource(R.string.invitation_study_id_format, invitation.studyId),
            style = MaterialTheme.typography.bodySmall,
        )

        Spacer(Modifier.height(16.dp))
        Divider()
        Spacer(Modifier.height(16.dp))

        Text(stringResource(R.string.invitation_can_see_heading), fontWeight = FontWeight.Medium)
        Text(elementList(invitation.approvedElements), style = MaterialTheme.typography.bodySmall)

        Spacer(Modifier.height(12.dp))
        Text(stringResource(R.string.invitation_cannot_see_heading), fontWeight = FontWeight.Medium)
        for (item in invitation.cannotLearn) {
            Text(item, style = MaterialTheme.typography.bodySmall)
        }

        Spacer(Modifier.height(16.dp))
        Text(stringResource(R.string.invitation_important_label), fontWeight = FontWeight.Medium)
        Text(invitation.irreversibilityNotice, style = MaterialTheme.typography.bodySmall)

        Spacer(Modifier.height(24.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = onDecline) {
                Text(stringResource(R.string.invitation_decline_button))
            }
            Button(onClick = { showParticipateConfirmation = true }) {
                Text(stringResource(R.string.invitation_participate_button))
            }
        }
    }

    if (showParticipateConfirmation) {
        AlertDialog(
            onDismissRequest = { showParticipateConfirmation = false },
            title = { Text(stringResource(R.string.invitation_confirm_dialog_title)) },
            text = { Text(invitation.irreversibilityNotice) },
            confirmButton = {
                TextButton(onClick = {
                    showParticipateConfirmation = false
                    onAccept()
                }) { Text(stringResource(R.string.invitation_confirm_button)) }
            },
            dismissButton = {
                TextButton(onClick = { showParticipateConfirmation = false }) {
                    Text(stringResource(R.string.common_cancel))
                }
            },
        )
    }
}

/**
 * New clinician grant — port of iOS NewClinicianGrantView.
 *
 * §6.1's principle is that the user consents to a use case and the system derives the
 * minimum necessary UHDR elements, so the tier's derived can-see / cannot-see lists are
 * shown before the grant is made rather than left to be discovered on the dashboard.
 */
@Composable
private fun NewClinicianGrantScreen(
    onGrant: (ClinicianConsentGrant) -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var name by remember { mutableStateOf("") }
    var organization by remember { mutableStateOf("") }
    var tier by remember { mutableStateOf(ClinicianUseCaseTier.MONITOR) }

    val approved = tier.uhdrElements
    val withheld = UHDRElement.entries.filterNot { it in approved }.toSet()

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(24.dp),
    ) {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onCancel) { Text(stringResource(R.string.common_cancel)) }
            Text(stringResource(R.string.clinician_grant_title), style = MaterialTheme.typography.titleMedium)
            Button(
                onClick = {
                    onGrant(
                        ClinicianConsentGrant(
                            id = UUID.randomUUID().toString(),
                            clinicianName = name.trim(),
                            clinicianOrganization = organization.trim(),
                            tier = tier,
                            grantedAtDay = LocalDate.now().toString(),
                            expiresAtDay = null,
                            isActive = true,
                        ),
                    )
                },
                enabled = name.isNotBlank() && organization.isNotBlank(),
            ) { Text(stringResource(R.string.clinician_grant_button)) }
        }
        Spacer(Modifier.height(16.dp))

        SectionHeader(stringResource(R.string.clinician_grant_section_details))
        OutlinedTextField(
            value = name,
            onValueChange = { name = it },
            label = { Text(stringResource(R.string.clinician_grant_name_placeholder)) },
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(Modifier.height(8.dp))
        OutlinedTextField(
            value = organization,
            onValueChange = { organization = it },
            label = { Text(stringResource(R.string.clinician_grant_org_placeholder)) },
            modifier = Modifier.fillMaxWidth(),
        )

        Spacer(Modifier.height(20.dp))
        SectionHeader(stringResource(R.string.clinician_grant_section_access))
        Text(stringResource(R.string.clinician_grant_tier_picker), style = MaterialTheme.typography.bodyMedium)
        for (candidate in ClinicianUseCaseTier.entries) {
            Row(
                Modifier.fillMaxWidth().padding(vertical = 2.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                RadioButton(selected = tier == candidate, onClick = { tier = candidate })
                Text(
                    stringResource(
                        R.string.clinician_grant_tier_format,
                        candidate.displayName,
                        candidate.monthlyPrice,
                    ),
                )
            }
        }

        Spacer(Modifier.height(16.dp))
        Text(stringResource(R.string.clinician_grant_can_see_heading), fontWeight = FontWeight.Medium)
        if (approved.isEmpty()) {
            // The Research tier's element set is IRB-defined per study descriptor, not
            // derivable from the tier — saying "nothing" would be wrong in both directions.
            Text(
                stringResource(R.string.clinician_grant_research_tier_note),
                style = MaterialTheme.typography.bodySmall,
            )
        } else {
            Text(elementList(approved), style = MaterialTheme.typography.bodySmall)
        }
        Spacer(Modifier.height(8.dp))
        Text(stringResource(R.string.clinician_grant_cannot_see_heading), fontWeight = FontWeight.Medium)
        Text(elementList(withheld), style = MaterialTheme.typography.bodySmall)
    }
}

/** Sorted, comma-joined element names — the same shape iOS's DASHBOARD_ACCESS_FORMAT takes. */
private fun elementList(elements: Set<UHDRElement>): String =
    elements.map { it.displayName }.sorted().joinToString(", ")

/**
 * Summary-row contact redaction, matching iOS: email → "•••@domain.com", anything else is
 * treated as a phone number → "••• ••• ••" plus its last two digits. The full value stays in
 * Research Preferences; the dashboard is the screen most likely to be read over a shoulder.
 */
private fun redactedContact(contact: String): String {
    val at = contact.indexOf('@')
    if (at >= 0) return "•••" + contact.substring(at)
    val digits = contact.filter { it.isDigit() }
    val suffix = if (digits.length >= 2) digits.takeLast(2) else digits
    return "••• ••• ••$suffix"
}
