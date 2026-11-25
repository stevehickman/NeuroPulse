package life.neurone.core.data

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore
import life.neurone.core.platform.secureRandomBytes

// The user-held key of a migration or backup bundle (SWR-MG-02, issue #569 package 6d).
//
// The UHDR key (UhdrKeyDerivation) is bound to this device, so a bundle moving to a new phone cannot use it. A bundle is
// sealed under a key made from something the user holds: a recovery code the app shows once, or a passphrase the user
// chooses. The rules (the cost parameters, the code's text form and checksum, the passphrase's normalisation and minimum
// length, what a header's kdf block may say) are decided once in the shared core. This file marshals them and runs the
// Argon2id computation, which the core also runs, so there is one implementation for every app.
//
// The secret and the key are memory-only, like the UHDR key: never written to a file or a log, never transmitted. NeurOne never
// holds either (CLAUDE.md §5.1). One exception, which the person chooses: to make a backup without asking again, the secret may
// be kept in the app's SEALED per-install store (`np.backup.auto-secret`, encrypted under the device-only data key, never in a
// bundle and never moved to another phone), by `toStoredText` and `storedSecret`. It is the person's own phone holding the
// person's own secret, and it is destroyed by turning the automatic backup off and by the clear. A run whose storage is not
// sealed keeps it nowhere (`SWR-MG-13` (e), `OI-PRIV-61`): the shared app's `ScheduledBackup` refuses to store it there.

enum class BundleKeyMethod(val wire: String) {
    RECOVERY_CODE("recoveryCode"),
    PASSPHRASE("passphrase");

    companion object {
        fun fromWire(w: String): BundleKeyMethod? = values().firstOrNull { it.wire == w }
    }
}

/** Randomness from the platform's CSPRNG. The core is pure and has none, so a salt or a code comes from here. */
fun interface RandomSource {
    fun nextBytes(count: Int): ByteArray
}

/** The platform's CSPRNG as a [RandomSource]: what every flow and every screen uses unless a test passes its own. */
val SystemRandomSource: RandomSource = RandomSource { secureRandomBytes(it) }

/** What a secret from a person (or a fresh code) became. [reason] is a code for the screen to word from the locale files. */
sealed class BundleSecretResult {
    class Ok(val secret: BundleSecret) : BundleSecretResult()

    /** `malformed` or `bad_checksum` (a recovery code), `too_short` (a passphrase, with [minChars]). */
    class Rejected(val reason: String, val minChars: Int? = null) : BundleSecretResult()
}

/** Memory-only secret bytes. Zero after use via [destroy]. */
class BundleSecret internal constructor(bytes: ByteArray) {
    private val material = bytes.copyOf()
    private var destroyed = false

    internal fun bytes(): ByteArray {
        check(!destroyed) { "BundleSecret has been destroyed" }
        return material
    }

    fun destroy() {
        material.fill(0)
        destroyed = true
    }
}

/** The secret as text, for the sealed per-install store only (the automatic backup). Never for a file, a log or the network. */
fun BundleSecret.toStoredText(): String = bytes().toHex()

/** The secret kept by [toStoredText], or null when [text] is not one. */
fun BundleKeyDerivation.storedSecret(text: String): BundleSecret? {
    val bytes = runCatching { text.hexToBytes() }.getOrNull()?.takeIf { it.isNotEmpty() } ?: return null
    return BundleSecret(bytes).also { bytes.fill(0) }
}

/** A recovery code just made: the [text] to show the user once, and the [secret] it stands for. */
class NewRecoveryCode(val text: String, val secret: BundleSecret)

/**
 * The derived 32-byte AES-256-GCM key. **The key itself is not here**: the shared core derived it and keeps it, and this
 * holds the handle the core gave (`SWR-MG-32` (b)). So no code on this side can read the key, print it or turn it into a
 * string, and the operations that use it are given the handle. [destroy] has the core wipe it; a key that is not
 * destroyed stays in the core until the process ends, and the core holds only so many.
 */
class BundleKey internal constructor(private val handle: JsonObject) {
    private var destroyed = false

    /** What a request to the core carries where the key belongs: `{"$handle": n}`. Not a secret. */
    internal fun ref(): JsonObject {
        check(!destroyed) { "BundleKey has been destroyed" }
        return handle
    }

    fun destroy() {
        if (destroyed) return
        destroyed = true
        runCatching { NppsCore.migration(buildJsonObject { put("op", "keyRelease"); put("key", handle) }) }
    }

    override fun toString(): String = "BundleKey(<in the core>)"
}

object BundleKeyDerivation {
    const val KEY_LENGTH = 32
    const val SALT_LENGTH = 32
    const val RECOVERY_BYTES = 32

    private fun ask(op: String, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit): JsonObject =
        NppsCore.migration(buildJsonObject { put("op", op); build() })

