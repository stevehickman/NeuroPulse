package life.neurone.core.npps

import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.long
import life.neurone.core.platform.toHex
import life.neurone.core.protocol.BundledProtocolFiles
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * The shared NPPS core through whichever binding this target has: JNI on the JVM, WebAssembly in the browser
 * and Node, the C ABI on Apple. The goldens are the web parser's and compiler's own output, and the Rust crate is
 * already diffed against them, so what this adds is each binding's marshalling: UTF-8 in both directions (the
 * messages hold `—` and `²`), the bytes of a descriptor, and the mapping of a refusal to an exception. The same
 * file runs on every target, so a binding cannot pass here and differ there. (`NppsCoreTests` in jvmTest covers
 * the rest of the API on the JVM.)
 */
class NppsBackendParityTests {

    private fun golden(text: String): JsonObject = Json.parseToJsonElement(text).jsonObject

    private val parseKeys = listOf("entries", "zones", "conditions", "wavelengthRules", "limits")

    /** What the web parser's `parseNPPSFile` and `parseNPPSLimits` reduce to: it keeps only the first `limits` block. */
    private fun reduce(parsed: JsonObject): JsonObject =
        JsonObject(parsed + ("limits" to (parsed["limits"]!!.jsonArray.firstOrNull() ?: JsonNull)))

    private fun checkParse(name: String, source: String, expected: JsonObject, failures: MutableList<String>) {
        val want = expected["error"]
        try {
            val got = reduce(NppsCore.parse(source))
            if (want != null && want !is JsonNull) failures += "$name: accepted what the web parser refuses: $want"
            else for (key in parseKeys) if (got[key] != expected[key]) failures += "$name: $key differ"
        } catch (e: IllegalArgumentException) {
            if (want == null || want is JsonNull) failures += "$name: refused what the web parser accepts: ${e.message}"
            else if (e.message != want.jsonPrimitive.content) {
                failures += "$name: message differs\n  core: ${e.message}\n  web:  ${want.jsonPrimitive.content}"
            }
        }
    }

    @Test
    fun theWholeShippedLibraryParsesAsTheWebParserDoes() = runTest {
        prepareNppsCore()
        val files = golden(TestGoldens.parse)["files"]!!.jsonObject
        assertTrue(files.size > 60, "the golden covers the shipped library")
        val failures = mutableListOf<String>()
        for ((relative, expected) in files) {
            val source = if (relative.startsWith("protocols/predefined/")) {
                BundledProtocolFiles.files[relative.removePrefix("protocols/predefined/")]
            } else {
                TestGoldens.fixtures[relative]
            }
            if (source == null) failures += "$relative: not bundled"
            else checkParse(relative, source, expected.jsonObject, failures)
        }
        assertTrue(failures.isEmpty(), "${failures.size} divergence(s):\n" + failures.joinToString("\n"))
    }

    @Test
    fun theRefusalAndAliasCorpusParsesAsTheWebParserDoes() = runTest {
        prepareNppsCore()
        val cases = golden(TestGoldens.parse)["cases"]!!.jsonObject
        val failures = mutableListOf<String>()
        for ((name, case) in cases) checkParse(name, case.jsonObject["source"]!!.jsonPrimitive.content, case.jsonObject, failures)
        assertTrue(failures.isEmpty(), "${failures.size} divergence(s):\n" + failures.joinToString("\n"))
    }

    private fun options(cases: JsonObject, withClinician: Boolean) = NppsCore.CompileOptions(
        zones = cases["zones"]!!.jsonObject.mapValues { (_, v) -> v.jsonArray.map { it.jsonPrimitive.int } },
        clinicianSockets = if (withClinician) cases["clinicianSockets"]!!.jsonArray.map { it.jsonPrimitive.int } else null,
        nowUnix = cases["compiledAt"]!!.jsonPrimitive.long,
        sessionUuid = ByteArray(16) { cases["sessionUuidByte"]!!.jsonPrimitive.int.toByte() },
    )

    @Test
    fun everyGoldenDefinitionCompilesToTheWebCompilersBytes() = runTest {
        prepareNppsCore()
        val cases = golden(TestGoldens.hubCases)
        val expected = golden(TestGoldens.hubGolden)
        val definitions = cases["cases"]!!.jsonObject
        assertTrue(definitions.size >= 22)
        for ((name, case) in definitions) {
            val blob = NppsCore.compile(case.jsonObject["def"]!!.jsonObject, options(cases, true))
            assertEquals(case.jsonObject["hex"]!!.jsonPrimitive.content, blob.toHex(), name)
            assertEquals(expected[name]!!.jsonPrimitive.content, blob.toHex(), "$name (the file the other runtimes read)")
        }
    }

    @Test
    fun everyRefusalReadsAsTheWebCompilersDoes() = runTest {
        prepareNppsCore()
        val cases = golden(TestGoldens.hubCases)
        val errors = cases["errors"]!!.jsonObject.filterValues { it.jsonObject["message"] !is JsonNull }
        assertTrue(errors.size >= 15)
        for ((name, case) in errors) {
            val withClinician = !case.jsonObject["noClinicianSockets"]!!.jsonPrimitive.content.toBoolean()
            val refusal = assertFailsWith<IllegalArgumentException>(name) {
                NppsCore.compile(case.jsonObject["def"]!!.jsonObject, options(cases, withClinician))
            }
            assertEquals(case.jsonObject["message"]!!.jsonPrimitive.content, refusal.message, name)
        }
    }

    @Test
    fun theBundledLibraryBuildsANamespaceWithoutErrors() = runTest {
        prepareNppsCore()
        assertTrue(life.neurone.core.protocol.NPBundledProtocols.namespace.entries.isNotEmpty())
    }
}

/** Make the NPPS core callable on this target before a test uses it (a no-op where it is linked in). */
internal expect suspend fun prepareNppsCore()
