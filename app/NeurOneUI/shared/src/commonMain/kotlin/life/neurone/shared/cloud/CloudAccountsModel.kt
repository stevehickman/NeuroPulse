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
    NOT_CONFIGURED,
    DENIED,
    FAILED,
    NETWORK,

    /** The stored sign-ins cannot be read on this device. Nothing was changed, and nothing is overwritten. */
    UNREADABLE,
    KEYS_SAVED,
    KEYS_INVALID,
}

class CloudNotice(val kind: CloudNoticeKind, val service: CloudService?)

/**
 * The cloud accounts screen's state and actions (SWR-MG-20), over the app's one [CloudAuth]. One account per service: a service the person is
 * signed in to offers sign out, one whose sign-in the provider refused offers signing in again (which keeps the account), and signing in
 * twice to the same service is not offered. One action runs at a time ([busy]); a second request while one runs is ignored.
 *
 * A sign-in the person cancels says nothing: closing a browser is not an error. Every other failure is a [CloudNotice] of a kind, not a message.
 */
class CloudAccountsModel(private val auth: CloudAuth) {
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
        if (service.family == AccountFamily.AWS || !begin(service)) return
        try {
            val existing = _rows.value.firstOrNull { it.service == service }?.account
            if (existing != null && !existing.needsSignIn) return
            auth.signIn(service, replacing = existing?.id)
            _notice.value = CloudNotice(CloudNoticeKind.SIGNED_IN, service)
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
            auth.addAwsKeys(accessKeyId.trim(), secretAccessKey.trim(), sessionToken?.trim()?.takeIf { it.isNotEmpty() }, replacing = existing?.id)
            _notice.value = CloudNotice(CloudNoticeKind.KEYS_SAVED, service)
        } catch (e: CloudAuthException) {
            report(e, service)
        } finally {
            end()
        }
    }

    private fun begin(service: CloudService): Boolean {
        if (_busy.value != null) return false
        _busy.value = service
        _notice.value = null
        return true
    }

    private suspend fun end() {
        refresh()
        _busy.value = null
    }

    private fun report(e: CloudAuthException, service: CloudService) {
        val kind = when (e.problem) {
            CloudAuthProblem.CANCELLED -> return // closing the browser is not a failure
            CloudAuthProblem.NOT_CONFIGURED -> CloudNoticeKind.NOT_CONFIGURED
            CloudAuthProblem.DENIED -> CloudNoticeKind.DENIED
            CloudAuthProblem.NETWORK -> CloudNoticeKind.NETWORK
            CloudAuthProblem.STORE_UNREADABLE -> CloudNoticeKind.UNREADABLE
            CloudAuthProblem.INVALID_INPUT -> if (service.family == AccountFamily.AWS) CloudNoticeKind.KEYS_INVALID else CloudNoticeKind.FAILED
            CloudAuthProblem.STATE_MISMATCH, CloudAuthProblem.SIGN_IN_REQUIRED, CloudAuthProblem.PROVIDER, CloudAuthProblem.UNKNOWN_ACCOUNT -> CloudNoticeKind.FAILED
        }
        _notice.value = CloudNotice(kind, service)
    }
}
