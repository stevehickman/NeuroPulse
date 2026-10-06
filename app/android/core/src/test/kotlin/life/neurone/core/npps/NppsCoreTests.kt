package life.neurone.core.npps

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * The Android binding of the shared NPPS core (OI-NPPS-CORE-01), exercised through the real
 * native library. The goldens are the web parser's and compiler's own output
 * (`bun scripts/gen-npps-parse-golden.ts`, `bun scripts/gen-hub-descriptor-golden.ts`); the Rust
 * crate is already diffed against them, so what this adds is the JNI layer: string and byte
 * marshalling (the messages contain `—` and `²`), exception mapping, and the Kotlin request
 * builder. Every entry, byte and message must match.
 */
class NppsCoreTests {
    private val root: File = generateSequence(File("").absoluteFile) { it.parentFile }
        .first { File(it, "common/npps-core").isDirectory }

    private fun resource(name: String): JsonObject =
        Json.parseToJsonElement(javaClass.getResourceAsStream("/$name")!!.bufferedReader().readText()).jsonObject

    /** What the web parser's entry list reduces to: protocols and the count of composites. */
    private fun reduce(entries: JsonArray): JsonArray = JsonArray(entries.filter {
        val o = it.jsonObject
        o["kind"]?.jsonPrimitive?.content == "single" || o["what"]?.jsonPrimitive?.content == "composite"
    })

    private fun checkParse(name: String, source: String, expected: JsonObject, failures: MutableList<String>) {
        val want = expected["error"]
        try {
            val got = reduce(NppsCore.parse(source))
            if (want != null && want !is JsonNull) failures += "$name: accepted what the web parser refuses: $want"
            else if (got != expected["entries"]) failures += "$name: entries differ"
        } catch (e: IllegalArgumentException) {
            if (want == null || want is JsonNull) failures += "$name: refused what the web parser accepts: ${e.message}"
            else if (e.message != want.jsonPrimitive.content) {
                failures += "$name: message differs\n  core: ${e.message}\n  web:  ${want.jsonPrimitive.content}"
            }
        }
    }

    @Test
    fun theWholeShippedLibraryParsesAsTheWebParserDoes() {
        val files = resource("npps-parse-golden.json")["files"]!!.jsonObject
        assertTrue(files.size > 60, "the golden covers the shipped library")
        val failures = mutableListOf<String>()
        for ((rel, expected) in files) checkParse(rel, File(root, rel).readText(), expected.jsonObject, failures)
        assertTrue(failures.isEmpty(), "${failures.size} divergence(s):\n" + failures.joinToString("\n"))
    }

    @Test
    fun theRefusalAndAliasCorpusParsesAsTheWebParserDoes() {
        val cases = resource("npps-parse-golden.json")["cases"]!!.jsonObject
        val failures = mutableListOf<String>()
        for ((name, c) in cases) checkParse(name, c.jsonObject["source"]!!.jsonPrimitive.content, c.jsonObject, failures)
        assertTrue(failures.isEmpty(), "${failures.size} divergence(s):\n" + failures.joinToString("\n"))
    }

    private fun hex(b: ByteArray) = b.joinToString("") { "%02x".format(it) }

    private fun options(cases: JsonObject, withClinician: Boolean) = NppsCore.CompileOptions(
        zones = cases["zones"]!!.jsonObject.mapValues { (_, v) -> v.jsonArray.map { it.jsonPrimitive.int } },
        clinicianSockets = if (withClinician) cases["clinicianSockets"]!!.jsonArray.map { it.jsonPrimitive.int } else null,
        nowUnix = cases["compiledAt"]!!.jsonPrimitive.long,
        sessionUuid = ByteArray(16) { cases["sessionUuidByte"]!!.jsonPrimitive.int.toByte() },
    )

    @Test
    fun everyGoldenDefinitionCompilesToTheWebCompilersBytes() {
        val cases = resource("hub-descriptor-cases.json")
        val golden = resource("hub-descriptor-golden.json")
        val defs = cases["cases"]!!.jsonObject
        assertTrue(defs.size >= 22)
        for ((name, c) in defs) {
            val blob = NppsCore.compile(c.jsonObject["def"]!!.jsonObject, options(cases, true))
            assertEquals(c.jsonObject["hex"]!!.jsonPrimitive.content, hex(blob), name)
            assertEquals(golden[name]!!.jsonPrimitive.content, hex(blob), "$name (the file the other runtimes read)")
        }
    }

    @Test
    fun everyRefusalReadsAsTheWebCompilersDoes() {
        val cases = resource("hub-descriptor-cases.json")
        val errors = cases["errors"]!!.jsonObject.filterValues { it.jsonObject["message"] !is JsonNull }
        assertTrue(errors.size >= 15)
        for ((name, c) in errors) {
            val with = !c.jsonObject["noClinicianSockets"]!!.jsonPrimitive.content.toBoolean()
            val e = assertFailsWith<IllegalArgumentException>(name) {
                NppsCore.compile(c.jsonObject["def"]!!.jsonObject, options(cases, with))
            }
            assertEquals(c.jsonObject["message"]!!.jsonPrimitive.content, e.message, name)
        }
    }

    @Test
    fun aShippedProtocolParsesThenCompiles() {
        val cases = resource("hub-descriptor-cases.json")
        val src = File(root, "protocols/predefined/01-gamma-focus.npps").readText()
        val proto = NppsCore.parse(src).first { it.jsonObject["kind"]!!.jsonPrimitive.content == "single" }
            .jsonObject["protocol"]!!.jsonObject
        val blob = NppsCore.compile(proto, options(cases, true))
        assertEquals(0x50, blob[0].toInt() and 0xFF, "NP_HUB_PROTO_MAGIC low byte")
        assertTrue(blob.size > 128)
        assertTrue(blob.takeLast(64).all { it.toInt() == 0 }, "the signature slot is zeroed for the caller to fill")
    }

    @Test
    fun aMalformedRequestIsRefusedNotCrashed() {
        val e = assertFailsWith<IllegalArgumentException> {
            NppsCore.compile(JsonObject(emptyMap()), options(resource("hub-descriptor-cases.json"), true))
        }
        assertTrue(e.message!!.isNotEmpty())
    }

}
