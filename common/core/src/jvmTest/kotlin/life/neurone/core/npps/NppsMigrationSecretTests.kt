package life.neurone.core.npps

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonObject
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * A key crosses the JNI boundary as bytes beside the request, or not at all (SWR-MG-32). The Rust tests hold the rules
 * (`common/npps-core/tests/secret_boundary.rs`); these hold the JVM's marshalling: the framing in both directions, a
 * refusal as an exception, and a kept key used by its handle.
 */
class NppsMigrationSecretTests {
    private val key = ByteArray(32) { (it * 7 + 3).toByte() }
    private val keyHex = key.joinToString("") { "%02x".format(it) }
    private val nonce = "a0a1a2a3a4a5a6a7a8a9aaab"

    private fun secret(n: Int): JsonObject = buildJsonObject { put("\$secret", n) }

    private fun seal(key: (kotlinx.serialization.json.JsonObjectBuilder) -> Unit): JsonObject = buildJsonObject {
        put("op", "sealValue"); key(this); put("nonce", nonce); put("name", "np.n"); put("value", "history")
    }

    // @verifies SWR-MG-32#a
    @Test
    fun aKeyBesideTheRequestSealsWhatTheSameKeyInHexSealedAndIsInNoText() {
        val byHex = NppsCore.migration(seal { it.put("key", keyHex) })
        val request = seal { it.put("key", secret(0)) }
        assertFalse(request.toString().contains(keyHex.take(16)), "the request names the key and does not hold it")
        val r = NppsCore.migrationSecret(request, listOf(key))
        assertEquals(byHex, r.answer)
        assertTrue(r.secrets.isEmpty())
        assertEquals(keyHex, key.joinToString("") { "%02x".format(it) }, "the caller's array is the caller's: it is not cleared for it")
    }

    // @verifies SWR-MG-32#a
    @Test
    fun aDerivedKeyComesBackAsBytesBesideAnAnswerThatOnlyNamesIt() {
        val seed = ByteArray(32) { 7 }
        val request = buildJsonObject { put("op", "deriveUhdrKey"); put("seed", secret(0)); put("salt", "09".repeat(32)) }
        val r = NppsCore.migrationSecret(request, listOf(seed))
        assertEquals(secret(0), r.answer["key"])
        assertEquals(1, r.secrets.size)
        val old = NppsCore.migration(buildJsonObject { put("op", "deriveUhdrKey"); put("seed", "07".repeat(32)); put("salt", "09".repeat(32)) })
        assertEquals(old["key"]!!.jsonPrimitive.content, r.secrets[0].joinToString("") { "%02x".format(it) })
        assertFalse(r.answer.toString().contains(old["key"]!!.jsonPrimitive.content.take(16)))
        assertFalse(r.toString().contains(old["key"]!!.jsonPrimitive.content.take(16)), "nor does the answer print it")
        r.clear()
        assertContentEquals(ByteArray(64), r.secrets[0], "clear() overwrites what was handed back")
    }

    // @verifies SWR-MG-32#a
    @Test
    fun aSecretTheCallWasNotGivenIsRefusedAsAnException() {
        val e = assertFailsWith<IllegalArgumentException> { NppsCore.migrationSecret(seal { it.put("key", secret(3)) }, listOf(key)) }
        assertTrue(e.message!!.contains("was not given"), e.message)
        assertFalse(e.message!!.contains(keyHex.take(16)))
        // The next call is not affected.
        assertEquals(2 * (12 + "history".length + 16), NppsCore.migrationSecret(seal { it.put("key", secret(0)) }, listOf(key)).answer["sealed"]!!.jsonPrimitive.content.length)
    }

    // @verifies SWR-MG-32#b
    @Test
    fun aKeyTheCoreKeepsIsBroughtInOnceUsedByItsHandleAndUnknownOnceReleased() {
        val imported = NppsCore.migrationSecret(buildJsonObject { put("op", "keyImport"); put("purpose", "data"); put("key", secret(0)) }, listOf(key))
        val handle = imported.answer["key"]!!.jsonObject
        assertTrue(handle["\$handle"]!!.jsonPrimitive.long > 0)
        assertTrue(imported.secrets.isEmpty(), "a kept key is not handed back")
        // A handle is not a secret: it goes through the plain entry point.
        val sealed = NppsCore.migration(seal { it.put("key", handle) })
        assertEquals(NppsCore.migration(seal { it.put("key", keyHex) }), sealed)
        // Typed: a data key seals no recording.
        val header = NppsCore.migration(buildJsonObject { put("op", "archiveStart"); put("salt", "01".repeat(16)) })["header"]!!
        val wrong = assertFailsWith<IllegalArgumentException> {
            NppsCore.migration(buildJsonObject { put("op", "archiveSealChunk"); put("objectId", "recording:r1"); put("key", handle); put("header", header); put("counter", 0); put("last", true); put("data", "00") })
        }
        assertTrue(wrong.message!!.startsWith("wrong_key_purpose"), wrong.message)
        assertEquals("true", NppsCore.migration(buildJsonObject { put("op", "keyRelease"); put("key", handle) })["released"]!!.jsonPrimitive.content)
        val gone = assertFailsWith<IllegalArgumentException> { NppsCore.migration(seal { it.put("key", handle) }) }
        assertEquals("unknown_key", gone.message)
    }

    // @verifies SWR-MG-32#c
    @Test
    fun aPassphraseBecomesAKeptKeyInOneCallAndTheSecretItStandsForNeverComesBack() {
        val kdf = NppsCore.migration(buildJsonObject { put("op", "bundleKdf"); put("method", "passphrase"); put("salt", "5a".repeat(32)) })["kdf"]!!
        val passphrase = "correct horse battery".encodeToByteArray()
        val r = NppsCore.migrationSecret(
            buildJsonObject { put("op", "deriveBundleKey"); put("passphrase", secret(0)); put("kdf", kdf); put("keep", "bundle") },
            listOf(passphrase),
        )
        assertTrue(r.secrets.isEmpty())
        val handle = r.answer["key"]!!.jsonObject
        val twoStep = NppsCore.migration(buildJsonObject {
            put("op", "deriveBundleKey"); put("secret", passphrase.joinToString("") { "%02x".format(it) }); put("kdf", kdf)
        })["key"]!!
        fun sealWith(k: kotlinx.serialization.json.JsonElement) = NppsCore.migration(buildJsonObject {
            put("op", "sealBundle"); put("epoch", 1); put("kind", "uhdr"); put("key", k); put("nonce", nonce); putJsonObject("items") {}; put("kdf", kdf)
        })
        assertEquals(sealWith(twoStep), sealWith(handle), "the same key either way")
        NppsCore.migration(buildJsonObject { put("op", "keyRelease"); put("key", handle) })
    }
}
