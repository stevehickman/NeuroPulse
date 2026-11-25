package life.neurone.core.common

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.BundleKey
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
//
// The record is sealed twice (`REQ-PRES-10`, `OI-PRIV-65`, NP-PRIV-ANALYSIS-007 Finding M12): the data key under the passphrase, and
// the whole record (the Argon2 parameters, the salt and the wrapped key) under the device's keystore key, the same key the device
// tier is sealed under. That key releases without the person, so the record is still read before anyone is present; a copy of the
// host's store taken without the keystore holds nothing an offline guesser can test a passphrase against.

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
 * Keeps the data key wrapped under a passphrase in [host]'s store, and that record sealed under [device]'s keystore key
 * (`REQ-PRES-10`): the host key is not part of either sealed tier, and [device] releases without the person. The wrapping key is derived again at each unlock and is never kept; the data key is held in memory until [lock]. A wrong passphrase
 * fails the AES-GCM tag and is `WRONG`; it is never taken for "no key" (`SWR-MG-15`). Forgetting the passphrase is unrecoverable by
 * design, which is what the backup the app insists on first is for (`REQ-PRES-08`).
 *
 * **Not offered yet.** Moving an install that already holds sealed UHDR to this key is `REQ-PRES-07` (increment 7); no host passes
 * this store to the shared app until it exists.
 */
