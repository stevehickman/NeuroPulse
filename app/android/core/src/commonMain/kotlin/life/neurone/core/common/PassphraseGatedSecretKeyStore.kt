package life.neurone.core.common

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecretResult
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.npps.NppsCore

// The passphrase fallback of the UHDR presence gate (NP-APP-PRESENCE-001 ruling 4, REQ-PRES-06): for a host with no device credential
// to bind a key to. Every primitive is the shared Rust core's (CLAUDE.md §22): the passphrase becomes a secret (NFKC, length check),
// the secret becomes a key by the core's Argon2id at the fixed bundle cost, and the key seals the data key with the core's
// AES-256-GCM value sealing. This file only orders those calls and keeps the result.

/** What a passphrase attempt came to. */
enum class PassphraseResult {
    OK,

    /** The passphrase does not open the data key. Nothing changed. */
    WRONG,

    /** Shorter than the core's minimum (`BundleKeyDerivation.passphraseSecret`). Nothing changed. */
    TOO_SHORT,

    /** The gate is not set up, is already set up, or the core refused. Nothing changed. */
    FAILED,
}

/** The shortest passphrase the core accepts, asked of the core so no screen carries a second copy of the figure. */
fun passphraseMinChars(): Int =
    (BundleKeyDerivation.passphraseSecret("") as? BundleSecretResult.Rejected)?.minChars ?: 12

/**
 * A [GatedSecretKeyStore] whose presence is a passphrase the person knows. The screens ask for it themselves (`unlock(reason)` has no
 * way to), so the lock is opened with [PresenceLock.unlockWithPassphrase].
 */
interface PassphraseKey : GatedSecretKeyStore {
    /** Open the data key with [passphrase]. [PassphraseResult.OK] leaves the key unlocked. */
    suspend fun unlockWithPassphrase(passphrase: String): PassphraseResult

    /** Set the gate up with [passphrase]: a fresh data key, wrapped under it. Refuses when one is already set. */
    suspend fun setPassphrase(passphrase: String): PassphraseResult

    /** Wrap the same data key under [new] after [old] opened it, so nothing sealed has to be sealed again. */
    suspend fun changePassphrase(old: String, new: String): PassphraseResult
}

/**
 * Keeps the data key wrapped under a passphrase in [host]'s plain store (the wrapped bytes and the Argon2 salt are not secret). The
 * wrapping key is derived again at each unlock and is never kept; the data key is held in memory until [lock]. A wrong passphrase
 * fails the AES-GCM tag and is `WRONG`; it is never taken for "no key" (`SWR-MG-15`). Forgetting the passphrase is unrecoverable by
 * design, which is what the backup the app insists on first is for (`REQ-PRES-08`).
 *
 * **Not offered yet.** Moving an install that already holds sealed UHDR to this key is `REQ-PRES-07` (increment 7); no host passes
 * this store to the shared app until it exists.
 */
