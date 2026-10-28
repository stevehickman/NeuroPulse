package life.neurone.app.ui

import android.Manifest
import android.content.Intent
import android.os.Bundle
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import life.neurone.app.AndroidReminderNotifier
import life.neurone.app.NeurOneApplication
import life.neurone.shared.ui.NeurOneApp

// Every screen is in app/NeurOneUI/shared; this activity only hosts them.
class MainActivity : ComponentActivity() {
    // The system's dialog for permission to notify (Android 13 and later), registered here because it must be before the activity starts.
    private val notificationPermission = registerForActivityResult(ActivityResultContracts.RequestPermission()) {}

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val app = application as NeurOneApplication
        (app.services.platform.reminderNotifier as? AndroidReminderNotifier)?.askPermission =
            { notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS) }
        deliverRedirect(intent)
        setContent { NeurOneApp(services = app.services) }
    }

    // The system browser returning a cloud sign-in (SWR-MG-19). Delivered here before onResume, which is what lets the launcher tell
    // an answer from a browser the person closed.
    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        deliverRedirect(intent)
    }

    private fun deliverRedirect(intent: Intent?) {
        val address = intent?.data?.toString() ?: return
        (application as NeurOneApplication).browserLauncher.onRedirect(address)
    }

    override fun onPause() {
        super.onPause()
        (application as NeurOneApplication).browserLauncher.onHostPaused()
    }

    override fun onResume() {
        super.onResume()
        (application as NeurOneApplication).browserLauncher.onHostResumed()
    }
}