class PassphraseGatedSecretKeyStore(
    private val host: KeyValueStore,
    /**
     * The device's keystore, the one the device tier is sealed under (`PlatformServices.secretKeyStore`), asked only when the record
     * is read or written. Null, or a keystore that cannot be asked, and nothing is set up or opened: the record is never written
     * under the passphrase alone (`REQ-PRES-10`).
     */
    private val device: () -> SecretKeyStore?,
    private val random: RandomSource = SystemRandomSource,
) : PassphraseKey {
    private var dataKey: ByteArray? = null
    private var unlocked = false

    override val isUnlocked: Boolean get() = unlocked

    /** On once a passphrase has been set. */
    override val isEnabled: Boolean get() = host.getString(WRAP_KEY) != null

    override suspend fun enable(reason: String): Boolean = false

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
        if (host.getString(WRAP_KEY) != null || device() == null) return PassphraseResult.FAILED
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

    /**
     * The record, opened under the device key, or null. A record that is not of this form is refused, not read: the earlier form
     * (the record in the clear) was never offered by a host, so none exists to read, and reading it would let a planted plain record
     * stand in for a sealed one (`REQ-PRES-10` (b)).
     */
    private fun readRecord(): Record? = runCatching {
        val stored = host.getString(WRAP_KEY) ?: return null
        if (!stored.startsWith(RECORD_PREFIX)) return null
        val text = withDeviceKey(create = false) { k ->
            val r = NppsCore.migrationSecret(
                buildJsonObject { put("op", "openValue"); put("key", NppsCore.secretRef(0)); put("name", DEVICE_NAME); put("sealed", stored.removePrefix(RECORD_PREFIX)) },
                listOf(k),
            ).answer
            if (r["ok"]!!.jsonPrimitive.boolean) r["value"]!!.jsonPrimitive.content else null
        } ?: return null
        val o = Json.parseToJsonElement(text).jsonObject
        Record(o["kdf"]!!.jsonObject, o["sealed"]!!.jsonPrimitive.content)
    }.getOrNull()

    /** Seal the record [text] under the device key, making the key if the keystore answered that there is none. Null if it cannot be. */
    private fun sealRecord(text: String): String? = runCatching {
        val nonce = random.nextBytes(NONCE_BYTES)
        require(nonce.size == NONCE_BYTES)
        withDeviceKey(create = true) { k ->
            NppsCore.migrationSecret(
                buildJsonObject { put("op", "sealValue"); put("key", NppsCore.secretRef(0)); put("nonce", nonce.hexString()); put("name", DEVICE_NAME); put("value", text) },
                listOf(k),
            ).answer["sealed"]?.jsonPrimitive?.contentOrNull
        }?.let { RECORD_PREFIX + it }
    }.getOrNull()

    /**
     * Run [use] with a copy of the device key, wiped after. A key is made only when the keystore answered "no key" and [create] is set,
     * as `EncryptedKeyValueStore` does, so a keystore that could not be asked (it throws) never has its key replaced.
     */
    private fun <T> withDeviceKey(create: Boolean, use: (ByteArray) -> T?): T? {
        val keystore = device() ?: return null
        val key = keystore.load() ?: if (create) {
            random.nextBytes(DATA_KEY_BYTES).also { require(it.size == DATA_KEY_BYTES); keystore.store(it) }
        } else {
            return null
        }
        return try { use(key) } finally { key.fill(0) }
    }

    /**
     * The wrapping key for [passphrase] under [kdf], or the reason it cannot be had. The key is made and kept in the shared
     * core (SWR-MG-32): what comes back is its handle, and the caller destroys it.
     */
    private fun wrappingKey(passphrase: String, kdf: JsonObject): Pair<BundleKey?, PassphraseResult> {
        val secret = when (val s = BundleKeyDerivation.passphraseSecret(passphrase)) {
            is BundleSecretResult.Ok -> s.secret
            is BundleSecretResult.Rejected -> return null to (if (s.reason == "too_short") PassphraseResult.TOO_SHORT else PassphraseResult.FAILED)
        }
        return try {
            BundleKeyDerivation.deriveKey(secret, kdf, BundleKeyDerivation.PURPOSE_WRAPPING) to PassphraseResult.OK
        } catch (ex: IllegalArgumentException) {
            null to PassphraseResult.FAILED
        } finally {
            secret.destroy()
        }
    }

    private fun open(passphrase: String, record: Record): Opened {
        val (key, status) = wrappingKey(passphrase, record.kdf)
        if (key == null) return Opened.Refused(status)
        // `unwrapKey`: the data key comes back beside the answer as bytes. It was `openValue`, whose answer held the key as text.
        val r = try {
            NppsCore.migrationSecret(buildJsonObject { put("op", "unwrapKey"); put("key", key.ref()); put("name", WRAP_NAME); put("sealed", record.sealed) }, emptyList())
        } finally {
            key.destroy()
        }
        try {
            if (!r.answer["ok"]!!.jsonPrimitive.boolean) {
                return Opened.Refused(if (r.answer["reason"]?.jsonPrimitive?.contentOrNull == "authentication_failed") PassphraseResult.WRONG else PassphraseResult.FAILED)
            }
            val bytes = NppsCore.secretAt(r, "key")
            return if (bytes != null && bytes.size == DATA_KEY_BYTES) Opened.Key(bytes.copyOf()) else Opened.Refused(PassphraseResult.FAILED)
        } finally {
            r.clear()
        }
    }

    /** Wrap [data] under a new salt and [passphrase], and keep the record. Nothing is written unless every step worked. */
    private fun wrap(passphrase: String, data: ByteArray): PassphraseResult {
        val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.PASSPHRASE, random)
        val (key, status) = wrappingKey(passphrase, kdf)
        if (key == null) return status
        val nonce = random.nextBytes(NONCE_BYTES)
        // `wrapKey`: the data key goes in beside the request as bytes. The record is the same bytes `sealValue` made of its hex.
        val r = try {
            NppsCore.migrationSecret(
                buildJsonObject { put("op", "wrapKey"); put("key", key.ref()); put("nonce", nonce.hexString()); put("name", WRAP_NAME); put("data", NppsCore.secretRef(0)) },
                listOf(data),
            ).answer
        } finally {
            key.destroy()
        }
        val sealed = r["sealed"]?.jsonPrimitive?.contentOrNull ?: return PassphraseResult.FAILED
        val record = sealRecord(buildJsonObject { put("kdf", kdf); put("sealed", sealed) }.toString()) ?: return PassphraseResult.FAILED
        host.putString(WRAP_KEY, record)
        return PassphraseResult.OK
    }

    private fun ByteArray.hexString() = joinToString("") { (it.toInt() and 0xff).toString(16).padStart(2, '0') }

    companion object {
        /**
         * The host key holding the Argon2 parameters and the sealed data key, sealed under the device key by this class (`REQ-PRES-10`).
         * Not in the UHDR tier: it is read before anyone is present.
         */
        const val WRAP_KEY = "np.presence.passphrase-wrap"

        /** Marks the record sealed under the device key. The tiers' own prefix (`np-enc:1:`) is a different one, so neither reads the other's. */
        internal const val RECORD_PREFIX = "np-ppwrap:1:"

        /** The associated data of the device-key sealing: the record opens only as itself, not as a tier value of the same host key. */
        private const val DEVICE_NAME = "np.presence.passphrase-wrap#device"

        /** The associated data of the sealing, so the wrapped key opens only as this record. */
        private const val WRAP_NAME = "np.presence.passphrase-wrap"
        private const val DATA_KEY_BYTES = 32
        private const val NONCE_BYTES = 12
    }
}
