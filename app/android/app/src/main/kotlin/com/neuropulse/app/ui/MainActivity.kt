package com.neuropulse.app.ui

import android.os.Build
import android.os.Bundle
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.DateRange
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.ShoppingCart
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.fragment.app.FragmentActivity
import com.neuropulse.app.NeuroPulseApplication
import com.neuropulse.app.R

// FragmentActivity (not ComponentActivity) — required by BiometricPrompt for
// the UHDR key credential flow.
class MainActivity : FragmentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val app = application as NeuroPulseApplication
        setContent {
            MaterialTheme {
                Root(app)
            }
        }
    }
}

@Composable
private fun Root(app: NeuroPulseApplication) {
    // Onboarding gates precede ALL personal-data collection or display
    // (parity with iOS: age gate before consent layers, BIPA before EEG).
    var ageConfirmed by remember {
        mutableStateOf(app.keyValueStore.getBoolean(OnboardingKeys.AGE_CONFIRMED))
    }
    var bipaAccepted by remember {
        mutableStateOf(app.keyValueStore.getBoolean(OnboardingKeys.BIPA_ACCEPTED))
    }
    var consentShown by remember {
        mutableStateOf(app.keyValueStore.getBoolean(OnboardingKeys.CONSENT_SHOWN))
    }

    when {
        !ageConfirmed -> AgeGateScreen(
            onConfirmed = {
                app.keyValueStore.putBoolean(OnboardingKeys.AGE_CONFIRMED, true)
                ageConfirmed = true
            },
        )
        !bipaAccepted -> BipaConsentScreen(
            onAccepted = {
                app.keyValueStore.putBoolean(OnboardingKeys.BIPA_ACCEPTED, true)
                bipaAccepted = true
            },
        )
        // Research-consent onboarding (L1–L4) follows biometric consent, before the app
        // proper — mirrors iOS ordering. All layers are optional.
        !consentShown -> ConsentOnboardingScreen(
            store = app.consentStore,
            onComplete = {
                app.keyValueStore.putBoolean(OnboardingKeys.CONSENT_SHOWN, true)
                consentShown = true
            },
        )
        else -> MainScaffold(app)
    }
}

private data class Tab(val labelRes: Int, val icon: androidx.compose.ui.graphics.vector.ImageVector)

@Composable
private fun MainScaffold(app: NeuroPulseApplication) {
    var selected by remember { mutableIntStateOf(0) }
    val tabs = listOf(
        Tab(R.string.tab_session, Icons.Filled.PlayArrow),
        Tab(R.string.tab_history, Icons.Filled.DateRange),
        Tab(R.string.tab_consumables, Icons.Filled.ShoppingCart),
        Tab(R.string.tab_consent, Icons.Filled.Lock),
        Tab(R.string.tab_settings, Icons.Filled.Settings),
    )

    Scaffold(
        bottomBar = {
            NavigationBar {
                tabs.forEachIndexed { index, tab ->
                    NavigationBarItem(
                        selected = selected == index,
                        onClick = { selected = index },
                        icon = { Icon(tab.icon, contentDescription = null) },
                        label = { Text(stringResource(tab.labelRes)) },
                    )
                }
            }
        },
    ) { padding ->
        val modifier = Modifier.padding(padding)
        when (selected) {
            0 -> SessionTab(app, modifier)
            1 -> HistoryScreen(app.sessionHistoryStore, modifier)
            2 -> ConsumablesScreen(app, modifier)
            3 -> ConsentDashboardScreen(app, modifier)
            else -> SettingsScreen(app, modifier)
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
private fun SessionTab(app: NeuroPulseApplication, modifier: Modifier) {
    var showMenu by remember { mutableStateOf(false) }
    val context = androidx.compose.ui.platform.LocalContext.current
    val consentGranted = app.keyValueStore.getBoolean(OnboardingKeys.BIPA_ACCEPTED)
    val eegMessage = stringResource(R.string.session_eeg_unavailable_body)

    // Live BLE state from the app-scoped GATT manager (OI-AND-BLE-01).
    val connectionState by app.gattManager.connectionState.collectAsState()
    val session by app.gattManager.session.collectAsState()

    // Request BLE runtime permissions (Android 12+); on grant, kick off scanning.
    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { grants ->
        if (grants.values.all { it }) app.bleCentral.refresh()
    }

    fun requestConnect() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissionLauncher.launch(
                arrayOf(
                    android.Manifest.permission.BLUETOOTH_SCAN,
                    android.Manifest.permission.BLUETOOTH_CONNECT,
                ),
            )
        } else {
            app.bleCentral.refresh()
        }
    }

    if (showMenu) {
        ProtocolMenuScreen(
            library = app.protocolLibrary,
            consentGranted = consentGranted,
            eegUnavailableMessage = eegMessage,
            limits = app.limitsStore.resolvedLimits,
            onSelect = { entry ->
                // Compile → sign → chunk → upload (Mode 2). Composite upload is a follow-up.
                val result = when (entry) {
                    is com.neuropulse.core.protocol.NPProtocolEntry.Single ->
                        app.protocolUploader.upload(entry.protocol)
                    else ->
                        com.neuropulse.app.session.ProtocolUploader.Result.Failure(
                            "Composite protocol upload is not yet supported.",
                        )
                }
                val message = when (result) {
                    is com.neuropulse.app.session.ProtocolUploader.Result.Success -> "Protocol sent to hub"
                    is com.neuropulse.app.session.ProtocolUploader.Result.Failure -> result.message
                }
                android.widget.Toast.makeText(context, message, android.widget.Toast.LENGTH_SHORT).show()
                showMenu = false
            },
            onBack = { showMenu = false },
            modifier = modifier,
        )
    } else {
        SessionScreen(
            connectionState = connectionState,
            session = session,
            onConnect = { requestConnect() },
            onChooseProtocol = { showMenu = true },
            onStop = { app.gattManager.requestSessionStop() },
            modifier = modifier,
        )
    }
}

object OnboardingKeys {
    const val AGE_CONFIRMED = "np.onboarding.age-confirmed"
    const val BIPA_ACCEPTED = "np.onboarding.bipa-accepted"
    const val CONSENT_SHOWN = "np.onboarding.consent-shown"
}
