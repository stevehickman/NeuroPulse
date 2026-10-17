package life.neurone.core.platform

import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.hexToBytes
import life.neurone.core.npps.NppsCore

/**
 * Ed25519 (RFC 8032) for every target, computed by the Rust core (`CLAUDE.md` §22). This object moves bytes and
 * holds nothing else: the arithmetic, the constant-time handling of the secret and the strict verification are the
 * core's, once, for Android, iOS, Windows and the browser.
 */
object Ed25519 {
    private const val SEED_BYTES = 32
    private const val PUBLIC_KEY_BYTES = 32

    fun publicKey(seed: ByteArray): ByteArray {
        require(seed.size == SEED_BYTES) { "An Ed25519 seed is $SEED_BYTES bytes" }
        return call("ed25519PublicKey") { put("seed", seed.toHex()) }["publicKey"]!!.jsonPrimitive.content.hexToBytes()
    }

    fun sign(seed: ByteArray, message: ByteArray): ByteArray {
        require(seed.size == SEED_BYTES) { "An Ed25519 seed is $SEED_BYTES bytes" }
        return call("ed25519Sign") {
            put("seed", seed.toHex())
            put("message", message.toHex())
        }["signature"]!!.jsonPrimitive.content.hexToBytes()
    }

    /** False for a wrong signature, a malformed one or an invalid key; the core never raises for those. */
    fun verify(publicKey: ByteArray, message: ByteArray, signature: ByteArray): Boolean =
        call("ed25519Verify") {
            put("publicKey", publicKey.toHex())
            put("message", message.toHex())
            put("signature", signature.toHex())
        }["ok"]!!.jsonPrimitive.boolean

    /** The DER SubjectPublicKeyInfo of a raw key (RFC 8410 §4), the form the fingerprint is taken over. */
    fun spki(publicKey: ByteArray): ByteArray {
        require(publicKey.size == PUBLIC_KEY_BYTES) { "An Ed25519 public key is $PUBLIC_KEY_BYTES bytes" }
        return SPKI_PREFIX + publicKey
    }

    private val SPKI_PREFIX = byteArrayOf(0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21, 0x00)

    private fun call(op: String, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit) =
        NppsCore.migration(buildJsonObject { put("op", op); build() })
}
