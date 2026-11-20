package life.neurone.core.cloudauth

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import life.neurone.core.common.EncryptedKeyValueStore
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

    /** Whether [other] holds the same typed credential (AWS keys, a WebDAV login, an Azure signature) as this record. */
    fun sameTypedCredential(other: StoredAccount): Boolean =
        awsAccessKeyId == other.awsAccessKeyId && awsSecretAccessKey == other.awsSecretAccessKey && awsSessionToken == other.awsSessionToken &&
            webdavUrl == other.webdavUrl && webdavUser == other.webdavUser && webdavPassword == other.webdavPassword && azureSas == other.azureSas

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
 * The person's cloud credentials on this install: one JSON document under [KEY] in the app's protected storage. [KEY] is in the
 * `perInstall` list of `common/npps/migration-keys.json`, so it is sealed on every host that seals and **never moves in a migration**:
 * a new phone signs in again, and a refresh token is not copied between devices (SWR-MG-16, SWR-MG-01).
 *
 * **There is no way to build one over a store that would keep the document in the clear** (`SWR-MG-19` (l), `OI-PRIV-14`). The
 * constructor is private; [over] takes the run's [ProtectedStorage] and answers by its state:
 * - [StorageProtection.SEALED], with [KEY] on the sealed list: the vault, sealed at rest ([durable] is true).
 * - [StorageProtection.LOCKED]: the vault for this run only. The protected store holds [KEY] in memory and writes nothing to it on
 *   disk, so the accounts an earlier run sealed are not readable now (they read as none, and are untouched), and an account signed in
 *   to now is forgotten when the app closes. [durable] is false.
 * - [StorageProtection.UNSEALED], or a store that does not protect [KEY]: **null. There is no vault and so no cloud sign-in**, and a
 *   document an earlier run left in the clear is removed from the host's store ([removePlainRemnant]).
 *
 * A document that cannot be read is never replaced: [load] throws [CloudAuthProblem.STORE_UNREADABLE], so one corrupt value cannot be
 * overwritten by the next sign-in and take the other accounts with it. (A value that is *absent* reads as no accounts; the protected store
 * also answers absent for a value sealed under a key that no longer exists, which is its documented behaviour and cannot be told apart here.)
 */
class CredentialStore private constructor(
    private val store: KeyValueStore,
    /** Whether what is saved outlives this run: true when sealed at rest, false when the run holds it in memory only. */
    val durable: Boolean,
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

    /**
     * Forget every account and credential on this install. **It revokes nothing**, so it is not called on its own: `CloudAuth.signOutAll` reads
     * the credentials, calls this, and then asks each provider to end its grant (SWR-MG-30 (d)).
     */
    internal fun clear() {
        store.remove(KEY)
    }

    companion object {
        /** In `perInstall` in `common/npps/migration-keys.json`. */
        const val KEY = "np.cloud.accounts"

        /**
         * The vault over this run's protected storage, or null when that storage would keep it in the clear (see the class). On the
         * null answer a plain document is removed from [ProtectedStorage.store].
         */
        fun over(storage: ProtectedStorage): CredentialStore? {
            if (storage.keepsFromTheClear(KEY)) return CredentialStore(storage.store, durable = storage.state == StorageProtection.SEALED)
            removePlainRemnant(storage.store)
            return null
        }

        /** A vault that lives in memory and is gone with the object: for a caller that must never keep a credential, and for tests of the flows. */
        fun inMemory(): CredentialStore = CredentialStore(InMemoryKeyValueStore(), durable = false)

        /**
         * Removes a credential document an earlier run left in the clear in [host], which is a host store nothing seals on this run.
         * A value that carries the sealing prefix is left alone: it was sealed by a run that could, it is not readable here, and a
         * later run that can open it must find it. Removing a plain one is an erase, not a weakening: the credentials are of no use
         * to a run with no vault, and the person signs in again on a run that has one.
         */
        internal fun removePlainRemnant(host: KeyValueStore) {
            val stored = try { host.getString(KEY) } catch (ignored: ClassCastException) { null } ?: return
            if (!stored.startsWith(EncryptedKeyValueStore.PREFIX)) host.remove(KEY)
        }
    }
}
