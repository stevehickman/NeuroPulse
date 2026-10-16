package life.neurone.shared.ui

import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource

// The words of a migration screen, found from the key the core names (SWR-MG-03, issue #569 package 6n and 6o). The core says
// which locale key words a reason, a rejected secret, a storage-check result or an item (`messageKeys`); a Compose resource is
// found by a static reference and not by a name looked up at run time, so this is the one table from a key to its resource. A
// test holds it to the core's table: a key the core names that is missing here fails.

/** The resource that words [key], or null for a key this table does not know. */
internal fun migrationText(key: String): StringResource? = when (key) {
        "MIGRATION_ITEM_CERVICAL_FAULT_LEDGER" -> Res.string.migration_item_cervical_fault_ledger
        "MIGRATION_ITEM_CHARACTERISATION_ENROLMENT" -> Res.string.migration_item_characterisation_enrolment
        "MIGRATION_ITEM_CONSENT_GRANTS" -> Res.string.migration_item_consent_grants
        "MIGRATION_ITEM_CONSUMABLE_COUNTERS" -> Res.string.migration_item_consumable_counters
        "MIGRATION_ITEM_LAST_PAIRED_HELMET_ID" -> Res.string.migration_item_last_paired_helmet_id
        "MIGRATION_ITEM_LAUNCH_COUNT" -> Res.string.migration_item_launch_count
        "MIGRATION_ITEM_LIMITS_PROFILES" -> Res.string.migration_item_limits_profiles
        "MIGRATION_ITEM_ONBOARDING_FLAGS" -> Res.string.migration_item_onboarding_flags
        "MIGRATION_ITEM_PROTOCOLS" -> Res.string.migration_item_protocols
        "MIGRATION_ITEM_RESEARCH_DEVICE_TOKEN" -> Res.string.migration_item_research_device_token
        "MIGRATION_ITEM_SESSION_HISTORY" -> Res.string.migration_item_session_history
        "MIGRATION_ITEM_WARRANTY_CONSENT_FLAG" -> Res.string.migration_item_warranty_consent_flag
        "MIGRATION_ITEM_WITHDRAWAL_STATE" -> Res.string.migration_item_withdrawal_state
        "MIGRATION_PROBE_BACKUP_ONLY" -> Res.string.migration_probe_backup_only
        "MIGRATION_PROBE_INSUFFICIENT" -> Res.string.migration_probe_insufficient
        "MIGRATION_PROBE_SUFFICIENT" -> Res.string.migration_probe_sufficient
        "MIGRATION_REASON_AUTHENTICATION_FAILED" -> Res.string.migration_reason_authentication_failed
        "MIGRATION_REASON_CLAIMED" -> Res.string.migration_reason_claimed
        "MIGRATION_REASON_CLAIM_NOT_OWNED" -> Res.string.migration_reason_claim_not_owned
        "MIGRATION_REASON_LINK_FAILED" -> Res.string.migration_reason_link_failed
        "MIGRATION_REASON_MALFORMED" -> Res.string.migration_reason_malformed
        "MIGRATION_REASON_MALFORMED_FRAME" -> Res.string.migration_reason_malformed_frame
        "MIGRATION_REASON_NEWER_VERSION" -> Res.string.migration_reason_newer_version
        "MIGRATION_REASON_NOT_AUTHENTICATED" -> Res.string.migration_reason_not_authenticated
        "MIGRATION_REASON_NO_ATOMIC_CLAIM" -> Res.string.migration_reason_no_atomic_claim
        "MIGRATION_REASON_NO_AUTHENTICATED_PAIRING" -> Res.string.migration_reason_no_authenticated_pairing
        "MIGRATION_REASON_NO_BUNDLE" -> Res.string.migration_reason_no_bundle
        "MIGRATION_REASON_NO_CONFIRMATION" -> Res.string.migration_reason_no_confirmation
        "MIGRATION_REASON_OTHER_BUNDLE" -> Res.string.migration_reason_other_bundle
        "MIGRATION_REASON_ROUTE_NOT_OFFERED" -> Res.string.migration_reason_route_not_offered
        "MIGRATION_REASON_ROUTE_UNAVAILABLE" -> Res.string.migration_reason_route_unavailable
        "MIGRATION_REASON_STEP_FAILED" -> Res.string.migration_reason_step_failed
        "MIGRATION_REASON_UNKNOWN_KIND" -> Res.string.migration_reason_unknown_kind
        "MIGRATION_REASON_VERIFY_FAILED" -> Res.string.migration_reason_verify_failed
        "MIGRATION_SECRET_BAD_CHECKSUM" -> Res.string.migration_secret_bad_checksum
        "MIGRATION_SECRET_MALFORMED" -> Res.string.migration_secret_malformed
        "MIGRATION_SECRET_TOO_SHORT" -> Res.string.migration_secret_too_short
        else -> null
    }
