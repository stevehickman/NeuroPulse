package life.neurone.shared.cloud

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import life.neurone.core.cloudauth.CloudAccount
import life.neurone.core.cloudauth.CloudAuth
import life.neurone.core.cloudauth.CloudAuthException
import life.neurone.core.cloudauth.CloudService
import life.neurone.core.cloudauth.CloudStores
import life.neurone.core.common.KeyValueStore
import life.neurone.core.net.HttpTransport
import life.neurone.shared.MigrationPlace

/** The key that holds the cloud account a backup is kept in, per install (`np.backup.cloud-account` in the core's key map). Absent means this device. */
const val BACKUP_CLOUD_ACCOUNT_KEY = "np.backup.cloud-account"

/** A signed-in account that can keep a backup, as the destination row lists it. */
class CloudDestination(val accountId: String, val service: CloudService, val needsSignIn: Boolean)

/**
 * Which cloud account, if any, the backup is kept in (SWR-MG-21), and the [MigrationPlace] it makes. **This device is the default and the only
 * thing that is ever chosen for the person**: a place is cloud only while the person's choice stands.
 *
 * - The choice is one account id, kept per install and never moved in a migration. It is dropped when the account is gone (signed out), and
 *   **not** when the provider has ended the sign-in: the place stays cloud, its calls fail with a sign-in status, and the row says to sign in
 *   again. Falling back to this device without a word would put a backup somewhere the person did not choose.
 * - The place is a Google Drive, OneDrive or iCloud store ([CloudStores]) with a route that promises no exclusive create, so it carries the
 *   health-data backup and never a migration, and it carries no recordings ([MigrationPlace.carriesRecordings]).
 */
class CloudBackupModel(
    private val auth: CloudAuth,
    private val transport: HttpTransport,
    private val keys: KeyValueStore,
) {
    private val _destinations = MutableStateFlow<List<CloudDestination>>(emptyList())

    /** The signed-in accounts that can keep a backup. */
    val destinations: StateFlow<List<CloudDestination>> get() = _destinations.asStateFlow()

    private val _selected = MutableStateFlow(keys.getString(BACKUP_CLOUD_ACCOUNT_KEY)?.takeIf { it.isNotBlank() })

    /** The account chosen, or null for this device. */
    val selected: StateFlow<String?> get() = _selected.asStateFlow()

    private val _place = MutableStateFlow<MigrationPlace?>(null)

    /** The place the choice makes, or null when this device is in use (or the build cannot make it). */
    val place: StateFlow<MigrationPlace?> get() = _place.asStateFlow()

    /** Read the accounts again and make the place for the choice. An unreadable store changes nothing. */
    suspend fun refresh() {
        val accounts = try {
            auth.accounts()
        } catch (e: CloudAuthException) {
            return
        }.filter { CloudStores.canKeepBackups(it.service) }
        _destinations.value = accounts.map { CloudDestination(it.id, it.service, it.needsSignIn) }
        val chosen = _selected.value
        val account = chosen?.let { id -> accounts.firstOrNull { it.id == id } }
        if (chosen != null && account == null) {
            // The account is gone: the choice goes with it, and the backup is kept on this device again.
            _selected.value = null
            keys.remove(BACKUP_CLOUD_ACCOUNT_KEY)
        }
        _place.value = account?.let { place(it) }
    }

    /** Keep the backup in [accountId]. Refused for an account that is not listed or whose sign-in was ended: the person signs in again first. */
    suspend fun choose(accountId: String) {
        refresh()
        val d = _destinations.value.firstOrNull { it.accountId == accountId } ?: return
        if (d.needsSignIn) return
        _selected.value = accountId
        keys.putString(BACKUP_CLOUD_ACCOUNT_KEY, accountId)
        refresh()
    }

    /** Keep the backup on this device again. */
    fun useDevice() {
        _selected.value = null
        keys.remove(BACKUP_CLOUD_ACCOUNT_KEY)
        _place.value = null
    }

    private fun place(account: CloudAccount): MigrationPlace? = try {
        MigrationPlace(
            label = brand(account.service),
            store = CloudStores.storeFor(auth, transport, account),
            route = CloudStores.routeFor(account.service),
            carriesRecordings = false,
        )
    } catch (e: CloudAuthException) {
        null // iCloud in a build with no container: nothing to keep a backup in
    }

    private fun brand(service: CloudService): String = when (service) {
        CloudService.GOOGLE_DRIVE -> "Google Drive"
        CloudService.ONEDRIVE -> "Microsoft OneDrive"
        CloudService.ICLOUD_CLOUDKIT -> "iCloud"
        else -> service.name
    }
}
