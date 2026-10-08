package life.neurone.core.session

import life.neurone.core.platform.Digests
import life.neurone.core.platform.Ed25519
import life.neurone.core.platform.secureRandomBytes
import life.neurone.core.platform.toHex

/**
 * Signs the hub descriptor with Ed25519 in common code, for hosts with no synchronous platform signer (iOS and the
 * browser). Same contract as the JVM and Android signers: it signs the descriptor's raw signed region, not a digest
 * (NP-FW-HUB-001 §4.4), and the key is a fresh secret per process, so the hub's key-pinning contract is the same
 * placeholder it is on those platforms (`OI-AND-SIGN-01`). The fingerprint is the first 8 bytes of SHA-256 over the
 * key's SubjectPublicKeyInfo, which is what the JDK-based signers compute, so one key fingerprints identically
 * everywhere.
 */
class Ed25519ProtocolSigner(
    private val randomBytes: (Int) -> ByteArray = ::secureRandomBytes,
) : ProtocolSigner {

    private val seed: ByteArray by lazy { randomBytes(32) }
    private val publicKey: ByteArray by lazy { Ed25519.publicKey(seed) }

    override fun sign(message: ByteArray): SignatureResult = SignatureResult(
        signature = Ed25519.sign(seed, message),
        publicKeyFingerprint = Digests.sha256(Ed25519.spki(publicKey)).copyOf(8).toHex(),
    )
}
