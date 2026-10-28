package life.neurone.app

import android.content.Context
import android.content.Intent
import android.net.Uri

/**
 * Shows a cloud provider's sign-in page in the person's own browser (SWR-MG-19). The system's browser, never a web view inside the
 * app: the app then cannot read what is typed there, and the person's existing sign-in to the provider is used (RFC 8252 section 4).
 * Only an https address is opened, because the address comes from a provider's endpoint and a sign-in over anything else is not one.
 *
 * The page's answer comes back as an intent for the redirect scheme the manifest registers, delivered to [life.neurone.app.ui.MainActivity].
 */
object AndroidBrowser {
    fun open(context: Context, address: String) {
        require(address.startsWith("https://")) { "a sign-in page is opened over https only" }
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(address))
            .addCategory(Intent.CATEGORY_BROWSABLE)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        // ActivityNotFoundException (no browser installed) propagates; the launcher reports it as a failure to open.
        context.startActivity(intent)
    }
}
