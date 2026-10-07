package life.neurone.core.platform

import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import platform.Security.SecRandomCopyBytes
import platform.Security.kSecRandomDefault

@OptIn(kotlinx.cinterop.ExperimentalForeignApi::class)
actual fun secureRandomBytes(count: Int): ByteArray {
    val out = ByteArray(count)
    if (count == 0) return out
    val status = out.usePinned { SecRandomCopyBytes(kSecRandomDefault, count.toULong(), it.addressOf(0)) }
    check(status == 0) { "SecRandomCopyBytes failed: $status" }
    return out
}

/** No Ed25519 verifier on this target yet, so a signed descriptor is refused, never trusted. */
actual fun ed25519Verify(publicKeyRaw: ByteArray, message: ByteArray, signature: ByteArray): Boolean? = null
