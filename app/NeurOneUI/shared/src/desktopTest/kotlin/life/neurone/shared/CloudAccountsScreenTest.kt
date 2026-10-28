package life.neurone.shared

import androidx.compose.runtime.Composable
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.hasSetTextAction
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import life.neurone.core.cloudauth.AuthorizationLauncher
import life.neurone.core.cloudauth.CloudAuthConfig
import life.neurone.core.cloudauth.OAuthClientConfig
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.resources.*
import life.neurone.shared.ui.CloudAccountsScreen
import life.neurone.shared.ui.SettingsScreen
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** The cloud accounts screen drives the model: what it offers follows the build's registrations, and a sign-in shows its outcome in words (SWR-MG-20). */
@OptIn(ExperimentalTestApi::class)
class CloudAccountsScreenTest {
    private var shown = 0

    private fun app(config: CloudAuthConfig, withCloud: Boolean = true) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = InMemoryKeyValueStore()
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val httpTransport: HttpTransport? = if (!withCloud) null else object : HttpTransport {
                override suspend fun post(request: HttpRequest) =
                    if ("revoke" in request.url) HttpResponse(200, emptyMap(), ByteArray(0))
                    else HttpResponse(200, emptyMap(), """{"access_token":"a","refresh_token":"r","expires_in":3600}""".encodeToByteArray())
            }
            override val authorizationLauncher: AuthorizationLauncher? = if (!withCloud) null else object : AuthorizationLauncher {
                override suspend fun authorize(authorizationUrl: String, redirectUri: String): String {
                    shown++
                    return "$redirectUri?code=c&state=${Regex("[?&]state=([^&]+)").find(authorizationUrl)!!.groupValues[1]}"
                }
            }
            @Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
        cloudAuthConfig = config,
    )

    // @verifies SWR-MG-20#a
    @Test fun a_provider_the_build_is_not_registered_for_says_so_and_offers_no_sign_in() = runComposeUiTest {
        var notConfigured = ""
        var signIn = ""
        setContent {
            notConfigured = stringResource(Res.string.cloud_status_not_configured)
            signIn = stringResource(Res.string.cloud_button_sign_in)
            CloudAccountsScreen(app(CloudAuthConfig(google = OAuthClientConfig("g", "http://127.0.0.1:53682"))), onDone = {})
        }
        waitForIdle()
        // Microsoft's two services and iCloud are not registered; Google's two are.
        assertEquals(3, onAllNodesWithText(notConfigured).fetchSemanticsNodes().size)
        assertEquals(2, onAllNodesWithText(signIn).fetchSemanticsNodes().size)
    }

    // @verifies SWR-MG-20#b
    @Test fun signing_in_shows_the_outcome_and_then_offers_sign_out() = runComposeUiTest {
        var signIn = ""
        var signedInNotice = ""
        var signOut = ""
        var signedOutNotice = ""
        setContent {
            signIn = stringResource(Res.string.cloud_button_sign_in)
            signOut = stringResource(Res.string.cloud_button_sign_out)
            signedInNotice = stringResource(Res.string.cloud_notice_signed_in, stringResource(Res.string.cloud_service_google_drive))
            signedOutNotice = stringResource(Res.string.cloud_notice_signed_out, stringResource(Res.string.cloud_service_google_drive))
            CloudAccountsScreen(app(CloudAuthConfig(google = OAuthClientConfig("g", "http://127.0.0.1:53682"))), onDone = {})
        }
        waitForIdle()
        onAllNodesWithText(signIn)[0].performClick() // Google Drive is first
        waitUntil(timeoutMillis = 10_000) { onAllNodesWithText(signedInNotice).fetchSemanticsNodes().isNotEmpty() }
        assertEquals(1, shown)
        assertEquals(1, onAllNodesWithText(signOut).fetchSemanticsNodes().size)

        onNodeWithText(signOut).performClick()
        waitUntil(timeoutMillis = 10_000) { onAllNodesWithText(signedOutNotice).fetchSemanticsNodes().isNotEmpty() }
        assertEquals(2, onAllNodesWithText(signIn).fetchSemanticsNodes().size, "both Google services offer sign-in again")
    }

    // @verifies SWR-MG-20#d
    @Test fun aws_keys_are_typed_into_masked_fields_and_saving_needs_both() = runComposeUiTest {
        var addKeys = ""
        var save = ""
        var cancel = ""
        setContent {
            addKeys = stringResource(Res.string.cloud_button_add_keys)
            save = stringResource(Res.string.cloud_button_save_keys)
            cancel = stringResource(Res.string.common_cancel)
            CloudAccountsScreen(app(CloudAuthConfig()), onDone = {})
        }
        waitForIdle()
        onNodeWithText(addKeys).performClick()
        val fields = onAllNodes(hasSetTextAction())
        assertEquals(3, fields.fetchSemanticsNodes().size, "access key id, secret key, session token")
        val masked = onAllNodes(SemanticsMatcher.keyIsDefined(SemanticsProperties.Password))
        assertEquals(2, masked.fetchSemanticsNodes().size, "the secret and the session token are masked; the key id is not")

        onNodeWithText(save).assertIsNotEnabled()
        fields[0].performTextInput("AKIAEXAMPLE")
        onNodeWithText(save).assertIsNotEnabled()
        fields[1].performTextInput("wJalrXUtnFEMI")
        onNodeWithText(save).assertIsEnabled()

        // The AWS card is the last on the screen, so its buttons are below the test window until scrolled to; a click there hits nothing.
        onNodeWithText(cancel).performScrollTo().performClick()
        assertTrue(onAllNodes(hasSetTextAction()).fetchSemanticsNodes().isEmpty(), "cancelling drops the form, and with it what was typed")
    }

    // @verifies SWR-MG-20#d
    @Test fun saving_aws_keys_shows_the_outcome_and_the_form_with_what_was_typed_goes_away() = runComposeUiTest {
        var addKeys = ""
        var save = ""
        var saved = ""
        setContent {
            addKeys = stringResource(Res.string.cloud_button_add_keys)
            save = stringResource(Res.string.cloud_button_save_keys)
            saved = stringResource(Res.string.cloud_notice_keys_saved, stringResource(Res.string.cloud_service_aws_s3))
            CloudAccountsScreen(app(CloudAuthConfig()), onDone = {})
        }
        waitForIdle()
        onNodeWithText(addKeys).performClick()
        val fields = onAllNodes(hasSetTextAction())
        fields[0].performTextInput("AKIAEXAMPLE")
        fields[1].performTextInput("wJalrXUtnFEMI")
        onNodeWithText(save).performScrollTo().performClick()
        waitUntil(timeoutMillis = 10_000) { onAllNodesWithText(saved).fetchSemanticsNodes().isNotEmpty() }
        assertTrue(onAllNodes(hasSetTextAction()).fetchSemanticsNodes().isEmpty(), "the fields are gone, and with them what was typed")
    }

    // @verifies SWR-MG-20#f
    @Test fun settings_offers_the_cloud_accounts_only_on_a_host_that_has_a_cloud_sign_in() = runComposeUiTest {
        var button = ""
        var back = ""
        var unavailable = ""
        setContent {
            button = stringResource(Res.string.cloud_accounts_button)
            back = stringResource(Res.string.common_back)
            unavailable = stringResource(Res.string.cloud_accounts_unavailable)
            SettingsScreen(app(CloudAuthConfig(), withCloud = false))
        }
        onNodeWithText(button).assertDoesNotExist()
    }

    // @verifies SWR-MG-20#f
    @Test fun settings_opens_the_cloud_accounts_where_the_host_has_a_cloud_sign_in() = runComposeUiTest {
        var button = ""
        var back = ""
        setContent {
            button = stringResource(Res.string.cloud_accounts_button)
            back = stringResource(Res.string.common_back)
            SettingsScreen(app(CloudAuthConfig()))
        }
        onNodeWithText(button).performScrollTo().performClick()
        onNodeWithText(back).assertExists() // the accounts screen has replaced Settings
        onNodeWithText(button).assertDoesNotExist()
    }
}
