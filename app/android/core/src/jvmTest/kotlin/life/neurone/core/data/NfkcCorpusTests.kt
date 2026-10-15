package life.neurone.core.data

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The passphrase normalisation through the JNI binding: the corpus of known NFKC answers (app/NeurOneShared/TestData/nfkc-corpus.json)
 * the Rust core, Swift, C# and the web run too. Kotlin's strings are UTF-16 and JNI hands them over as modified UTF-8, so the
 * cases outside the BMP are the ones this proves (SWR-MG-02, NP-SOUP-RUST-001).
 */
class NfkcCorpusTests {
    private val corpus: JsonObject =
        Json.parseToJsonElement(javaClass.getResourceAsStream("/nfkc-corpus.json")!!.bufferedReader().readText()).jsonObject

    private fun secret(passphrase: String): JsonObject =
        NppsCore.migration(buildJsonObject { put("op", "passphraseSecret"); put("passphrase", passphrase) })

    private fun hexOf(text: String): String = text.encodeToByteArray().joinToString("") { "%02x".format(it) }

    // @verifies SWR-MG-02
    @Test
    fun everyKnownNfkcAnswerHoldsThroughTheBinding() {
        val pad = corpus["padding"]!!.jsonPrimitive.content
        val wrong = mutableListOf<String>()
        corpus["cases"]!!.jsonArray.forEach {
            val c = it.jsonObject
            val got = secret(pad + c["input"]!!.jsonPrimitive.content)
            val want = hexOf(pad + c["expected"]!!.jsonPrimitive.content)
            if (got["secret"]?.jsonPrimitive?.content != want) wrong += "${c["name"]!!.jsonPrimitive.content}: ${got["secret"]} != $want"
        }
        corpus["exact"]!!.jsonArray.forEach {
            val c = it.jsonObject
            if (secret(c["input"]!!.jsonPrimitive.content)["secret"]?.jsonPrimitive?.content != hexOf(c["expected"]!!.jsonPrimitive.content)) {
                wrong += "${c["name"]!!.jsonPrimitive.content}: the floor is counted wrongly"
            }
        }
        corpus["tooShort"]!!.jsonArray.forEach {
            val c = it.jsonObject
            if (secret(c["input"]!!.jsonPrimitive.content)["reason"]?.jsonPrimitive?.content != "too_short") wrong += "${c["name"]!!.jsonPrimitive.content}: accepted under the floor"
        }
        assertTrue(wrong.isEmpty(), "${wrong.size} answer(s) differ:\n" + wrong.joinToString("\n"))
        assertEquals(12, corpus["minChars"]!!.jsonPrimitive.content.toInt())
    }
}
