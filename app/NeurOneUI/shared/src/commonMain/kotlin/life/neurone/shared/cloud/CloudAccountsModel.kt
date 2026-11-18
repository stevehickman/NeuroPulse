package life.neurone.shared.cloud

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import life.neurone.core.cloudauth.AccountFamily
import life.neurone.core.cloudauth.CloudAccount
import life.neurone.core.cloudauth.CloudAuth
import life.neurone.core.cloudauth.CloudAuthException
import life.neurone.core.cloudauth.CloudAuthProblem
import life.neurone.core.cloudauth.CloudService
import life.neurone.core.cloudauth.Revocation

/** One service on the cloud accounts screen: whether this build can sign in to it at all, and the account the person has there, if any. */
class CloudServiceRow(val service: CloudService, val configured: Boolean, val account: CloudAccount?) {
    /**
     * True for a service where a file this app deletes is kept by the service, recoverable, until the person empties it there: OneDrive's recycle
     * bin (Microsoft's delete page) and Dropbox's deleted files. The app can end neither with the access it asks for, so the screen says so
     * (SWR-MG-28 (d)).
     */
    val keepsDeletedFiles: Boolean get() = account != null && (service == CloudService.ONEDRIVE || service == CloudService.DROPBOX)

    val signedIn: Boolean get() = account != null && !account.needsSignIn
    val needsSignIn: Boolean get() = account?.needsSignIn == true
}

/**
 * What happened, in words the screen can say. **Never the provider's own text, a token, or an exception message**: the screen maps each
 * kind to a localized string of its own (`CLAUDE.md` §17), so nothing a provider or a failed call sends can reach the person's screen
 * or a log through here.
 */
enum class CloudNoticeKind {
    SIGNED_IN,
    SIGNED_OUT,

    /** The local sign-in is gone, but the provider could not be asked to end the app's access (it has no such call, or the call failed). */
    SIGNED_OUT_NOT_REVOKED,

    /**
     * A new sign-in, key, login or signature was saved in place of an earlier one, and the provider could not be asked to end the earlier one (it
     * has no such call, or the call failed), so that one may still work there (SWR-MG-28 (d)).
     */
    REPLACED_NOT_REVOKED,
    NOT_CONFIGURED,
    DENIED,
    FAILED,
    NETWORK,

    /** The stored sign-ins cannot be read on this device. Nothing was changed, and nothing is overwritten. */
    UNREADABLE,
    KEYS_SAVED,
    KEYS_INVALID,
    LOGIN_SAVED,
    LOGIN_INVALID,
    SAS_SAVED,
    SAS_INVALID,
}

class CloudNotice(val kind: CloudNoticeKind, val service: CloudService?)

/**
 * The cloud accounts screen's state and actions (SWR-MG-20), over the app's one [CloudAuth]. One account per service: a service the person is
 * signed in to offers sign out, one whose sign-in the provider refused offers signing in again (which keeps the account), and signing in
 * twice to the same service is not offered. One action runs at a time ([busy]); a second request while one runs is ignored.
 *
 * A sign-in the person cancels says nothing: closing a browser is not an error. Every other failure is a [CloudNotice] of a kind, not a message.
 */
