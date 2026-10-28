package life.neurone.shared

import androidx.compose.runtime.Composable
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.runBlocking
import life.neurone.core.cloudauth.AuthorizationLauncher
import life.neurone.core.cloudauth.CloudAuthConfig
import life.neurone.core.cloudauth.CloudService
import life.neurone.core.cloudauth.OAuthClientConfig
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.resources.*
import life.neurone.shared.ui.BackupDestinationRow
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** The row on the backup and restore screens that says where the backup is kept (SWR-MG-21). */
@OptIn(ExperimentalTestApi::class)
class CloudBackupRowTest {
    private fun app(signedInToDrive: Boolean = false, endedSignIn: Boolean = false): AppServices {
        val services = AppServices(
            platform = object : PlatformServices {
                override val keyValueStore = InMemoryKeyValueStore()
                override val bleCentral = UnavailableBleCentral()
                override val protocolSigner = Ed25519ProtocolSigner()
                override val analyticsBackend = NoOpAnalyticsBackend()
                override val httpTransport: HttpTransport? = object : HttpTransport {
                    override suspend fun post(request: HttpRequest) =
                        HttpResponse(200, emptyMap(), """{"access_token":"a","refresh_token":"r","expires_in":3600,"files":[]}""".encodeToByteArray())
                }
                override val authorizationLauncher: AuthorizationLauncher? = object : AuthorizationLauncher {
                    override suspend fun authorize(authorizationUrl: String, redirectUri: String) =
                        "$redirectUri?code=c&state=${Regex("[?&]state=([^&]+)").find(authorizationUrl)!!.groupValues[1]}"
                }
                @Composable
                override fun rememberBleConnectAction(): () -> Unit = {}
            },
            scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
            cloudAuthConfig = CloudAuthConfig(google = OAuthClientConfig("g", "http://127.0.0.1:53682")),
        )
        if (signedInToDrive) runBlocking {
            val account = services.cloudAuth!!.signIn(CloudService.GOOGLE_DRIVE)
            if (endedSignIn) services.cloudAuth!!.markNeedsSignIn(account.id)
        }
        return services
    }

    // @verifies SWR-MG-21#a
    @Test fun with_no_account_the_row_says_where_to_sign_in_and_offers_no_choice() = runComposeUiTest {
        var none = ""
        var device = ""
        setContent {
            none = stringResource(Res.string.cloud_backup_none)
            device = stringResource(Res.string.cloud_backup_this_device)
            BackupDestinationRow(app())
        }
        waitForIdle()
        onNodeWithText(none).assertExists()
        onNodeWithText(device).assertDoesNotExist()
    }

    // @verifies SWR-MG-21#a
    @Test fun choosing_an_account_and_going_back_to_this_device() = runComposeUiTest {
        var device = ""
        var drive = ""
        var limits = ""
        val services = app(signedInToDrive = true)
        setContent {
            device = stringResource(Res.string.cloud_backup_this_device)
            drive = stringResource(Res.string.cloud_service_google_drive)
            limits = stringResource(Res.string.cloud_backup_limits)
            BackupDestinationRow(services)
        }
        waitForIdle()
        onNodeWithText(limits).assertDoesNotExist()
        onNodeWithText(drive).performClick()
        waitUntil(timeoutMillis = 10_000) { onAllNodesWithTextCount(limits) == 1 }
        assertEquals("Google Drive", services.backupPlace.value?.label, "the screens now use the cloud place")

        onNodeWithText(device).performClick()
        waitUntil(timeoutMillis = 10_000) { onAllNodesWithTextCount(limits) == 0 }
        assertNull(services.backupPlace.value, "this device again (the host here has no place of its own)")
    }

    // @verifies SWR-MG-21#e
    @Test fun an_account_whose_sign_in_was_ended_cannot_be_chosen_and_the_row_says_to_sign_in_again() = runComposeUiTest {
        var drive = ""
        var warning = ""
        setContent {
            drive = stringResource(Res.string.cloud_service_google_drive)
            warning = stringResource(Res.string.cloud_backup_needs_sign_in, stringResource(Res.string.cloud_service_google_drive))
            BackupDestinationRow(app(signedInToDrive = true, endedSignIn = true))
        }
        waitForIdle()
        onNodeWithText(drive).assertIsNotEnabled()
        onNodeWithText(warning).assertExists()
    }

    private fun androidx.compose.ui.test.ComposeUiTest.onAllNodesWithTextCount(text: String) =
        onAllNodes(androidx.compose.ui.test.hasText(text)).fetchSemanticsNodes().size
}