class PassphraseGatedSecretKeyStore(
    private val host: KeyValueStore,
    private val random: RandomSource = SystemRandomSource,
) : PassphraseKey {
    private var dataKey: ByteArray? = null
    private var unlocked = false

    override val isUnlocked: Boolean get() = unlocked

    /** On once a passphrase has been set. */
    override val isEnabled: Boolean get() = host.getString(WRAP_KEY) != null

    override suspend fun enable(reason: String): Boolean = false

    override suspend fun disable(): Boolean = false

    override suspend fun unlock(reason: String): Boolean = unlocked

    override fun lock() {
        dataKey?.fill(0)
        dataKey = null
        unlocked = false
    }

    override fun load(): ByteArray? {
        if (!unlocked) throw SecretKeyUnavailableException("the person's presence has not been established")
        return dataKey?.copyOf()
    }

    override fun store(key: ByteArray) {
        // The data key is made when the passphrase is set; a store would need the passphrase again to wrap another one.
        throw SecretKeyUnavailableException("the data key is made when the passphrase is set")
    }

    override fun destroy() {
        host.remove(WRAP_KEY)
        lock()
    }

    override suspend fun unlockWithPassphrase(passphrase: String): PassphraseResult {
        if (unlocked) return PassphraseResult.OK
        val record = readRecord() ?: return PassphraseResult.FAILED
        val key = open(passphrase, record)
        return when (key) {
            is Opened.Key -> { dataKey = key.bytes; unlocked = true; PassphraseResult.OK }
            is Opened.Refused -> key.result
        }
    }

    override suspend fun setPassphrase(passphrase: String): PassphraseResult {
        if (host.getString(WRAP_KEY) != null) return PassphraseResult.FAILED
        val fresh = random.nextBytes(DATA_KEY_BYTES)
        val result = wrap(passphrase, fresh)
        if (result == PassphraseResult.OK) { dataKey = fresh.copyOf(); unlocked = true }
        fresh.fill(0)
        return result
    }

    override suspend fun changePassphrase(old: String, new: String): PassphraseResult {
        val record = readRecord() ?: return PassphraseResult.FAILED
        return when (val opened = open(old, record)) {
            is Opened.Refused -> opened.result
            is Opened.Key -> try { wrap(new, opened.bytes) } finally { opened.bytes.fill(0) }
        }
    }

    private sealed interface Opened {
        class Key(val bytes: ByteArray) : Opened
        class Refused(val result: PassphraseResult) : Opened
    }

    private class Record(val kdf: JsonObject, val sealed: String)

    private fun readRecord(): Record? = runCatching {
        val o = Json.parseToJsonElement(host.getString(WRAP_KEY) ?: return null).jsonObject
        Record(o["kdf"]!!.jsonObject, o["sealed"]!!.jsonPrimitive.content)
    }.getOrNull()

    /** The wrapping key for [passphrase] under [kdf], as hex, or the reason it cannot be had. */
    private fun wrappingKey(passphrase: String, kdf: JsonObject): Pair<String?, PassphraseResult> {
        val secret = when (val s = BundleKeyDerivation.passphraseSecret(passphrase)) {
            is BundleSecretResult.Ok -> s.secret
            is BundleSecretResult.Rejected -> return null to (if (s.reason == "too_short") PassphraseResult.TOO_SHORT else PassphraseResult.FAILED)
        }
        return try {
            val key = BundleKeyDerivation.deriveKey(secret, kdf)
            try { key.hex() to PassphraseResult.OK } finally { key.destroy() }
        } catch (ex: IllegalArgumentException) {
            null to PassphraseResult.FAILED
        } finally {
            secret.destroy()
        }
    }

    private fun open(passphrase: String, record: Record): Opened {
        val (keyHex, status) = wrappingKey(passphrase, record.kdf)
        if (keyHex == null) return Opened.Refused(status)
        val r = NppsCore.migration(buildJsonObject {
            put("op", "openValue"); put("key", keyHex); put("name", WRAP_NAME); put("sealed", record.sealed)
        })
        if (!r["ok"]!!.jsonPrimitive.boolean) {
            return Opened.Refused(if (r["reason"]?.jsonPrimitive?.contentOrNull == "authentication_failed") PassphraseResult.WRONG else PassphraseResult.FAILED)
        }
        val bytes = runCatching { r["value"]!!.jsonPrimitive.content.hexBytes() }.getOrNull()
        return if (bytes != null && bytes.size == DATA_KEY_BYTES) Opened.Key(bytes) else Opened.Refused(PassphraseResult.FAILED)
    }

    /** Wrap [data] under a new salt and [passphrase], and keep the record. Nothing is written unless every step worked. */
    private fun wrap(passphrase: String, data: ByteArray): PassphraseResult {
        val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.PASSPHRASE, random)
        val (keyHex, status) = wrappingKey(passphrase, kdf)
        if (keyHex == null) return status
        val nonce = random.nextBytes(NONCE_BYTES)
        val r = NppsCore.migration(buildJsonObject {
            put("op", "sealValue"); put("key", keyHex); put("nonce", nonce.hexString()); put("name", WRAP_NAME); put("value", data.hexString())
        })
        val sealed = r["sealed"]?.jsonPrimitive?.contentOrNull ?: return PassphraseResult.FAILED
        host.putString(WRAP_KEY, buildJsonObject { put("kdf", kdf); put("sealed", sealed) }.toString())
        return PassphraseResult.OK
    }

    private fun ByteArray.hexString() = joinToString("") { (it.toInt() and 0xff).toString(16).padStart(2, '0') }
    private fun String.hexBytes() = ByteArray(length / 2) { i -> substring(2 * i, 2 * i + 2).toInt(16).toByte() }

    companion object {
        /** The plain host key holding the Argon2 parameters and the sealed data key. Not UHDR: it is read before anyone is present. */
        const val WRAP_KEY = "np.presence.passphrase-wrap"

        /** The associated data of the sealing, so the wrapped key opens only as this record. */
        private const val WRAP_NAME = "np.presence.passphrase-wrap"
        private const val DATA_KEY_BYTES = 32
        private const val NONCE_BYTES = 12
    }
}
