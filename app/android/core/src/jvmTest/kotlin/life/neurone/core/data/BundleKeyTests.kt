package life.neurone.core.data

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertIs
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue

private class RecordingArgon2 : Argon2Provider {
    var memoryKiB = 0
    var iterations = 0
    var parallelism = 0
    var outputLength = 0
    var password = ByteArray(0)
    var salt = ByteArray(0)

    override fun argon2idHash(password: ByteArray, salt: ByteArray, memoryKiB: Int, iterations: Int, parallelism: Int, outputLength: Int): ByteArray {
        this.memoryKiB = memoryKiB; this.iterations = iterations; this.parallelism = parallelism; this.outputLength = outputLength
        this.password = password.copyOf(); this.salt = salt.copyOf()
        return ByteArray(outputLength) { (it * 7 + password.size).toByte() }
    }
}

private class CountingRandom : RandomSource {
    private var n = 0
    override fun nextBytes(count: Int): ByteArray = ByteArray(count) { (n++ % 251).toByte() }
}

/** The bundle key through the JNI binding of the shared core. The rules are tested in Rust; this holds the marshalling. */
class BundleKeyTests {
    // @verifies SWR-MG-02
    @Test
    fun aRecoveryCodeShownToTheUserDerivesTheSameKeyWhenTypedBack() {
        val made = BundleKeyDerivation.newRecoveryCode(CountingRandom())
        val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.RECOVERY_CODE, CountingRandom())
        val argon = RecordingArgon2()
        val first = BundleKeyDerivation.deriveKey(argon, made.secret, kdf).hex()
        val typed = BundleKeyDerivation.recoveryCodeSecret(made.text.lowercase().replace("-", " "))
        assertIs<BundleSecretResult.Ok>(typed)
        assertEquals(first, BundleKeyDerivation.deriveKey(RecordingArgon2(), typed.secret, kdf).hex())
    }

    // @verifies SWR-MG-02
    @Test
    fun theCostParametersAreTheCoresAndNeverTheHeaders() {
        val argon = RecordingArgon2()
        val made = BundleKeyDerivation.newRecoveryCode(CountingRandom())
        val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.PASSPHRASE, CountingRandom())
        BundleKeyDerivation.deriveKey(argon, made.secret, kdf)
        assertEquals(65_536, argon.memoryKiB)
        assertEquals(4, argon.iterations)
        assertEquals(1, argon.parallelism)
        assertEquals(32, argon.outputLength)
        assertEquals(32, argon.salt.size)
        val weak = JsonObject(kdf + ("memoryKiB" to JsonPrimitive(8)))
        assertFailsWith<IllegalArgumentException> { BundleKeyDerivation.deriveKey(RecordingArgon2(), made.secret, weak) }
        val other = JsonObject(kdf + ("algorithm" to JsonPrimitive("argon2i")))
        assertFailsWith<IllegalArgumentException> { BundleKeyDerivation.deriveKey(RecordingArgon2(), made.secret, other) }
    }

    // @verifies SWR-MG-02
    @Test
    fun aPassphraseIsRefusedWhenShortAndNormalisedWhenNot() {
        val short = BundleKeyDerivation.passphraseSecret("too short")
        assertIs<BundleSecretResult.Rejected>(short)
        assertEquals("too_short", short.reason)
        assertTrue((short.minChars ?: 0) > 0)
        val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.PASSPHRASE, CountingRandom())
        val a = BundleKeyDerivation.passphraseSecret("café au lait noir") as BundleSecretResult.Ok
        val b = BundleKeyDerivation.passphraseSecret("café au lait noir") as BundleSecretResult.Ok
        assertEquals(BundleKeyDerivation.deriveKey(RecordingArgon2(), a.secret, kdf).hex(), BundleKeyDerivation.deriveKey(RecordingArgon2(), b.secret, kdf).hex())
    }

    // @verifies SWR-MG-02
    @Test
    fun aMistypedCodeIsATypingErrorAndADestroyedSecretOrKeyRefusesUse() {
        val made = BundleKeyDerivation.newRecoveryCode(CountingRandom())
        val flipped = (if (made.text[0] == '2') '3' else '2') + made.text.substring(1)
        val r = BundleKeyDerivation.recoveryCodeSecret(flipped)
        assertIs<BundleSecretResult.Rejected>(r)
        assertEquals("bad_checksum", r.reason)
        val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.RECOVERY_CODE, CountingRandom())
        val key = BundleKeyDerivation.deriveKey(RecordingArgon2(), made.secret, kdf)
        key.destroy()
        assertFailsWith<IllegalStateException> { key.hex() }
        made.secret.destroy()
        assertFailsWith<IllegalStateException> { BundleKeyDerivation.deriveKey(RecordingArgon2(), made.secret, kdf) }
    }

    // @verifies SWR-MG-02
    @Test
    fun theDerivedKeySealsAndOpensABundleAndAnotherCodeCannotOpenIt() {
        val made = BundleKeyDerivation.newRecoveryCode(CountingRandom())
        val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.RECOVERY_CODE, CountingRandom())
        val key = BundleKeyDerivation.deriveKey(RecordingArgon2(), made.secret, kdf)
        val sealed = NppsCore.migration(buildJsonObject {
            put("op", "sealBundle"); put("kind", "uhdr"); put("key", key.hex()); put("nonce", "a0".repeat(12))
            put("kdf", kdf); put("items", buildJsonObject { })
        })
        val header = NppsCore.migration(buildJsonObject { put("op", "readBundleHeader"); put("bundle", sealed["bundle"]!!) })
        assertEquals(JsonPrimitive("recoveryCode"), header["method"])
        val ok = NppsCore.migration(buildJsonObject { put("op", "openBundle"); put("bundle", sealed["bundle"]!!); put("key", key.hex()) })
        assertEquals(JsonPrimitive(true), ok["ok"])
        val otherSecret = (BundleKeyDerivation.passphraseSecret("a different passphrase") as BundleSecretResult.Ok).secret
        val otherKey = BundleKeyDerivation.deriveKey(RecordingArgon2(), otherSecret, kdf).hex()
        assertNotEquals(key.hex(), otherKey)
        val denied = NppsCore.migration(buildJsonObject { put("op", "openBundle"); put("bundle", sealed["bundle"]!!); put("key", otherKey) })
        assertEquals(JsonPrimitive("authentication_failed"), denied["reason"])
    }
}