    /** A fresh recovery code. Show [NewRecoveryCode.text] once; the bundle can be opened only with it. */
    fun newRecoveryCode(random: RandomSource): NewRecoveryCode {
        val bytes = random.nextBytes(RECOVERY_BYTES)
        require(bytes.size == RECOVERY_BYTES) { "The random source gave ${bytes.size} bytes, not $RECOVERY_BYTES" }
        // The bytes are the secret: they go to the core beside the request, never as hex in it (SWR-MG-32).
        val code = NppsCore.migrationSecret(buildJsonObject { put("op", "encodeRecoveryCode"); put("bytes", NppsCore.secretRef(0)) }, listOf(bytes))
            .answer["code"]!!.jsonPrimitive.content
        val secret = BundleSecret(bytes)
        bytes.fill(0)
        return NewRecoveryCode(code, secret)
    }

    /** A recovery code typed or pasted by the user. */
    fun recoveryCodeSecret(text: String): BundleSecretResult = secretResult("parseRecoveryCode", "code", text)

    /** A passphrase the user chose (or typed to restore). Normalised and length-checked by the core. */
    fun passphraseSecret(text: String): BundleSecretResult = secretResult("passphraseSecret", "passphrase", text)

    /**
     * [text] is already a string when a screen hands it over, and that copy is the toolkit's. From here on it is bytes: it
     * goes to the core beside the request, and the secret it stands for comes back beside the answer, neither as text
     * (SWR-MG-32).
     */
    private fun secretResult(op: String, field: String, text: String): BundleSecretResult {
        val typed = text.encodeToByteArray()
        val r = try {
            NppsCore.migrationSecret(buildJsonObject { put("op", op); put(field, NppsCore.secretRef(0)) }, listOf(typed))
        } finally {
            typed.fill(0)
        }
        try {
            if (r.answer["ok"]!!.jsonPrimitive.content != "true") {
                return BundleSecretResult.Rejected(r.answer["reason"]!!.jsonPrimitive.content, r.answer["minChars"]?.jsonPrimitive?.int)
            }
            return BundleSecretResult.Ok(BundleSecret(NppsCore.secretAt(r, "secret") ?: error("the core answered ok with no secret")))
        } finally {
            r.clear()
        }
    }

    /** The `kdf` block for the header of a bundle about to be sealed: a fresh random salt and the core's fixed cost. */
    fun newKdf(method: BundleKeyMethod, random: RandomSource): JsonObject {
        val salt = random.nextBytes(SALT_LENGTH)
        require(salt.size == SALT_LENGTH) { "The random source gave ${salt.size} bytes, not $SALT_LENGTH" }
        return ask("bundleKdf") { put("method", method.wire); put("salt", salt.toHex()) }["kdf"] as JsonObject
    }

    /**
     * The bundle key for a secret and a header's `kdf` block: Argon2id computed by the shared core, so every app derives
     * the same key and none carries a second implementation (the device-bound UHDR key is the same crate in the same core, `deriveUhdrKey`, on Kotlin; Swift keeps its vendored PHC argon2 for now). The core
     * checks the block against the fixed cost first, so a bundle cannot ask this phone for a cheaper derivation or another
     * algorithm. Throws [IllegalArgumentException] for a block the core refuses.
     */
    fun deriveKey(secret: BundleSecret, kdf: JsonObject): BundleKey = deriveKey(secret, kdf, PURPOSE_BUNDLE)

    /**
     * As [deriveKey], for the purpose the key is kept under in the core: [PURPOSE_BUNDLE] seals bundles and recordings,
     * [PURPOSE_WRAPPING] wraps the data key behind a passphrase. The core refuses a key used for the other purpose. The
     * secret goes in beside the request as bytes and the key does not come back at all (SWR-MG-32).
     */
    internal fun deriveKey(secret: BundleSecret, kdf: JsonObject, purpose: String): BundleKey {
        val r = NppsCore.migrationSecret(
            buildJsonObject { put("op", "deriveBundleKey"); put("secret", NppsCore.secretRef(0)); put("kdf", kdf); put("keep", purpose) },
            listOf(secret.bytes()),
        )
        r.clear()
        require(r.answer["ok"]!!.jsonPrimitive.content == "true") { "unsupported kdf: ${r.answer["reason"]?.jsonPrimitive?.content}" }
        return BundleKey(r.answer["key"] as JsonObject)
    }

    internal const val PURPOSE_BUNDLE = "bundle"
    internal const val PURPOSE_WRAPPING = "wrapping"
}

internal fun ByteArray.toHex(): String = joinToString("") { (it.toInt() and 0xff).toString(16).padStart(2, '0') }

internal fun String.hexToBytes(): ByteArray {
    require(length % 2 == 0) { "not hex" }
    return ByteArray(length / 2) { i -> substring(2 * i, 2 * i + 2).toInt(16).toByte() }
}