class CloudAccountsModel(
    private val auth: CloudAuth,
    /** Told after every action and refresh that changed the accounts, so what is built on them (the backup destination) is read again. */
    private val onChanged: suspend () -> Unit = {},
) {
    private val _rows = MutableStateFlow<List<CloudServiceRow>>(emptyList())
    val rows: StateFlow<List<CloudServiceRow>> get() = _rows

    private val _busy = MutableStateFlow<CloudService?>(null)

    /** The service an action is running for, or null. */
    val busy: StateFlow<CloudService?> get() = _busy

    private val _notice = MutableStateFlow<CloudNotice?>(null)
    val notice: StateFlow<CloudNotice?> get() = _notice

    fun dismissNotice() {
        _notice.value = null
    }

    /** Read the accounts again. An unreadable store leaves the rows as they were and says so. */
    suspend fun refresh() {
        val accounts = try {
            auth.accounts()
        } catch (e: CloudAuthException) {
            _notice.value = CloudNotice(CloudNoticeKind.UNREADABLE, null)
            if (_rows.value.isEmpty()) _rows.value = CloudService.values().map { CloudServiceRow(it, auth.isConfigured(it), null) }
            return
        }
        _rows.value = CloudService.values().map { service ->
            CloudServiceRow(service, auth.isConfigured(service), accounts.firstOrNull { it.service == service })
        }
    }

    suspend fun signIn(service: CloudService) {
        if (service.family == AccountFamily.AWS || service.family == AccountFamily.WEBDAV || service.family == AccountFamily.AZURE_SAS || !begin(service)) return
        try {
            val existing = _rows.value.firstOrNull { it.service == service }?.account
            if (existing != null && !existing.needsSignIn) return
            val account = auth.signIn(service, replacing = existing?.id)
            _notice.value = CloudNotice(saved(account, CloudNoticeKind.SIGNED_IN), service)
        } catch (e: CloudAuthException) {
            report(e, service)
        } finally {
            end()
        }
    }

    suspend fun signOut(service: CloudService) {
        val account = _rows.value.firstOrNull { it.service == service }?.account ?: return
        if (!begin(service)) return
        try {
            val kind = when (auth.signOut(account.id)) {
                Revocation.REVOKED -> CloudNoticeKind.SIGNED_OUT
                Revocation.NOT_AVAILABLE, Revocation.FAILED -> CloudNoticeKind.SIGNED_OUT_NOT_REVOKED
            }
            _notice.value = CloudNotice(kind, service)
        } catch (e: CloudAuthException) {
            report(e, service)
        } finally {
            end()
        }
    }

    /** Keep an AWS key pair the person typed. The caller empties its fields afterwards; nothing here keeps them. */
    suspend fun addAwsKeys(accessKeyId: String, secretAccessKey: String, sessionToken: String?) {
        val service = CloudService.AWS_S3
        if (!begin(service)) return
        try {
            val existing = _rows.value.firstOrNull { it.service == service }?.account
            val account = auth.addAwsKeys(accessKeyId.trim(), secretAccessKey.trim(), sessionToken?.trim()?.takeIf { it.isNotEmpty() }, replacing = existing?.id)
            _notice.value = CloudNotice(saved(account, CloudNoticeKind.KEYS_SAVED), service)
        } catch (e: CloudAuthException) {
            report(e, service)
        } finally {
            end()
        }
    }

    /** Keep a WebDAV login the person typed (SWR-MG-24). The caller empties its fields afterwards; nothing here keeps them. The password is not trimmed. */
    suspend fun addWebDavLogin(url: String, user: String, password: String) {
        val service = CloudService.WEBDAV
        if (!begin(service)) return
        try {
            val existing = _rows.value.firstOrNull { it.service == service }?.account
            val account = auth.addWebDavLogin(url.trim(), user.trim(), password, replacing = existing?.id)
            _notice.value = CloudNotice(saved(account, CloudNoticeKind.LOGIN_SAVED), service)
        } catch (e: CloudAuthException) {
            report(e, service)
        } finally {
            end()
        }
    }

    /** Keep an Azure shared access signature the person pasted (SWR-MG-26). The caller empties its field afterwards; nothing here keeps it. */
    suspend fun addAzureSas(sas: String) {
        val service = CloudService.AZURE_BLOB_SAS
        if (!begin(service)) return
        try {
            val existing = _rows.value.firstOrNull { it.service == service }?.account
            val account = auth.addAzureSas(sas, replacing = existing?.id)
            _notice.value = CloudNotice(saved(account, CloudNoticeKind.SAS_SAVED), service)
        } catch (e: CloudAuthException) {
            report(e, service)
        } finally {
            end()
        }
    }

    /** [plain] when nothing was replaced or the earlier credential was ended at the provider; otherwise the notice that says it may still work there. */
    private fun saved(account: CloudAccount, plain: CloudNoticeKind): CloudNoticeKind =
        if (account.replaced == null || account.replaced == Revocation.REVOKED) plain else CloudNoticeKind.REPLACED_NOT_REVOKED

    private fun begin(service: CloudService): Boolean {
        if (_busy.value != null) return false
        _busy.value = service
        _notice.value = null
        return true
    }

    private suspend fun end() {
        refresh()
        onChanged()
        _busy.value = null
    }

    private fun report(e: CloudAuthException, service: CloudService) {
        val kind = when (e.problem) {
            CloudAuthProblem.CANCELLED -> return // closing the browser is not a failure
            CloudAuthProblem.NOT_CONFIGURED -> CloudNoticeKind.NOT_CONFIGURED
            CloudAuthProblem.DENIED -> CloudNoticeKind.DENIED
            CloudAuthProblem.NETWORK -> CloudNoticeKind.NETWORK
            CloudAuthProblem.STORE_UNREADABLE -> CloudNoticeKind.UNREADABLE
            CloudAuthProblem.INVALID_INPUT -> when (service.family) {
                AccountFamily.AWS -> CloudNoticeKind.KEYS_INVALID
                AccountFamily.WEBDAV -> CloudNoticeKind.LOGIN_INVALID
                AccountFamily.AZURE_SAS -> CloudNoticeKind.SAS_INVALID
                else -> CloudNoticeKind.FAILED
            }
            CloudAuthProblem.STATE_MISMATCH, CloudAuthProblem.SIGN_IN_REQUIRED, CloudAuthProblem.PROVIDER, CloudAuthProblem.UNKNOWN_ACCOUNT -> CloudNoticeKind.FAILED
        }
        _notice.value = CloudNotice(kind, service)
    }
}
