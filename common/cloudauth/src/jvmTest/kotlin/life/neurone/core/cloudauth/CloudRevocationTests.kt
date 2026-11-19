package life.neurone.core.cloudauth

import kotlinx.coroutines.runBlocking
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * SWR-MG-29 (d): a credential that leaves this install is ended at the provider where the provider has a call for it, and the result says when
 * it was not. Against the same stand-in as `CloudAuthTests`, so a pass shows what this library sends, not what a provider does with it.
 */
class CloudRevocationTests {
    private fun Rig.revokes() = providers.seen.filter { it.url == GOOGLE_REVOKE || it.url == DROPBOX_REVOKE }

    // @verifies SWR-MG-29#d
    @Test
    fun signingInAgainRevokesTheEarlierGoogleGrantBeforeTheBrowserOpens() = runBlocking {
        val rig = Rig()
        val first = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        assertNull(first.replaced, "a first sign-in replaced nothing")
        var revokedWhenTheBrowserOpened = -1
        rig.browser.callback = null
        val before = rig.browser.urls.size
        val watching = object : AuthorizationLauncher {
            override suspend fun authorize(authorizationUrl: String, redirectUri: String): String {
                revokedWhenTheBrowserOpened = rig.revokes().size
                return rig.browser.authorize(authorizationUrl, redirectUri)
            }
        }
        val auth = CloudAuth(rig.providers, rig.credentials, rig.config, watching, CountingRandom(), life.neurone.core.platform.Digests::sha256, { rig.clock })
        val again = auth.signIn(CloudService.GOOGLE_DRIVE, replacing = first.id)
        assertEquals(first.id, again.id)
        assertEquals(Revocation.REVOKED, again.replaced)
        assertEquals(1, revokedWhenTheBrowserOpened, "revoked first: Google's page says a revocation ends every token of the grant, so revoking afterwards could end the new one")
        assertEquals("refresh-1", rig.providers.form(rig.revokes().single())["token"], "the earlier refresh token, not the new one")
        assertEquals(before + 1, rig.browser.urls.size)
        assertEquals("access-2", auth.accessToken(again.id).bearer(), "the new sign-in is the one kept")
    }

