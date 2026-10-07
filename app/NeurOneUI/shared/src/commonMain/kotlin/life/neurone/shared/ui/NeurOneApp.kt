package life.neurone.shared.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
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
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.PersistedKeys
import life.neurone.shared.AppServices
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

/** Storage keys of the onboarding steps. The values are the ones the Android app has always written. */
object OnboardingKeys {
    const val BIPA_ACCEPTED = "np.onboarding.bipa-accepted"
    const val CONSENT_SHOWN = "np.onboarding.consent-shown"
}

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
 *
 * [tabOverrides] lets a host supply a tab's content while that screen is still platform code. A tab
 * with neither a shared screen nor an override shows [TabPending]. That list is the honest map of
 * where the platforms still differ, and it shrinks as screens move into this module
 * (app/NeurOneUI/README.md keeps the current list).
 */
@Composable
fun NeurOneApp(
    services: AppServices,
    tabOverrides: Map<AppTab, @Composable (Modifier) -> Unit> = emptyMap(),
) {
    MaterialTheme {
        Root(services, tabOverrides)
    }
}

@Composable
private fun Root(services: AppServices, tabOverrides: Map<AppTab, @Composable (Modifier) -> Unit>) {
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
        else -> MainScaffold(services, tabOverrides)
    }
}

@Composable
private fun MainScaffold(services: AppServices, tabOverrides: Map<AppTab, @Composable (Modifier) -> Unit>) {
    var selected by remember { mutableStateOf(AppTab.SESSION) }

    Scaffold(
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
        val override = tabOverrides[selected]
        when {
            override != null -> override(modifier)
            selected == AppTab.HISTORY -> HistoryScreen(services.sessionHistoryStore, modifier)
            selected == AppTab.CONSUMABLES -> ConsumablesScreen(services, modifier)
            else -> TabPending(modifier)
        }
    }
}

/** A tab whose screen has not been brought into the shared module, on a host that has no override for it. */
@Composable
fun TabPending(modifier: Modifier = Modifier) {
    Box(modifier.fillMaxSize().padding(24.dp), contentAlignment = Alignment.Center) {
        Text(stringResource(Res.string.app_tab_pending_body), style = MaterialTheme.typography.bodyLarge)
    }
}
