package life.neurone.app.ui

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import life.neurone.app.NeurOneApplication
import life.neurone.shared.ui.NeurOneApp

// Every screen is in app/NeurOneUI/shared; this activity only hosts them.
class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val app = application as NeurOneApplication
        setContent { NeurOneApp(services = app.services) }
    }
}
