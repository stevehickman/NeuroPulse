package life.neurone.app.ui

import android.Manifest
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
        setContent { NeurOneApp(services = app.services) }
    }
}
