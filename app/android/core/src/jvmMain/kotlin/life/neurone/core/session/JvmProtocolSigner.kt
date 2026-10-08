package life.neurone.core.session

import life.neurone.core.platform.Digests
import life.neurone.core.platform.toHex
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.Signature

/**
 * Ed25519 [ProtocolSigner] over the JDK's own provider (JDK 15+), for hosts that run on a JVM and have no key
 * store of their own, the desktop apps. Signs the descriptor's raw signed region: the hub verifies Ed25519 over
 * the bytes, not over a digest (NP-FW-HUB-001 §4.4). The key is ephemeral, regenerated per process, as Android's
 * is, while the hub firmware's key-pinning contract is still a placeholder (OI-AND-SIGN-01).
 */
class JvmProtocolSigner : ProtocolSigner {

    private val keyPair: KeyPair by lazy { KeyPairGenerator.getInstance("Ed25519").generateKeyPair() }

    override fun sign(message: ByteArray): SignatureResult {
        val signature = Signature.getInstance("Ed25519").run {
            initSign(keyPair.private)
            update(message)
            sign()
        }
        val fingerprint = Digests.sha256(keyPair.public.encoded).take(8).toByteArray().toHex()
        return SignatureResult(signature = signature, publicKeyFingerprint = fingerprint)
    }
}
