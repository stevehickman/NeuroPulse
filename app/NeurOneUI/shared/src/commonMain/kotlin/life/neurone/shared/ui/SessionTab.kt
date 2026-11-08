package life.neurone.shared.ui

import androidx.compose.material3.AlertDialog
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
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
import life.neurone.core.models.HelmetRefusalCause
import life.neurone.core.models.SessionStatus
import life.neurone.core.session.SocketShortfall
import life.neurone.shared.AppServices
import life.neurone.shared.resources.*
import life.neurone.shared.session.ProgrammingOutcome
import life.neurone.shared.session.ProtocolUploader
import life.neurone.shared.session.shouldShowSessionDownload
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
    val progress by gatt.progress.collectAsState()
    val hardwareVersion by gatt.hardwareVersion.collectAsState()
    val cervicalPadAlert by gatt.cervicalPadAlert.collectAsState()
    val cervicalFaultStatus by gatt.cervicalFaultStatus.collectAsState()
    val unreadCervicalFaults by gatt.unacknowledgedCervicalFaults.collectAsState()
    val cardiacWarningAcknowledged by gatt.cardiacWarningAcknowledged.collectAsState()
    val pairingRequired by gatt.pairingRequired.collectAsState()

    val edf = services.edfDownload
    val fileTransfer = services.platform.rememberFileTransfer()
    // A new session clears the last download's outcome.
    LaunchedEffect(session.status == SessionStatus.RUNNING) { if (session.status == SessionStatus.RUNNING) edf?.reset() }
    // canForgetHub reads storage, so re-read it after a forget.
    var hubForgotten by remember { mutableStateOf(0) }
    val canForgetHub = hubForgotten >= 0 && gatt.canForgetHub

    // A cervical protocol held back until the wearer confirms the selected profile is theirs.
    var awaitingDifferentPerson by remember { mutableStateOf<NPProtocolEntry.Single?>(null) }
    // A protocol held back until its author acknowledges each zone-model caution (docs/reference/safety-zones.md).
    var awaitingCautions by remember { mutableStateOf<Pair<NPProtocolEntry.Single, List<NPZoneCaution>>?>(null) }

    // A protocol the fitted modules cannot run as authored, held back before anything is sent (OI-HWFIT-01).
    var notFitted by remember { mutableStateOf<List<SocketShortfall>?>(null) }

    // Mode 3: the next protocol chosen is programmed for use without a phone (the iOS `programAutonomous`).
    var programMode by remember { mutableStateOf(false) }

    // On Android 12+ this asks for BLUETOOTH_SCAN/CONNECT and, on grant, starts scanning; elsewhere it refreshes.
    val requestConnect = services.platform.rememberBleConnectAction()

    val sentToHub = stringResource(Res.string.protocol_menu_protocol_sent_to_hub)
    val storedOnHub = stringResource(Res.string.setup_protocol_stored_message)
    // What to say for each reason the helmet can refuse a protocol (SWR-HV-08, REQ-UPG-01, SWR-UP-02).
    val refusalText: Map<HelmetRefusalCause, String> = helmetRefusalMessages()
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
        // Compile → sign → chunk → upload (Mode 2). A composite compiles its layers to one descriptor (OI-NPPS-COMP-01).
        val result = when (entry) {
            is NPProtocolEntry.Single ->
                uploader.upload(entry.protocol, acknowledgedCautions = acknowledgedCautions, autonomous = programMode)
            // The Mode 3 (autonomous) program path does not take a composite (OI-NPPS-COMP-01); Mode 2 compiles its layers to one descriptor.
            is NPProtocolEntry.Composite ->
                if (programMode) {
                    ProtocolUploader.Result.Failure(compositeUnsupported)
                } else {
                    // A layer naming a protocol the library lacks leaves it out, and the compiler says which.
                    val members = services.protocolLibrary.compositeMembers(entry) ?: emptyList()
                    uploader.uploadComposite(entry.composite, members, acknowledgedCautions = acknowledgedCautions)
                }
            else -> ProtocolUploader.Result.Failure(compositeUnsupported)
        }
        if (result is ProtocolUploader.Result.DifferentPersonConfirmationRequired && entry is NPProtocolEntry.Single) {
            awaitingDifferentPerson = entry
            return
        }
        val programming = programMode
        val message = when (result) {
            is ProtocolUploader.Result.DoesNotFit -> {
                notFitted = result.shortfalls
                showMenu = false
                programMode = false
                return
            }
            is ProtocolUploader.Result.Success -> sentToHub
            is ProtocolUploader.Result.Failure -> result.message
            is ProtocolUploader.Result.CervicalRestartBlocked -> cervicalBlocked
            is ProtocolUploader.Result.DifferentPersonConfirmationRequired -> differentPersonBody
        }
        // SWR-UP-01/-02: the upload is not over when the last chunk is written. The helmet's report decides whether it
        // holds the protocol, and the library marks the family used for a confirmed or an unknown outcome, never for a
        // refusal before sending (SWR-UP-02). A refusal before sending (the compile failed, the cervical gate held it)
        // marks nothing.
        if (result is ProtocolUploader.Result.Success) {
            scope.launch {
                val verdict = uploader.verify(result.sentHash)
                services.protocolLibrary.recordUploadResult(
                    entry, result.sentHash, verdict.helmetStatus, verdict.helmetHash, verdict.helmetRefusal,
                )
                // The "sent" message above was true when it was shown; this says what the helmet then decided, whatever its
                // reason. A tier refusal never names a purchase (NP-PWRSRC-001 §6.3).
                // A final answer for other bytes says nothing about this upload: the helmet may hold something else.
                val cause = verdict.causeFor(result.sentHash)
                // Mode 3 says "stored" only when the helmet holds the protocol (SWR-UP-02, SWR-HV-08, as the iOS flow): every
                // refusal, and an answer for other bytes, is shown instead and nothing is reported stored.
                if (programming) {
                    snackbar.showSnackbar(
                        when (val outcome = ProgrammingOutcome.of(cause)) {
                            is ProgrammingOutcome.Refused -> refusalText.getValue(outcome.cause)
                            ProgrammingOutcome.Stored -> storedOnHub
                        },
                    )
                } else {
                    cause?.let { snackbar.showSnackbar(refusalText.getValue(it)) }
                }
            }
        }
        if (!(programming && result is ProtocolUploader.Result.Success)) scope.launch { snackbar.showSnackbar(message) }
        showMenu = false
        programMode = false
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

    notFitted?.let { NotFittedDialog(it, onDismiss = { notFitted = null }) }

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
            onBack = { showMenu = false; programMode = false },
            helmetHardwareVersion = hardwareVersion,
            fileTransfer = fileTransfer,
            modifier = modifier,
        )
    } else {
        SessionScreen(
            connectionState = connectionState,
            pairingRequired = pairingRequired,
            onForgetHub = if (canForgetHub) ({ gatt.forgetHub(); hubForgotten++ }) else null,
            session = session,
            finishedContent = if (edf != null && shouldShowSessionDownload(session.status, session.epoch)) {
                { EdfDownloadSection(edf, session.epoch, fileTransfer) }
            } else null,
            onConnect = requestConnect,
            onChooseProtocol = { showMenu = true },
            // The Session tab opens only once first-run setup has finished (the first-setup gate), as the iOS card appears on its complete step.
            onProgramAutonomous = { programMode = true; showMenu = true },
            onStop = { gatt.requestSessionStop() },
            progress = progress,
            onPause = { gatt.requestSessionPause() },
            onResume = { gatt.requestSessionResume() },
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
