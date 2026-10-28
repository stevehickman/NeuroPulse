package life.neurone.core.cloudauth

import kotlinx.datetime.Instant
import life.neurone.core.cloud.AzureBlobObjectStore
import life.neurone.core.cloud.CloudKitObjectStore
import life.neurone.core.cloud.GcsObjectStore
import life.neurone.core.cloud.GoogleDriveObjectStore
import life.neurone.core.cloud.NppsAwsSigV4
import life.neurone.core.cloud.OneDriveObjectStore
import life.neurone.core.cloud.S3ObjectStore
import life.neurone.core.migration.ObjectStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.migration.RouteDescriptor
import life.neurone.core.net.HttpTransport
import life.neurone.core.platform.epochSeconds

/**
 * The object store a signed-in account keeps a backup in (SWR-MG-21): the service's adapter in `:cloudstore`, given the account's credential.
 * The credential is [CloudAuth]'s lazy one, so **every call asks for a token and gets a fresh one**: a store built when the person opens a
 * screen is still good an hour later, and a refused sign-in comes back as a 401 the flow reports as "route unavailable" ([CloudAuth.needsSignIn]).
 *
 * **Which services.** All six. Google Drive, OneDrive and iCloud need nothing beyond the sign-in. Amazon S3, Google Cloud Storage and Azure Blob
 * also need a place the person names ([BackupTarget]: bucket and region, bucket, or storage account and container), which [targetFields] lists
 * and [target] checks. The names go into a host name or a path, so a value that is not a plain bucket, region, account or container name is refused
 * here, before it can reach a URL.
 *
 * **What a route promises: no exclusive create** ([routeFor]), for every service. A migration relies on one (`SWR-MG-07`), and no
 * provider's has been checked against a real account (`SWR-MG-11` (contract) has no test), so the core withholds a cloud route from a
 * migration and offers it for the health-data backup only. Google Drive could not promise one in any case (its adapter's create always throws).
 */
object CloudStores {
    // PROVISIONAL (UC-115): no measurement of a bundle's size or of a mobile uplink; the adapters leave the figure to the host.
    /** How long one call to a cloud service may take, in seconds. */
    const val CALL_TIMEOUT_SECONDS: Int = 120

    /** What a service needs the person to name, in the order the form asks. Empty for a service that needs nothing beyond the sign-in. */
    fun targetFields(service: CloudService): List<TargetField> = when (service) {
        CloudService.AWS_S3 -> listOf(TargetField.BUCKET, TargetField.REGION)
        CloudService.GOOGLE_CLOUD_STORAGE -> listOf(TargetField.GCS_BUCKET)
        CloudService.AZURE_BLOB -> listOf(TargetField.STORAGE_ACCOUNT, TargetField.CONTAINER)
        else -> emptyList()
    }

    /**
     * The target for [service] from what the person typed, or null when a field is missing or is not a plain name. The patterns are the providers'
     * naming rules as this code knows them (S3 buckets: 3 to 63 of lowercase letters, digits, dots and hyphens; Google Cloud Storage buckets: 3 to 222
     * with underscores too; Azure storage accounts: 3 to 24 lowercase letters and digits; Azure containers: 3 to 63 of lowercase letters, digits and
     * single hyphens), kept stricter than the provider where it is cheaper to refuse than to explain. They are not re-read against the providers' pages,
     * and the provider is what refuses a name that matches and does not exist.
     */
    fun target(service: CloudService, values: Map<TargetField, String>): BackupTarget? {
        val fields = targetFields(service)
        val clean = fields.associateWith { (values[it] ?: return null).trim() }
        if (clean.any { (f, v) -> !f.accepts(v) }) return null
        return BackupTarget(clean)
    }

    /** What a cloud route promises the core: nothing about an exclusive create, so it carries a backup and never a migration. */
    fun routeFor(service: CloudService): RouteDescriptor =
        ObjectStoreRoute("cloud-" + service.name.lowercase().replace('_', '-'), atomicClaim = false)

    /**
     * The store for [account], over [transport]. Throws [CloudAuthException] (NOT_CONFIGURED) for iCloud when the build has no container, and
     * (INVALID_INPUT) for a service that needs a [target] when [target] does not hold what [targetFields] lists.
     */
    fun storeFor(auth: CloudAuth, transport: HttpTransport, account: CloudAccount, target: BackupTarget? = null): ObjectStore {
        fun need(f: TargetField): String = target?.values?.get(f)?.takeIf { f.accepts(it) }
            ?: throw CloudAuthException(CloudAuthProblem.INVALID_INPUT, "${account.service} needs a valid ${f.name.lowercase()}")
        return when (account.service) {
            CloudService.GOOGLE_DRIVE -> GoogleDriveObjectStore(transport, auth.accessToken(account.id), CALL_TIMEOUT_SECONDS)
            CloudService.ONEDRIVE -> OneDriveObjectStore(transport, auth.accessToken(account.id), CALL_TIMEOUT_SECONDS)
            CloudService.ICLOUD_CLOUDKIT -> {
                val c = auth.cloudKitConfig ?: throw CloudAuthException(CloudAuthProblem.NOT_CONFIGURED, "no CloudKit container is configured")
                CloudKitObjectStore(
                    transport, c.container, auth.cloudKitCredentials(account.id), CALL_TIMEOUT_SECONDS,
                    environment = c.environment, baseUrl = c.baseUrl, onWebAuthToken = auth.cloudKitTokenSink(account.id),
                )
            }
            CloudService.GOOGLE_CLOUD_STORAGE ->
                GcsObjectStore(transport, need(TargetField.GCS_BUCKET), auth.accessToken(account.id), CALL_TIMEOUT_SECONDS)
            CloudService.AZURE_BLOB ->
                AzureBlobObjectStore(transport, need(TargetField.STORAGE_ACCOUNT), need(TargetField.CONTAINER), auth.accessToken(account.id), CALL_TIMEOUT_SECONDS)
            CloudService.AWS_S3 -> {
                val bucket = need(TargetField.BUCKET)
                S3ObjectStore(
                    transport, bucket, need(TargetField.REGION), auth.awsCredentials(account.id), NppsAwsSigV4(),
                    { NppsAwsSigV4.amzDate(Instant.fromEpochSeconds(epochSeconds())) }, CALL_TIMEOUT_SECONDS,
                    pathStyle = '.' in bucket, // a dotted bucket name does not match the virtual-hosted TLS name
                )
            }
        }
    }
}

/** What the person names for a service that keeps a backup in a bucket or container. */
enum class TargetField(private val pattern: Regex) {
    /** An Amazon S3 bucket. */
    BUCKET(Regex("[a-z0-9][a-z0-9.-]{1,61}[a-z0-9]")),

    /** A Google Cloud Storage bucket, which may be longer and may hold an underscore. */
    GCS_BUCKET(Regex("[a-z0-9][a-z0-9._-]{1,220}[a-z0-9]")),
    REGION(Regex("[a-z]{2}(-[a-z0-9]+){1,3}")),
    STORAGE_ACCOUNT(Regex("[a-z0-9]{3,24}")),
    CONTAINER(Regex("[a-z0-9](?:[a-z0-9]|-(?!-)){1,61}[a-z0-9]"));

    /** Whether [value] is a plain name of this kind. Nothing in it can add a path, a query or a host. */
    fun accepts(value: String): Boolean = pattern.matches(value)
}

/** Where a service keeps the backup: one value per field the service asks for ([CloudStores.targetFields]). */
class BackupTarget(val values: Map<TargetField, String>)
