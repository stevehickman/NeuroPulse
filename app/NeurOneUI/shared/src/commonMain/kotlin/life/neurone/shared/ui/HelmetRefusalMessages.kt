package life.neurone.shared.ui

import androidx.compose.runtime.Composable
import life.neurone.core.models.HelmetRefusalCause
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

/**
 * What to say for each reason the helmet can refuse a protocol, or answer for other bytes (SWR-HV-08, REQ-UPG-01, SWR-UP-02). One
 * message for every cause, so a refusal never shows a key or nothing; a tier refusal never names a purchase (NP-PWRSRC-001 §6.3).
 */
@Composable
fun helmetRefusalMessages(): Map<HelmetRefusalCause, String> = mapOf(
        HelmetRefusalCause.TIER to stringResource(Res.string.protocol_menu_tier_refused),
        HelmetRefusalCause.HARDWARE_VERSION to stringResource(Res.string.protocol_menu_hardware_version_refused),
        HelmetRefusalCause.NOT_READY to stringResource(Res.string.protocol_menu_helmet_not_ready),
        HelmetRefusalCause.SIGNATURE to stringResource(Res.string.protocol_menu_refused_signature),
        HelmetRefusalCause.WRONG_DEVICE to stringResource(Res.string.protocol_menu_refused_wrong_device),
        HelmetRefusalCause.FORMAT_VERSION to stringResource(Res.string.protocol_menu_refused_format_version),
        HelmetRefusalCause.DAMAGED to stringResource(Res.string.protocol_menu_refused_damaged),
        HelmetRefusalCause.MALFORMED to stringResource(Res.string.protocol_menu_refused_malformed),
        HelmetRefusalCause.SESSION_ACTIVE to stringResource(Res.string.protocol_menu_refused_session_active),
        HelmetRefusalCause.BOOT_REFUSED to stringResource(Res.string.protocol_menu_refused_boot),
        HelmetRefusalCause.NO_LAYOUT to stringResource(Res.string.protocol_menu_refused_no_layout),
        HelmetRefusalCause.PHOTIC_ZONE to stringResource(Res.string.protocol_menu_refused_photic_zone),
        HelmetRefusalCause.BAD_ADDRESS to stringResource(Res.string.protocol_menu_refused_bad_address),
        HelmetRefusalCause.TOO_MANY_COMMANDS to stringResource(Res.string.protocol_menu_refused_too_many_commands),
        HelmetRefusalCause.OTHER to stringResource(Res.string.protocol_menu_refused_generic),
        HelmetRefusalCause.UNMATCHED to stringResource(Res.string.protocol_menu_upload_unmatched),
)
