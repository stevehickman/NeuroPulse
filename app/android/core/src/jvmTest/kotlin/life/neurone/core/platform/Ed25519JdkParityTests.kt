package life.neurone.core.platform

import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.core.session.JvmProtocolSigner
import java.security.KeyFactory
import java.security.MessageDigest
import java.security.Signature
import java.security.spec.PKCS8EncodedKeySpec
import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertEquals

/** The common Ed25519 and SHA-512 against the JDK's, byte for byte, on random inputs: Ed25519 is deterministic. */
class Ed25519JdkParityTests {

    private val pkcs8Prefix = byteArrayOf(0x30, 0x2e, 0x02, 0x01, 0x00, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x04, 0x22, 0x04, 0x20)

    @Test fun sha512_matchesTheJdkOnEveryBlockBoundary() {
        val random = Random(512)
        for (size in listOf(0, 1, 55, 56, 111, 112, 113, 127, 128, 129, 255, 256, 1000, 4097)) {
            val data = random.nextBytes(size)
            assertEquals(MessageDigest.getInstance("SHA-512").digest(data).toHex(), Digests.sha512(data).toHex(), "size $size")
        }
    }

    @Test fun signatures_matchTheJdk_andVerifyWithItsVerifier() {
        val random = Random(25519)
        repeat(40) { n ->
            val seed = random.nextBytes(32)
            val message = random.nextBytes(random.nextInt(0, 300))

            val jdkKey = KeyFactory.getInstance("Ed25519").generatePrivate(PKCS8EncodedKeySpec(pkcs8Prefix + seed))
            val jdkSignature = Signature.getInstance("Ed25519").run { initSign(jdkKey); update(message); sign() }

            val ours = Ed25519.sign(seed, message)
            assertEquals(jdkSignature.toHex(), ours.toHex(), "signature $n")
            // The verifier shipped for study descriptors reads ours as valid, and a changed message as not.
            assertEquals(true, ed25519Verify(Ed25519.publicKey(seed), message, ours), "verifies $n")
            assertEquals(false, ed25519Verify(Ed25519.publicKey(seed), message + 1, ours), "a changed message does not verify $n")
        }
    }

    @Test fun fingerprints_areTheSameShapeOnTheCommonAndTheJdkSigner() {
        val seed = ByteArray(32) { (it * 3).toByte() }
        val common = Ed25519ProtocolSigner { seed }.sign(byteArrayOf(1, 2, 3))
        assertEquals(Digests.sha256(Ed25519.spki(Ed25519.publicKey(seed))).copyOf(8).toHex(), common.publicKeyFingerprint)
        assertEquals(16, JvmProtocolSigner().sign(byteArrayOf(1)).publicKeyFingerprint.length)
    }
}
