package life.neurone.core.cloudauth

import life.neurone.core.cloud.CloudKitObjectStore
import life.neurone.core.cloud.GoogleDriveObjectStore
import life.neurone.core.cloud.OneDriveObjectStore
import life.neurone.core.migration.ObjectStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.migration.RouteDescriptor
import life.neurone.core.net.HttpTransport

/**
 * The object store a signed-in account keeps a backup in (SWR-MG-21): the service's adapter in `:cloudstore`, given the account's credential.
 * The credential is [CloudAuth]'s lazy one, so **every call asks for a token and gets a fresh one**: a store built when the person opens a
 * screen is still good an hour later, and a refused sign-in comes back as a 401 the flow reports as "route unavailable" ([CloudAuth.needsSignIn]).
 *
 * **Which services.** Only those that need nothing from the person beyond the sign-in: Google Drive (its application data folder), OneDrive
 * (its app folder) and iCloud (the container the build is registered with). Google Cloud Storage, Azure Blob and Amazon S3 also need a bucket or
 * container, a region or an account typed in and kept; they have an adapter and no way to be chosen yet.
 *
 * **What a route promises: no exclusive create** ([routeFor]), for every service. A migration relies on one (`SWR-MG-07`), and no
 * provider's has been checked against a real account (`SWR-MG-11` (contract) has no test), so the core withholds a cloud route from a
 * migration and offers it for the health-data backup only. Google Drive could not promise one in any case (its adapter's create always throws).
 */
object CloudStores {
    // PROVISIONAL (UC-115): no measurement of a bundle's size or of a mobile uplink; the adapters leave the figure to the host.
    /** How long one call to a cloud service may take, in seconds. */
    const val CALL_TIMEOUT_SECONDS: Int = 120

    private val keepsBackups = setOf(CloudService.GOOGLE_DRIVE, CloudService.ONEDRIVE, CloudService.ICLOUD_CLOUDKIT)

    /** Whether a backup can be kept in [service] with nothing but the sign-in. */
    fun canKeepBackups(service: CloudService): Boolean = service in keepsBackups

    /** What a cloud route promises the core: nothing about an exclusive create, so it carries a backup and never a migration. */
    fun routeFor(service: CloudService): RouteDescriptor {
        require(canKeepBackups(service)) { "$service cannot keep a backup with the sign-in alone" }
        return ObjectStoreRoute("cloud-" + service.name.lowercase().replace('_', '-'), atomicClaim = false)
    }

    /** The store for [account], over [transport]. Throws [CloudAuthException] (NOT_CONFIGURED) for iCloud when the build has no container. */
    fun storeFor(auth: CloudAuth, transport: HttpTransport, account: CloudAccount): ObjectStore {
        require(canKeepBackups(account.service)) { "${account.service} cannot keep a backup with the sign-in alone" }
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
            else -> error("unreachable: ${account.service}")
        }
    }
}
