package life.neurone.shared.ui

import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.DateRange
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.ShoppingCart
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import life.neurone.core.protocol.PersistedKeys
import life.neurone.shared.AppServices
import life.neurone.shared.resources.*
import life.neurone.shared.text.installValidationText
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

/** Storage keys of the onboarding steps. The values are the ones the Android app has always written. */
object OnboardingKeys {
    const val BIPA_ACCEPTED = "np.onboarding.bipa-accepted"
    const val CONSENT_SHOWN = "np.onboarding.consent-shown"
}

/** Where a screen reports the outcome of an action (an upload, a refusal) without leaving the screen. */
val LocalSnackbarHost = compositionLocalOf { SnackbarHostState() }

/** The five top-level destinations, in tab order. */
enum class AppTab(val label: StringResource, val icon: ImageVector) {
    SESSION(Res.string.session_title, Icons.Filled.PlayArrow),
    HISTORY(Res.string.tab_history, Icons.Filled.DateRange),
    CONSUMABLES(Res.string.tab_consumables, Icons.Filled.ShoppingCart),
    PRIVACY(Res.string.setup_privacy_card_title, Icons.Filled.Lock),
    SETTINGS(Res.string.tab_settings, Icons.Filled.Settings),
}

/**
 * The whole app, for every platform: onboarding gates, then the five tabs.
 *
 * Onboarding gates precede ALL personal-data collection or display (age gate before consent layers,
 * BIPA release before EEG), and the order is decided here, once, so no platform can reorder it.

 * There is no per-host override: every tab is the shared screen, so the platforms can differ only through
 * `PlatformServices`.
 */
@Composable
fun NeurOneApp(services: AppServices) {
    // The validator's messages are locale keys until their text is read (asynchronously, once).
    LaunchedEffect(Unit) { installValidationText() }
    MaterialTheme {
        Root(services)
    }
}

@Composable
private fun Root(services: AppServices) {
    val kv = services.keyValueStore
    var ageConfirmed by remember { mutableStateOf(kv.getBoolean(PersistedKeys.AGE_CONFIRMED_KEY)) }
    var bipaAccepted by remember { mutableStateOf(kv.getBoolean(OnboardingKeys.BIPA_ACCEPTED)) }
    var consentShown by remember { mutableStateOf(kv.getBoolean(OnboardingKeys.CONSENT_SHOWN)) }

    when {
        !ageConfirmed -> AgeGateScreen(
            onConfirmed = {
                kv.putBoolean(PersistedKeys.AGE_CONFIRMED_KEY, true)
                ageConfirmed = true
            },
        )
        !bipaAccepted -> BipaConsentScreen(
            onAccepted = {
                kv.putBoolean(OnboardingKeys.BIPA_ACCEPTED, true)
                bipaAccepted = true
            },
        )
        // Research-consent onboarding (L1–L4) follows biometric consent, before the app proper.
        // All layers are optional.
        !consentShown -> ConsentOnboardingScreen(
            store = services.consentStore,
            onComplete = {
                kv.putBoolean(OnboardingKeys.CONSENT_SHOWN, true)
                consentShown = true
            },
        )
        else -> MainScaffold(services)
    }
}

@Composable
private fun MainScaffold(services: AppServices) {
    var selected by remember { mutableStateOf(AppTab.SESSION) }

    val snackbar = remember { SnackbarHostState() }
    Scaffold(
        snackbarHost = { SnackbarHost(snackbar) },
        bottomBar = {
            NavigationBar {
                AppTab.entries.forEach { tab ->
                    NavigationBarItem(
                        selected = selected == tab,
                        onClick = { selected = tab },
                        icon = { Icon(tab.icon, contentDescription = null) },
                        label = { Text(stringResource(tab.label)) },
                    )
                }
            }
        },
    ) { padding ->
        val modifier = Modifier.padding(padding)
        CompositionLocalProvider(LocalSnackbarHost provides snackbar) {
            when (selected) {
                AppTab.SESSION -> SessionTab(services, modifier)
                AppTab.HISTORY -> HistoryScreen(services.sessionHistoryStore, modifier)
                AppTab.CONSUMABLES -> ConsumablesScreen(services, modifier)
                AppTab.PRIVACY -> ConsentDashboardScreen(services, modifier)
                AppTab.SETTINGS -> SettingsScreen(services, modifier)
            }
        }
    }
}
