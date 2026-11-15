package life.neurone.core.cloudauth

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.ProtectedStorage
import life.neurone.core.common.StorageProtection

/**
 * One account's record, as stored. **Every field here is a credential or is stored beside one**, which is why the whole document
 * is one sealed value and not several. A field is null unless its service uses it.
 */
@Serializable
internal class StoredAccount(
    val id: String,
    val service: CloudService,
    @SerialName("createdAt") val createdAtEpochSeconds: Long,
    @SerialName("needsSignIn") val needsSignIn: Boolean = false,
    @SerialName("accessToken") val accessToken: String? = null,
    @SerialName("accessTokenExpiresAt") val accessTokenExpiresAt: Long = 0,
    @SerialName("refreshToken") val refreshToken: String? = null,
    @SerialName("webAuthToken") val webAuthToken: String? = null,
    @SerialName("awsAccessKeyId") val awsAccessKeyId: String? = null,
    @SerialName("awsSecretAccessKey") val awsSecretAccessKey: String? = null,
    @SerialName("awsSessionToken") val awsSessionToken: String? = null,
    @SerialName("awsExpiresAt") val awsExpiresAt: Long = 0,
    @SerialName("webdavUrl") val webdavUrl: String? = null,
    @SerialName("webdavUser") val webdavUser: String? = null,
    @SerialName("webdavPassword") val webdavPassword: String? = null,
    @SerialName("azureSas") val azureSas: String? = null,
    @SerialName("azureSasExpiresAt") val azureSasExpiresAt: Long = 0,
) {
    fun view() = CloudAccount(id, service, createdAtEpochSeconds, needsSignIn)

    fun copyWith(
        needsSignIn: Boolean = this.needsSignIn,
        accessToken: String? = this.accessToken,
        accessTokenExpiresAt: Long = this.accessTokenExpiresAt,
        refreshToken: String? = this.refreshToken,
        webAuthToken: String? = this.webAuthToken,
    ) = StoredAccount(
        id, service, createdAtEpochSeconds, needsSignIn, accessToken, accessTokenExpiresAt, refreshToken, webAuthToken,
        awsAccessKeyId, awsSecretAccessKey, awsSessionToken, awsExpiresAt, webdavUrl, webdavUser, webdavPassword, azureSas, azureSasExpiresAt,
    )
}

@Serializable
internal class Vault(val version: Int = 1, val accounts: List<StoredAccount> = emptyList())

/**
 * The person's cloud credentials on this install: one JSON document under [KEY]. **It is written to disk only through a store that seals
 * it, and that is a type, not a comment** (`SWR-MG-28`, `OI-PRIV-14`): the only way to get one is [open], which takes the app's
 * [ProtectedStorage] and answers by its state.
 *
 * - **[StorageProtection.SEALED]**, and the storage seals [KEY]: the vault is kept in that store, sealed at rest. [KEY] is in the
 *   `perInstall` list of `common/npps/migration-keys.json`, so it **never moves in a migration**: a new phone signs in again, and a
 *   refresh token is not copied between devices (SWR-MG-16, SWR-MG-01).
 * - **[StorageProtection.LOCKED]**: the vault is held in memory for this run and nothing is written. The accounts an earlier run sealed
 *   cannot be read and are left exactly as they are; a sign-in made in this run is forgotten when it ends, as everything else done under
 *   the protected keys in a locked run is.
 * - **[StorageProtection.UNSEALED]**, or a sealed storage whose key list does not hold [KEY]: there is no vault ([open] answers null) and
 *   the host offers no cloud accounts. A refresh token, an AWS secret, a WebDAV password or an Azure signature is never written to a
 *   host store in the clear, and there is no fallback that does.
 *
 * A document that cannot be read is never replaced: [load] throws [CloudAuthProblem.STORE_UNREADABLE], so one corrupt value cannot be
 * overwritten by the next sign-in and take the other accounts with it. (A value that is *absent* reads as no accounts; the protected store
 * also answers absent for a value sealed under a key that no longer exists, which is its documented behaviour and cannot be told apart here.)
 */
class CredentialStore private constructor(
    private val store: KeyValueStore,
    /** Whether the accounts outlive this run: false for the memory-only vault of a locked run. */
    val keptAcrossRuns: Boolean,
) {
    private val json = Json { ignoreUnknownKeys = true }

    internal fun load(): Vault {
        val text = store.getString(KEY) ?: return Vault()
        return try {
            json.decodeFromString(Vault.serializer(), text)
        } catch (e: Exception) {
            throw CloudAuthException(CloudAuthProblem.STORE_UNREADABLE, "the stored cloud accounts cannot be read")
        }
    }

    internal fun save(vault: Vault) {
        store.putString(KEY, json.encodeToString(Vault.serializer(), vault))
    }

    /** Forget every account and credential (an erase, SWR-MG-17, or a person leaving every cloud store). */
    fun clear() {
        store.remove(KEY)
    }

    companion object {
        /** The vault for [storage], or null where the credentials could only be kept in the clear (see the class comment). */
        fun open(storage: ProtectedStorage): CredentialStore? = when (storage.state) {
            StorageProtection.SEALED -> if (storage.seals(KEY)) CredentialStore(storage.store, keptAcrossRuns = true) else null
            StorageProtection.LOCKED -> inMemory()
            StorageProtection.UNSEALED -> null
        }

        /** A vault that is never written anywhere: what a locked run uses, and what a test that needs no disk uses. */
        fun inMemory(): CredentialStore = CredentialStore(InMemoryKeyValueStore(), keptAcrossRuns = false)

        /** In `perInstall` in `common/npps/migration-keys.json`. */
        const val KEY = "np.cloud.accounts"
    }
}
