package life.neurone.core.data

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertIs
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue

private class CountingRandom : RandomSource {
    private var n = 0
    override fun nextBytes(count: Int): ByteArray = ByteArray(count) { (n++ % 251).toByte() }
}

/**
 * A key the core keeps cannot be read, so two keys are compared by what only the same key can make: the confirmation tag
 * (HMAC-SHA-256 under a key derived from it, `SWR-MG-08`) over a fixed text. Equal marks, equal keys.
 */
private fun BundleKey.mark(): String =
    NppsCore.migration(buildJsonObject { put("op", "confirmationTag"); put("key", ref()); put("bundleHash", "a fixed text") })["tag"]!!.jsonPrimitive.content

/** The bundle key through the JNI binding of the shared core, Argon2id included. The rules are tested in Rust; this holds the marshalling. */
class BundleKeyTests {
    // @verifies SWR-MG-02
    @Test
    fun aRecoveryCodeShownToTheUserDerivesTheSameKeyWhenTypedBack() {
        val made = BundleKeyDerivation.newRecoveryCode(CountingRandom())
        val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.RECOVERY_CODE, CountingRandom())
        val first = BundleKeyDerivation.deriveKey(made.secret, kdf).mark()
        val typed = BundleKeyDerivation.recoveryCodeSecret(made.text.lowercase().replace("-", " "))
        assertIs<BundleSecretResult.Ok>(typed)
        assertEquals(first, BundleKeyDerivation.deriveKey(typed.secret, kdf).mark())
    }

    // @verifies SWR-MG-05
    @Test
    fun aHeaderCannotAskThisPhoneForACheaperDerivation() {
        val made = BundleKeyDerivation.newRecoveryCode(CountingRandom())
        val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.PASSPHRASE, CountingRandom())
        assertEquals(64, BundleKeyDerivation.deriveKey(made.secret, kdf).mark().length, "the fixed cost is accepted and makes a usable key")
        val weak = JsonObject(kdf + ("memoryKiB" to JsonPrimitive(8)))
        assertFailsWith<IllegalArgumentException> { BundleKeyDerivation.deriveKey(made.secret, weak) }
        val other = JsonObject(kdf + ("algorithm" to JsonPrimitive("argon2i")))
        assertFailsWith<IllegalArgumentException> { BundleKeyDerivation.deriveKey(made.secret, other) }
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
        assertEquals(BundleKeyDerivation.deriveKey(a.secret, kdf).mark(), BundleKeyDerivation.deriveKey(b.secret, kdf).mark())
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
        val key = BundleKeyDerivation.deriveKey(made.secret, kdf)
        key.destroy()
        assertFailsWith<IllegalStateException> { key.ref() }
        made.secret.destroy()
        assertFailsWith<IllegalStateException> { BundleKeyDerivation.deriveKey(made.secret, kdf) }
    }

    // @verifies SWR-MG-02
    @Test
    fun theDerivedKeySealsAndOpensABundleAndAnotherCodeCannotOpenIt() {
        val made = BundleKeyDerivation.newRecoveryCode(CountingRandom())
        val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.RECOVERY_CODE, CountingRandom())
        val key = BundleKeyDerivation.deriveKey(made.secret, kdf)
        val sealed = NppsCore.migration(buildJsonObject {
            put("op", "sealBundle"); put("kind", "uhdr"); put("key", key.ref()); put("nonce", "a0".repeat(12))
            put("kdf", kdf); put("items", buildJsonObject { })
        })
        val header = NppsCore.migration(buildJsonObject { put("op", "readBundleHeader"); put("bundle", sealed["bundle"]!!) })
        assertEquals(JsonPrimitive("recoveryCode"), header["method"])
        val ok = NppsCore.migration(buildJsonObject { put("op", "openBundle"); put("bundle", sealed["bundle"]!!); put("key", key.ref()) })
        assertEquals(JsonPrimitive(true), ok["ok"])
        val otherSecret = (BundleKeyDerivation.passphraseSecret("a different passphrase") as BundleSecretResult.Ok).secret
        val otherKey = BundleKeyDerivation.deriveKey(otherSecret, kdf)
        assertNotEquals(key.mark(), otherKey.mark())
        val denied = NppsCore.migration(buildJsonObject { put("op", "openBundle"); put("bundle", sealed["bundle"]!!); put("key", otherKey.ref()) })
        assertEquals(JsonPrimitive("authentication_failed"), denied["reason"])
    }
}
