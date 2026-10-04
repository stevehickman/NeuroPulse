package life.neurone.core.protocol

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.double
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * The shared cross-platform fixtures in npps/fixtures (NP-NPPS-GRAM-001 Rev 6): every
 * `error_*.npps` must be refused, and every `.expected.json` must parse to the same
 * modalities and, for the dose-bearing blocks, the same absolute values the web reference
 * produces. Checks the fields Rev 18 changed (wavelength, irradiance, level), not the whole
 * normalised shape.
 */
class NPPSSharedFixtureTests {

    private val dir: File = generateSequence(File("").absoluteFile) { it.parentFile }
        .map { File(it, "npps/fixtures") }
        .first { it.isDirectory }

    private fun parse(text: String): List<NPProtocolEntry> =
        NPPSParser(NPPSLexer(text).tokenize()).parse()

    @Test
    fun everyErrorFixtureIsRefused() {
        val files = dir.listFiles { f -> f.name.startsWith("error_") && f.name.endsWith(".npps") }!!
        assertTrue(files.size >= 9, "the Rev 6 error fixtures must be present")
        for (f in files) {
            assertFailsWith<NPPSError>(f.name) { parse(f.readText()) }
        }
    }

    @Test
    fun everyExpectedFixtureParsesToTheSameDoses() {
        val expectedFiles = dir.listFiles { f -> f.name.endsWith(".expected.json") }!!
        assertTrue(expectedFiles.any { it.name == "absolute_quantities.expected.json" })
        for (ef in expectedFiles) {
            val expected = Json.parseToJsonElement(ef.readText()).jsonObject
            if (expected["kind"]?.jsonPrimitive?.content != "single") continue
            val src = File(dir, ef.name.removeSuffix(".expected.json") + ".npps").readText()
            val proto = (parse(src).single() as NPProtocolEntry.Single).protocol
            val mods = expected["protocol"]!!.jsonObject["modalities"]!!.jsonArray
            assertEquals(mods.size, proto.modalities.size, ef.name)
            for ((i, m) in mods.withIndex()) {
                val exp: JsonObject = m.jsonObject["params"]!!.jsonObject
                when (val p = proto.modalities[i].params) {
                    is NPModalityParams.PbmTranscranial -> {
                        assertEquals(exp["wavelength"]!!.jsonPrimitive.content, p.params.wavelength.rawValue, ef.name)
                        assertEquals(exp["irradianceMWcm2"]!!.jsonPrimitive.double, p.params.irradianceMWcm2, ef.name)
                    }
                    is NPModalityParams.PbmIntranasal -> {
                        assertEquals(exp["wavelength"]!!.jsonPrimitive.content, p.params.wavelength.rawValue, ef.name)
                        assertEquals(exp["irradianceMWcm2"]!!.jsonPrimitive.double, p.params.irradianceMWcm2, ef.name)
                    }
                    is NPModalityParams.AudioEntrainment ->
                        assertEquals(exp["volumeDb"]!!.jsonPrimitive.double, p.params.volumeDb, ef.name)
                    else -> {}
                }
            }
        }
    }
}
