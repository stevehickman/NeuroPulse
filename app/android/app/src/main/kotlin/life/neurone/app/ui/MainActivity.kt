package life.neurone.app.ui

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.fragment.app.FragmentActivity
import life.neurone.app.NeurOneApplication
import life.neurone.shared.ui.AppTab
import life.neurone.shared.ui.NeurOneApp

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
                    AppTab.SETTINGS to { modifier -> SettingsScreen(app, modifier) },
                ),
            )
        }
    }
}
