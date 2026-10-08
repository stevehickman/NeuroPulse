package life.neurone.shared.ui

import androidx.compose.material3.AlertDialog
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import kotlinx.coroutines.launch
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.NPZoneCaution
import life.neurone.shared.AppServices
import life.neurone.shared.resources.*
import life.neurone.shared.session.ProtocolUploader
import org.jetbrains.compose.resources.stringResource

/**
 * Session tab: the live session view, or the protocol menu when the user taps "Browse Protocols". Mirrors iOS,
 * where ProtocolMenuView is presented from SessionView. Everything the screens read comes from [AppServices]
 * (the hub link and the protocol library), and the two platform actions, asking the operating system for
 * Bluetooth access and signing a protocol, go through `PlatformServices`.
 */
@Composable
fun SessionTab(services: AppServices, modifier: Modifier = Modifier) {
    var showMenu by remember { mutableStateOf(false) }
    val gatt = services.gattManager
    val uploader = services.protocolUploader
    val consentGranted = services.keyValueStore.getBoolean(OnboardingKeys.BIPA_ACCEPTED)
    val eegMessage = stringResource(Res.string.session_eeg_unavailable_body)
    val snackbar = LocalSnackbarHost.current
    val scope = rememberCoroutineScope()

    // Live BLE state from the app-scoped GATT manager.
    val connectionState by gatt.connectionState.collectAsState()
    val session by gatt.session.collectAsState()
    val cervicalPadAlert by gatt.cervicalPadAlert.collectAsState()
    val cervicalFaultStatus by gatt.cervicalFaultStatus.collectAsState()
    val unreadCervicalFaults by gatt.unacknowledgedCervicalFaults.collectAsState()
    val cardiacWarningAcknowledged by gatt.cardiacWarningAcknowledged.collectAsState()
    val pairingRequired by gatt.pairingRequired.collectAsState()
    // canForgetHub reads storage, so re-read it after a forget.
    var hubForgotten by remember { mutableStateOf(0) }
    val canForgetHub = hubForgotten >= 0 && gatt.canForgetHub

    // A cervical protocol held back until the wearer confirms the selected profile is theirs.
    var awaitingDifferentPerson by remember { mutableStateOf<NPProtocolEntry.Single?>(null) }
    // A protocol held back until its author acknowledges each zone-model caution (docs/reference/safety-zones.md).
    var awaitingCautions by remember { mutableStateOf<Pair<NPProtocolEntry.Single, List<NPZoneCaution>>?>(null) }

    // On Android 12+ this asks for BLUETOOTH_SCAN/CONNECT and, on grant, starts scanning; elsewhere it refreshes.
    val requestConnect = services.platform.rememberBleConnectAction()

    val sentToHub = stringResource(Res.string.protocol_menu_protocol_sent_to_hub)
    val cervicalBlocked = stringResource(Res.string.upload_cervical_blocked)
    val differentPersonBody = stringResource(Res.string.cvns_different_person_body)
    val compositeUnsupported = stringResource(Res.string.and_ui_composite_protocol_upload_is_not_yet_support)

    fun uploadAndReport(entry: NPProtocolEntry, acknowledgedCautions: List<String> = emptyList()) {
        // A protocol in the caution zone runs only after its author acknowledges each caution
        // (docs/reference/safety-zones.md). The dialog calls back here with the ids, for this run only.
        if (entry is NPProtocolEntry.Single && acknowledgedCautions.isEmpty()) {
            val cautions = uploader.cautions(entry.protocol)
            if (cautions.isNotEmpty()) {
                awaitingCautions = entry to cautions
                return
            }
        }
        // Compile → sign → chunk → upload (Mode 2). Composite upload is a follow-up.
        val result = when (entry) {
            is NPProtocolEntry.Single -> uploader.upload(entry.protocol, acknowledgedCautions = acknowledgedCautions)
            else -> ProtocolUploader.Result.Failure(compositeUnsupported)
        }
        if (result is ProtocolUploader.Result.DifferentPersonConfirmationRequired && entry is NPProtocolEntry.Single) {
            awaitingDifferentPerson = entry
            return
        }
        val message = when (result) {
            is ProtocolUploader.Result.Success -> sentToHub
            is ProtocolUploader.Result.Failure -> result.message
            is ProtocolUploader.Result.CervicalRestartBlocked -> cervicalBlocked
            is ProtocolUploader.Result.DifferentPersonConfirmationRequired -> differentPersonBody
        }
        scope.launch { snackbar.showSnackbar(message) }
        showMenu = false
    }

    awaitingCautions?.let { (held, cautions) ->
        CautionAcknowledgementDialog(
            protocolName = held.protocol.name,
            cautions = cautions,
            onAcknowledge = { ids ->
                awaitingCautions = null
                uploadAndReport(held, ids)
            },
            onCancel = { awaitingCautions = null },
        )
    }

    awaitingDifferentPerson?.let { held ->
        AlertDialog(
            onDismissRequest = { awaitingDifferentPerson = null },
            title = { Text(stringResource(Res.string.cvns_different_person_title)) },
            text = { Text(stringResource(Res.string.cvns_different_person_body)) },
            confirmButton = {
                TextButton(onClick = {
                    awaitingDifferentPerson = null
                    uploader.confirmDifferentPerson()
                    uploadAndReport(held)
                }) { Text(stringResource(Res.string.cvns_different_person_confirm)) }
            },
            dismissButton = {
                TextButton(onClick = { awaitingDifferentPerson = null }) {
                    Text(stringResource(Res.string.common_cancel))
                }
            },
        )
    }

    if (showMenu) {
        ProtocolMenuScreen(
            library = services.protocolLibrary,
            consentGranted = consentGranted,
            eegUnavailableMessage = eegMessage,
            limits = services.limitsStore.resolvedLimits,
            onSelect = { entry -> uploadAndReport(entry) },
            onBack = { showMenu = false },
            modifier = modifier,
        )
    } else {
        SessionScreen(
            connectionState = connectionState,
            pairingRequired = pairingRequired,
            onForgetHub = if (canForgetHub) ({ gatt.forgetHub(); hubForgotten++ }) else null,
            session = session,
            onConnect = requestConnect,
            onChooseProtocol = { showMenu = true },
            onStop = { gatt.requestSessionStop() },
            modifier = modifier,
            cervicalPadAlert = cervicalPadAlert,
            onAcknowledgeCervicalPadAlert = { gatt.acknowledgeCervicalPadAlert() },
            cervicalFaultStatus = cervicalFaultStatus,
            unacknowledgedCervicalFaults = unreadCervicalFaults,
            onAcknowledgeCervicalFaults = { gatt.acknowledgeCervicalFaults() },
            cardiacWarningAcknowledged = cardiacWarningAcknowledged,
            onAcknowledgeCardiacWarning = { gatt.acknowledgeCardiacWarning() },
            onConfirmCervicalResume = { gatt.sendCervicalReenableConfirm() },
        )
    }
}
