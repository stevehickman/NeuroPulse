package com.neuropulse.app.ui

import android.os.Bundle
import androidx.activity.compose.setContent
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
            0 -> SessionScreen(modifier)
            1 -> HistoryScreen(app.sessionHistoryStore, modifier)
            2 -> ConsumablesScreen(modifier)
            3 -> ConsentDashboardScreen(app.consentStore, modifier)
            else -> SettingsScreen(app, modifier)
        }
    }
}

object OnboardingKeys {
    const val AGE_CONFIRMED = "np.onboarding.age-confirmed"
    const val BIPA_ACCEPTED = "np.onboarding.bipa-accepted"
}
