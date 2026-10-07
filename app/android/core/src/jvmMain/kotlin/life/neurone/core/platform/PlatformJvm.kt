package life.neurone.core.platform

import java.security.KeyFactory
import java.security.SecureRandom
import java.security.Signature
import java.security.spec.X509EncodedKeySpec

private val secureRandom = SecureRandom()

actual fun secureRandomBytes(count: Int): ByteArray = ByteArray(count).also { secureRandom.nextBytes(it) }

/**
 * SubjectPublicKeyInfo header for Ed25519 (RFC 8410 §4): SEQUENCE, AlgorithmIdentifier with OID
 * 1.3.101.112 and absent parameters, then a 33-byte BIT STRING with zero unused bits. The JDK takes
 * only SPKI where CryptoKit takes only the raw point, so a raw key is wrapped here and nowhere else.
 */
private val ED25519_SPKI_PREFIX = byteArrayOf(
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21, 0x00,
)

actual fun ed25519Verify(publicKeyRaw: ByteArray, message: ByteArray, signature: ByteArray): Boolean? =
    runCatching {
        val key = KeyFactory.getInstance("Ed25519")
            .generatePublic(X509EncodedKeySpec(ED25519_SPKI_PREFIX + publicKeyRaw))
        Signature.getInstance("Ed25519").run {
            initVerify(key)
            update(message)
            verify(signature)
        }
    }.getOrDefault(false)
