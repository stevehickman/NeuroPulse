package life.neurone.app.ui

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.fragment.app.FragmentActivity
import life.neurone.app.NeurOneApplication
import life.neurone.shared.ui.NeurOneApp

// FragmentActivity (not ComponentActivity) — required by BiometricPrompt for
// the UHDR key credential flow. Every screen is in app/NeurOneUI/shared; this activity only hosts them.
class MainActivity : FragmentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val app = application as NeurOneApplication
        setContent { NeurOneApp(services = app.services) }
    }
}