    // @verifies SWR-MG-29#d
    @Test
    fun aSignInAgainThatThePersonAbandonsLeavesNoCredentialThatLooksAlive() = runBlocking {
        val rig = Rig()
        val first = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        rig.browser.mode = "cancel"
        assertEquals(CloudAuthProblem.CANCELLED, assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.GOOGLE_DRIVE, replacing = first.id) }.problem)
        val left = rig.auth.accounts().single()
        assertEquals(first.id, left.id)
        assertTrue(left.needsSignIn, "the grant was revoked, so the account reads as needing sign-in")
        assertEquals(CloudAuthProblem.SIGN_IN_REQUIRED, assertFailsWith<CloudAuthException> { rig.auth.accessToken(first.id).bearer() }.problem)
        assertTrue("refresh-1" !in rig.kv.getString(CredentialStore.KEY)!! && "access-1" !in rig.kv.getString(CredentialStore.KEY)!!, "the revoked tokens are not kept")
    }

    // @verifies SWR-MG-29#d
    @Test
    fun anEarlierGrantThatCouldNotBeRevokedIsReportedAndKeptForALaterSignOut() = runBlocking {
        val rig = Rig()
        val first = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        rig.providers.revokeStatus = 400
        rig.browser.mode = "cancel"
        assertFailsWith<CloudAuthException> { rig.auth.signIn(CloudService.GOOGLE_DRIVE, replacing = first.id) }
        assertTrue("refresh-1" in rig.kv.getString(CredentialStore.KEY)!!, "not revoked, so still held: a sign-out can try again")
        rig.browser.mode = "approve"
        assertEquals(Revocation.FAILED, rig.auth.signIn(CloudService.GOOGLE_DRIVE, replacing = first.id).replaced)

        // Microsoft documents no call by which a public client ends a refresh token.
        val m = rig.auth.signIn(CloudService.ONEDRIVE)
        val seenBefore = rig.providers.seen.size
        val again = rig.auth.signIn(CloudService.ONEDRIVE, replacing = m.id)
        assertEquals(Revocation.NOT_AVAILABLE, again.replaced)
        assertEquals(1, rig.providers.seen.size - seenBefore, "only the code was redeemed")
    }

    // @verifies SWR-MG-29#d
    @Test
    fun signingInToDropboxAgainRevokesTheEarlierGrantWithItsOwnAccessToken() = runBlocking {
        val rig = Rig()
        val first = rig.auth.signIn(CloudService.DROPBOX)
        val again = rig.auth.signIn(CloudService.DROPBOX, replacing = first.id)
        assertEquals(Revocation.REVOKED, again.replaced)
        assertEquals("Bearer access-1", rig.revokes().single().headers["Authorization"], "Dropbox disables the token that makes the call, and its refresh token")

        // With the kept access token past its time, one is refreshed for the call and not stored.
        rig.clock += 7_200
        val third = rig.auth.signIn(CloudService.DROPBOX, replacing = first.id)
        assertEquals(Revocation.REVOKED, third.replaced)
        assertEquals("Bearer access-3", rig.revokes().last().headers["Authorization"])
    }

    // @verifies SWR-MG-29#d
    @Test
    fun aSignInToAProviderThatIsNotSetUpRevokesNothing() = runBlocking {
        val rig = Rig()
        val first = rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        val bare = CloudAuth(rig.providers, rig.credentials, CloudAuthConfig(), rig.browser, CountingRandom(), life.neurone.core.platform.Digests::sha256, { rig.clock })
        assertEquals(CloudAuthProblem.NOT_CONFIGURED, assertFailsWith<CloudAuthException> { bare.signIn(CloudService.GOOGLE_DRIVE, replacing = first.id) }.problem)
        assertTrue(rig.revokes().isEmpty())
        assertEquals("access-1", rig.auth.accessToken(first.id).bearer(), "the account is as it was")
    }

    // @verifies SWR-MG-29#d
    @Test
    fun replacingATypedCredentialSaysTheEarlierOneStillWorksAndTypingTheSameOneReplacesNothing() = runBlocking {
        val rig = Rig()
        val keys = rig.auth.addAwsKeys("AKID", "secret-1")
        assertNull(keys.replaced)
        assertNull(rig.auth.addAwsKeys("AKID", "secret-1", replacing = keys.id).replaced, "the same keys again")
        assertEquals(Revocation.NOT_AVAILABLE, rig.auth.addAwsKeys("AKID2", "secret-2", replacing = keys.id).replaced, "the old keys work until the person deletes them in AWS")

        val login = rig.auth.addWebDavLogin("https://dav.example.test/remote.php/dav/files/me/", "me", "pw-1")
        assertEquals(Revocation.NOT_AVAILABLE, rig.auth.addWebDavLogin("https://dav.example.test/remote.php/dav/files/me/", "me", "pw-2", replacing = login.id).replaced)

        val sas = "sv=2024-11-04&spr=https&se=2030-01-01&sr=c&sp=rcwdl&sig=one"
        val signature = rig.auth.addAzureSas(sas)
        assertNull(rig.auth.addAzureSas(sas, replacing = signature.id).replaced)
        assertEquals(Revocation.NOT_AVAILABLE, rig.auth.addAzureSas(sas.replace("sig=one", "sig=two"), replacing = signature.id).replaced)
        assertTrue(rig.providers.seen.isEmpty(), "nothing was sent to anyone")
    }

    // @verifies SWR-MG-29#d
    @Test
    fun anEraseForgetsEveryAccountAndAsksEachProviderToEndItsGrant() = runBlocking {
        val rig = Rig()
        rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        rig.auth.signIn(CloudService.ONEDRIVE)
        rig.auth.signIn(CloudService.DROPBOX)
        rig.auth.addAwsKeys("AKID", "secret")
        val result = rig.auth.signOutAll()
        assertEquals(
            listOf(
                CloudService.GOOGLE_DRIVE to Revocation.REVOKED, CloudService.ONEDRIVE to Revocation.NOT_AVAILABLE,
                CloudService.DROPBOX to Revocation.REVOKED, CloudService.AWS_S3 to Revocation.NOT_AVAILABLE,
            ),
            result,
        )
        assertNull(rig.kv.getString(CredentialStore.KEY), "every credential is gone from this install")
        assertEquals(2, rig.revokes().size)
        assertTrue(rig.auth.signOutAll().isEmpty(), "nothing held, nothing asked")
    }

    // @verifies SWR-MG-29#d
    @Test
    fun anEraseWithNoNetworkStillForgetsEverythingAndSaysWhatMayRemain() = runBlocking {
        val rig = Rig()
        rig.auth.signIn(CloudService.GOOGLE_DRIVE)
        rig.auth.signIn(CloudService.DROPBOX)
        rig.providers.offline = true
        assertEquals(listOf(CloudService.GOOGLE_DRIVE to Revocation.FAILED, CloudService.DROPBOX to Revocation.FAILED), rig.auth.signOutAll())
        assertNull(rig.kv.getString(CredentialStore.KEY))

        // Credentials that cannot be read are not deleted here and nothing is sent: the erase removes the raw value itself.
        val broken = Rig()
        broken.kv.putString(CredentialStore.KEY, "not json")
        assertEquals(CloudAuthProblem.STORE_UNREADABLE, assertFailsWith<CloudAuthException> { broken.auth.signOutAll() }.problem)
        assertTrue(broken.providers.seen.isEmpty())
    }
}
