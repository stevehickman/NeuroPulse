package life.neurone.shared.cloud

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import life.neurone.core.cloudauth.CloudAccount
import kotlinx.serialization.builtins.MapSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.json.Json
import life.neurone.core.cloudauth.BackupTarget
import life.neurone.core.cloudauth.CloudAuth
import life.neurone.core.cloudauth.CloudAuthException
import life.neurone.core.cloudauth.CloudService
import life.neurone.core.cloudauth.CloudStores
import life.neurone.core.cloudauth.TargetField
import life.neurone.core.common.KeyValueStore
import life.neurone.core.net.HttpTransport
import life.neurone.shared.MigrationPlace

/** The key that holds the cloud account a backup is kept in, per install (`np.backup.cloud-account` in the core's key map). Absent means this device. */
const val BACKUP_CLOUD_ACCOUNT_KEY = "np.backup.cloud-account"

/**
 * The bucket, container or region the backup is kept in, for a service that needs one (`np.backup.cloud-target`): a JSON object of field name to
 * value. Names only, never a credential. Per install, and never moved in a migration.
 */
const val BACKUP_CLOUD_TARGET_KEY = "np.backup.cloud-target"

/** A signed-in account that can keep a backup, as the destination row lists it. [fields] is what the person names for it (empty for Google Drive, OneDrive and iCloud). */
class CloudDestination(val accountId: String, val service: CloudService, val needsSignIn: Boolean, val fields: List<TargetField> = emptyList())

/**
 * Which cloud account, if any, the backup is kept in (SWR-MG-21), and the [MigrationPlace] it makes. **This device is the default and the only
 * thing that is ever chosen for the person**: a place is cloud only while the person's choice stands.
 *
 * - The choice is one account id, kept per install and never moved in a migration. It is dropped when the account is gone (signed out), and
 *   **not** when the provider has ended the sign-in: the place stays cloud, its calls fail with a sign-in status, and the row says to sign in
 *   again. Falling back to this device without a word would put a backup somewhere the person did not choose.
 * - Amazon S3, Google Cloud Storage and Azure Blob also need a [BackupTarget] the person types ([choose]); it is checked by [CloudStores.target] before it is
 *   kept, and kept beside the choice. A choice with no usable target is dropped, as a choice of an account that is gone is.
 * - The place is a store of one of the six services ([CloudStores]) with a route that promises no exclusive create, so it carries the
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

    // Declared before the first use: a property initialiser runs in order, and readTarget() below needs this one.
    private val mapSerializer = MapSerializer(String.serializer(), String.serializer())

    private val _target = MutableStateFlow(readTarget())

    /** The bucket or container named for the chosen account, or null when none is needed or none is chosen. */
    val target: StateFlow<BackupTarget?> get() = _target.asStateFlow()

    private val _place = MutableStateFlow<MigrationPlace?>(null)

    /** The place the choice makes, or null when this device is in use (or the build cannot make it). */
    val place: StateFlow<MigrationPlace?> get() = _place.asStateFlow()

    /** Read the accounts again and make the place for the choice. An unreadable store changes nothing. */
    suspend fun refresh() {
        val accounts = try {
            auth.accounts()
        } catch (e: CloudAuthException) {
            return
        }
        _destinations.value = accounts.map { CloudDestination(it.id, it.service, it.needsSignIn, CloudStores.targetFields(it.service)) }
        val chosen = _selected.value
        val account = chosen?.let { id -> accounts.firstOrNull { it.id == id } }
        val needsTarget = account != null && CloudStores.targetFields(account.service).isNotEmpty()
        if (chosen != null && (account == null || (needsTarget && _target.value == null))) {
            // The account is gone, or a service that needs a bucket has none: the choice goes, and the backup is kept on this device again.
            useDevice()
            return
        }
        _place.value = account?.let { place(it) }
    }

    /**
     * Keep the backup in [accountId], in the bucket or container [values] name when the service needs one. Returns false, and changes nothing, for an
     * account that is not listed, whose sign-in was ended (the person signs in again first), or for names [CloudStores.target] refuses.
     */
    suspend fun choose(accountId: String, values: Map<TargetField, String> = emptyMap()): Boolean {
        refresh()
        val d = _destinations.value.firstOrNull { it.accountId == accountId } ?: return false
        if (d.needsSignIn) return false
        val target = if (d.fields.isEmpty()) null else CloudStores.target(d.service, values) ?: return false
        _selected.value = accountId
        _target.value = target
        keys.putString(BACKUP_CLOUD_ACCOUNT_KEY, accountId)
        if (target == null) keys.remove(BACKUP_CLOUD_TARGET_KEY) else keys.putString(BACKUP_CLOUD_TARGET_KEY, encode(target))
        refresh()
        return true
    }

    /** Keep the backup on this device again. */
    fun useDevice() {
        _selected.value = null
        _target.value = null
        keys.remove(BACKUP_CLOUD_ACCOUNT_KEY)
        keys.remove(BACKUP_CLOUD_TARGET_KEY)
        _place.value = null
    }

    private fun encode(t: BackupTarget): String = Json.encodeToString(mapSerializer, t.values.mapKeys { it.key.name })

    /** The kept target, or null when there is none or it is not a map of known fields (an unreadable one reads as none and the choice is then dropped). */
    private fun readTarget(): BackupTarget? = try {
        keys.getString(BACKUP_CLOUD_TARGET_KEY)?.let { text ->
            val m = Json.decodeFromString(mapSerializer, text)
            BackupTarget(m.entries.associate { TargetField.valueOf(it.key) to it.value })
        }
    } catch (e: Exception) {
        null
    }

    private fun place(account: CloudAccount): MigrationPlace? = try {
        MigrationPlace(
            label = brand(account.service),
            store = CloudStores.storeFor(auth, transport, account, _target.value),
            route = CloudStores.routeFor(account.service),
            carriesRecordings = false,
        )
    } catch (e: CloudAuthException) {
        null // iCloud in a build with no container, or a bucket service with no usable target: nothing to keep a backup in
    }

    private fun brand(service: CloudService): String = when (service) {
        CloudService.GOOGLE_DRIVE -> "Google Drive"
        CloudService.ONEDRIVE -> "Microsoft OneDrive"
        CloudService.ICLOUD_CLOUDKIT -> "iCloud"
        CloudService.GOOGLE_CLOUD_STORAGE -> "Google Cloud Storage"
        CloudService.AZURE_BLOB -> "Azure Blob Storage"
        CloudService.AWS_S3 -> "Amazon S3"
    }
}
