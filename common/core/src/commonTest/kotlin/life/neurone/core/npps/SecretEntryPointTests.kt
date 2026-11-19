package life.neurone.core.npps

import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * The secret entry point through whichever binding this target has (SWR-MG-30 (a), (b)): in common code, so the same
 * assertions run through JNI on the JVM and through WebAssembly under Node. It holds the marshalling in both directions:
 * bytes in beside the request, bytes out beside the answer, and a handle used through the plain entry point.
 */
class SecretEntryPointTests {
    private val key = ByteArray(32) { (it * 7 + 3).toByte() }
    private fun hex(b: ByteArray) = b.joinToString("") { (it.toInt() and 0xff).toString(16).padStart(2, '0') }

    // @verifies SWR-MG-30#a
    @Test
    fun aSecretGoesInBesideTheRequestAndComesBackBesideTheAnswerOnThisBinding() = runTest {
        prepareNppsCore()
        val seed = ByteArray(32) { 7 }
        val r = NppsCore.migrationSecret(buildJsonObject { put("op", "deriveUhdrKey"); put("seed", NppsCore.secretRef(0)); put("salt", "09".repeat(32)) }, listOf(seed))
        assertEquals(NppsCore.secretRef(0), r.answer["key"])
        val byHex = NppsCore.migration(buildJsonObject { put("op", "deriveUhdrKey"); put("seed", "07".repeat(32)); put("salt", "09".repeat(32)) })
        assertEquals(byHex["key"]!!.jsonPrimitive.content, hex(NppsCore.secretAt(r, "key")!!))
        r.clear()
        assertContentEquals(ByteArray(64), r.secrets[0])
        // Two secrets, in the order the request names them, and none given is a refusal with the core's message.
        val wrapped = NppsCore.migrationSecret(
            buildJsonObject { put("op", "wrapKey"); put("key", NppsCore.secretRef(1)); put("nonce", "a0".repeat(12)); put("name", "np.wrap"); put("data", NppsCore.secretRef(0)) },
            listOf(key, ByteArray(32) { 0x5a }),
        ).answer
        val opened = NppsCore.migrationSecret(
            buildJsonObject { put("op", "unwrapKey"); put("key", NppsCore.secretRef(0)); put("name", "np.wrap"); put("sealed", wrapped["sealed"]!!) },
            listOf(ByteArray(32) { 0x5a }),
        )
        assertContentEquals(key, NppsCore.secretAt(opened, "key"))
        val refused = assertFailsWith<IllegalArgumentException> {
            NppsCore.migrationSecret(buildJsonObject { put("op", "unwrapKey"); put("key", NppsCore.secretRef(0)); put("name", "np.wrap"); put("sealed", wrapped["sealed"]!!) }, emptyList())
        }
        assertTrue(refused.message!!.contains("was not given"), refused.message)
    }

    // @verifies SWR-MG-30#b
    @Test
    fun aKeptKeyIsUsedByItsHandleAndIsUnknownOnceReleasedOnThisBinding() = runTest {
        prepareNppsCore()
        val handle = NppsCore.migrationSecret(buildJsonObject { put("op", "keyImport"); put("purpose", "data"); put("key", NppsCore.secretRef(0)) }, listOf(key)).answer["key"]!!.jsonObject
        val seal = { k: kotlinx.serialization.json.JsonElement ->
            NppsCore.migration(buildJsonObject { put("op", "sealValue"); put("key", k); put("nonce", "a0".repeat(12)); put("name", "np.n"); put("value", "history") })
        }
        assertEquals(seal(kotlinx.serialization.json.JsonPrimitive(hex(key))), seal(handle))
        assertEquals("true", NppsCore.migration(buildJsonObject { put("op", "keyRelease"); put("key", handle) })["released"]!!.jsonPrimitive.content)
        assertEquals("unknown_key", assertFailsWith<IllegalArgumentException> { seal(handle) }.message)
    }
}
