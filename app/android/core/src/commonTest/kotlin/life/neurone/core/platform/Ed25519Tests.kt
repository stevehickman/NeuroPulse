package life.neurone.core.platform

import life.neurone.core.session.Ed25519ProtocolSigner
import kotlin.test.Test
import kotlin.test.assertEquals

private fun hex(s: String): ByteArray = ByteArray(s.length / 2) { s.substring(it * 2, it * 2 + 2).toInt(16).toByte() }

/**
 * The common Ed25519 and SHA-512, pinned to the published vectors (FIPS 180-4 and RFC 8032 §7.1). The JVM test
 * (`Ed25519JdkParityTests`) additionally compares them with the JDK's own on random inputs.
 */
class Ed25519Tests {

    @Test fun sha512_matchesFips180Vectors() {
        assertEquals(
            "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
            Digests.sha512(ByteArray(0)).toHex(),
        )
        assertEquals(
            "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
            Digests.sha512("abc".encodeToByteArray()).toHex(),
        )
        // Two blocks: 112 bytes forces the length into a second block.
        assertEquals(
            "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909",
            Digests.sha512(
                "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu".encodeToByteArray(),
            ).toHex(),
        )
    }

    @Test fun ed25519_rfc8032_test1_emptyMessage() {
        val seed = hex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60")
        assertEquals("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", Ed25519.publicKey(seed).toHex())
        assertEquals(
            "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b",
            Ed25519.sign(seed, ByteArray(0)).toHex(),
        )
    }

    @Test fun signer_signsWithAKeyThatMatchesItsFingerprint() {
        var calls = 0
        val signer = Ed25519ProtocolSigner { n -> ByteArray(n) { (it + 7).toByte() }.also { calls++ } }
        val a = signer.sign("descriptor".encodeToByteArray())
        val b = signer.sign("descriptor".encodeToByteArray())
        assertEquals(1, calls, "one key per process")
        assertEquals(a.signature.toHex(), b.signature.toHex(), "Ed25519 is deterministic")
        assertEquals(16, a.publicKeyFingerprint.length)
    }
}
