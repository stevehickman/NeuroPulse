package life.neurone.app.ui

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.fragment.app.FragmentActivity
import life.neurone.app.NeurOneApplication
import life.neurone.app.R
import life.neurone.shared.ui.AppTab
import life.neurone.shared.ui.NeurOneApp
import life.neurone.shared.ui.OnboardingKeys

// FragmentActivity (not ComponentActivity) — required by BiometricPrompt for
// the UHDR key credential flow.
class MainActivity : FragmentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val app = application as NeurOneApplication
        setContent {
            NeurOneApp(
                services = app.services,
                // Screens still written for Android only; each moves into app/NeurOneUI/shared and
                // leaves this map when it does.
                tabOverrides = mapOf(
                    AppTab.SESSION to { modifier -> SessionTab(app, modifier) },
                    AppTab.PRIVACY to { modifier -> ConsentDashboardScreen(app, modifier) },
                    AppTab.SETTINGS to { modifier -> SettingsScreen(app, modifier) },
                ),
            )
        }
    }
}

/**
 * Session tab: shows the live session view, or the protocol menu when the user taps
 * "Browse Protocols". Mirrors iOS, where ProtocolMenuView is presented from SessionView.
 * Live connection/session state flows in once an Android BleCentral is wired
 * (OI-AND-BLE-01); until then the session view renders the disconnected state.
 */
@Composable
fun SessionTab(app: NeurOneApplication, modifier: Modifier) {
    var showMenu by remember { mutableStateOf(false) }
    val context = androidx.compose.ui.platform.LocalContext.current
    val consentGranted = app.keyValueStore.getBoolean(OnboardingKeys.BIPA_ACCEPTED)
    val eegMessage = stringResource(R.string.session_eeg_unavailable_body)

    // Live BLE state from the app-scoped GATT manager (OI-AND-BLE-01).
    val connectionState by app.gattManager.connectionState.collectAsState()
    val session by app.gattManager.session.collectAsState()
    val cervicalPadAlert by app.gattManager.cervicalPadAlert.collectAsState()
    val cervicalFaultStatus by app.gattManager.cervicalFaultStatus.collectAsState()
    val unreadCervicalFaults by app.gattManager.unacknowledgedCervicalFaults.collectAsState()
    val cardiacWarningAcknowledged by app.gattManager.cardiacWarningAcknowledged.collectAsState()
    // A cervical protocol held back until the wearer confirms the selected profile is theirs.
    var awaitingDifferentPerson by remember {
        mutableStateOf<life.neurone.core.protocol.NPProtocolEntry.Single?>(null)
    }

    // A protocol held back until its author acknowledges each zone-model caution (docs/reference/safety-zones.md).
    var awaitingCautions by remember {
        mutableStateOf<Pair<life.neurone.core.protocol.NPProtocolEntry.Single, List<life.neurone.core.protocol.NPZoneCaution>>?>(null)
    }

    // Request BLE runtime permissions (Android 12+); on grant, kick off scanning.
    val requestConnect = app.services.platform.rememberBleConnectAction()

    fun uploadAndReport(
        entry: life.neurone.core.protocol.NPProtocolEntry,
        acknowledgedCautions: List<String> = emptyList(),
    ) {
        // A protocol in the caution zone runs only after its author acknowledges each caution
        // (docs/reference/safety-zones.md). The dialog calls back here with the ids, for this run only.
        if (entry is life.neurone.core.protocol.NPProtocolEntry.Single && acknowledgedCautions.isEmpty()) {
            val cautions = app.protocolUploader.cautions(entry.protocol)
            if (cautions.isNotEmpty()) {
                awaitingCautions = entry to cautions
                return
            }
        }
        // Compile → sign → chunk → upload (Mode 2). Composite upload is a follow-up.
        val result = when (entry) {
            is life.neurone.core.protocol.NPProtocolEntry.Single ->
                app.protocolUploader.upload(entry.protocol, acknowledgedCautions = acknowledgedCautions)
            else ->
                life.neurone.app.session.ProtocolUploader.Result.Failure(
                    context.getString(R.string.and_ui_composite_protocol_upload_is_not_yet_support),
                )
        }
        if (result is life.neurone.app.session.ProtocolUploader.Result.DifferentPersonConfirmationRequired &&
            entry is life.neurone.core.protocol.NPProtocolEntry.Single
        ) {
            awaitingDifferentPerson = entry
            return
        }
        val message = when (result) {
            is life.neurone.app.session.ProtocolUploader.Result.Success ->
                context.getString(R.string.protocol_menu_protocol_sent_to_hub)
            is life.neurone.app.session.ProtocolUploader.Result.Failure -> result.message
            is life.neurone.app.session.ProtocolUploader.Result.CervicalRestartBlocked ->
                context.getString(R.string.upload_cervical_blocked)
            is life.neurone.app.session.ProtocolUploader.Result.DifferentPersonConfirmationRequired ->
                context.getString(R.string.cvns_different_person_body)
        }
        android.widget.Toast.makeText(context, message, android.widget.Toast.LENGTH_SHORT).show()
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
            title = { Text(stringResource(R.string.cvns_different_person_title)) },
            text = { Text(stringResource(R.string.cvns_different_person_body)) },
            confirmButton = {
                TextButton(onClick = {
                    awaitingDifferentPerson = null
                    app.protocolUploader.confirmDifferentPerson()
                    uploadAndReport(held)
                }) { Text(stringResource(R.string.cvns_different_person_confirm)) }
            },
            dismissButton = {
                TextButton(onClick = { awaitingDifferentPerson = null }) {
                    Text(stringResource(R.string.common_cancel))
                }
            },
        )
    }

    if (showMenu) {
        ProtocolMenuScreen(
            library = app.protocolLibrary,
            consentGranted = consentGranted,
            eegUnavailableMessage = eegMessage,
            limits = app.limitsStore.resolvedLimits,
            onSelect = { entry -> uploadAndReport(entry) },
            onBack = { showMenu = false },
            modifier = modifier,
        )
    } else {
        SessionScreen(
            connectionState = connectionState,
            session = session,
            onConnect = requestConnect,
            onChooseProtocol = { showMenu = true },
            onStop = { app.gattManager.requestSessionStop() },
            modifier = modifier,
            cervicalPadAlert = cervicalPadAlert,
            onAcknowledgeCervicalPadAlert = { app.gattManager.acknowledgeCervicalPadAlert() },
            cervicalFaultStatus = cervicalFaultStatus,
            unacknowledgedCervicalFaults = unreadCervicalFaults,
            onAcknowledgeCervicalFaults = { app.gattManager.acknowledgeCervicalFaults() },
            cardiacWarningAcknowledged = cardiacWarningAcknowledged,
            onAcknowledgeCardiacWarning = { app.gattManager.acknowledgeCardiacWarning() },
            onConfirmCervicalResume = { app.gattManager.sendCervicalReenableConfirm() },
        )
    }
}
