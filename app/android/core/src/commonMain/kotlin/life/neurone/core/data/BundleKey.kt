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
// The secret and the key are memory-only, like the UHDR key: never written to a KeyValueStore, a file or a log, never
// transmitted. NeurOne never holds either (CLAUDE.md §5.1).

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

/** A recovery code just made: the [text] to show the user once, and the [secret] it stands for. */
class NewRecoveryCode(val text: String, val secret: BundleSecret)

/** The derived 32-byte AES-256-GCM key. Memory-only; zero after use via [destroy]. */
class BundleKey internal constructor(bytes: ByteArray) {
    init {
        require(bytes.size == BundleKeyDerivation.KEY_LENGTH) { "A bundle key is ${BundleKeyDerivation.KEY_LENGTH} bytes" }
    }

    private val material = bytes.copyOf()
    private var destroyed = false

    /** The key as the lowercase hex the core's sealBundle and openBundle take. */
    fun hex(): String {
        check(!destroyed) { "BundleKey has been destroyed" }
        return material.toHex()
    }

    fun destroy() {
        material.fill(0)
        destroyed = true
    }
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
        val code = ask("encodeRecoveryCode") { put("bytes", bytes.toHex()) }["code"]!!.jsonPrimitive.content
        val secret = BundleSecret(bytes)
        bytes.fill(0)
        return NewRecoveryCode(code, secret)
    }

    /** A recovery code typed or pasted by the user. */
    fun recoveryCodeSecret(text: String): BundleSecretResult = secretResult(ask("parseRecoveryCode") { put("code", text) })

    /** A passphrase the user chose (or typed to restore). Normalised and length-checked by the core. */
    fun passphraseSecret(text: String): BundleSecretResult = secretResult(ask("passphraseSecret") { put("passphrase", text) })

    private fun secretResult(r: JsonObject): BundleSecretResult =
        if (r["ok"]!!.jsonPrimitive.content == "true") {
            val bytes = r["secret"]!!.jsonPrimitive.content.hexToBytes()
            BundleSecretResult.Ok(BundleSecret(bytes)).also { bytes.fill(0) }
        } else {
            BundleSecretResult.Rejected(r["reason"]!!.jsonPrimitive.content, r["minChars"]?.jsonPrimitive?.int)
        }

    /** The `kdf` block for the header of a bundle about to be sealed: a fresh random salt and the core's fixed cost. */
    fun newKdf(method: BundleKeyMethod, random: RandomSource): JsonObject {
        val salt = random.nextBytes(SALT_LENGTH)
        require(salt.size == SALT_LENGTH) { "The random source gave ${salt.size} bytes, not $SALT_LENGTH" }
        return ask("bundleKdf") { put("method", method.wire); put("salt", salt.toHex()) }["kdf"] as JsonObject
    }

    /**
     * The bundle key for a secret and a header's `kdf` block: Argon2id computed by the shared core, so every app derives
     * the same key and none carries a second implementation (the device-bound UHDR key keeps [Argon2Provider]). The core
     * checks the block against the fixed cost first, so a bundle cannot ask this phone for a cheaper derivation or another
     * algorithm. Throws [IllegalArgumentException] for a block the core refuses.
     */
    fun deriveKey(secret: BundleSecret, kdf: JsonObject): BundleKey {
        val r = ask("deriveBundleKey") { put("secret", secret.bytes().toHex()); put("kdf", kdf) }
        require(r["ok"]!!.jsonPrimitive.content == "true") { "unsupported kdf: ${r["reason"]?.jsonPrimitive?.content}" }
        val raw = r["key"]!!.jsonPrimitive.content.hexToBytes()
        val key = BundleKey(raw)
        raw.fill(0)
        return key
    }
}

internal fun ByteArray.toHex(): String = joinToString("") { (it.toInt() and 0xff).toString(16).padStart(2, '0') }

internal fun String.hexToBytes(): ByteArray {
    require(length % 2 == 0) { "not hex" }
    return ByteArray(length / 2) { i -> substring(2 * i, 2 * i + 2).toInt(16).toByte() }
}
