package life.neurone.core.cloudauth

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import life.neurone.core.common.KeyValueStore

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
        awsAccessKeyId, awsSecretAccessKey, awsSessionToken, awsExpiresAt, webdavUrl, webdavUser, webdavPassword,
    )
}

@Serializable
internal class Vault(val version: Int = 1, val accounts: List<StoredAccount> = emptyList())

/**
 * The person's cloud credentials on this install: one JSON document under [KEY] in the app's key-value store. **The store handed in
 * must be the protected one** (`EncryptedKeyValueStore` over the host's store), which seals every key in `common/npps/migration-keys.json`,
 * and [KEY] is in that file's `perInstall` list, so it is sealed on every host that seals and **never moves in a migration**: a new
 * phone signs in again, and a refresh token is not copied between devices (SWR-MG-16, SWR-MG-01).
 *
 * A document that cannot be read is never replaced: [load] throws [CloudAuthProblem.STORE_UNREADABLE], so one corrupt value cannot be
 * overwritten by the next sign-in and take the other accounts with it. (A value that is *absent* reads as no accounts; the protected store
 * also answers absent for a value sealed under a key that no longer exists, which is its documented behaviour and cannot be told apart here.)
 */
class CredentialStore(private val store: KeyValueStore) {
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
        /** In `perInstall` in `common/npps/migration-keys.json`. */
        const val KEY = "np.cloud.accounts"
    }
}
