package life.neurone.core.session

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** The Kotlin side of the fit check marshals and decides nothing; the rule is tested in Rust (common/npps-core/tests/fit.rs). */
class ModuleFitTests {
    private fun golden(name: String): ByteArray {
        val text = javaClass.getResourceAsStream("/hub-descriptor-golden.json")!!.bufferedReader().readText()
        val hex = Json.parseToJsonElement(text).jsonObject[name]!!.jsonPrimitive.content
        return ByteArray(hex.length / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
    }

    // @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21)
    @Test fun a_helmet_with_no_modules_is_short_everywhere_and_a_1064_tile_everywhere_fits() {
        val blob = golden("pbm1064")
        val bare = ModuleFit.check(blob, emptyMap())
        assertTrue(bare.isNotEmpty() && bare.all { it.empty })
        assertTrue(bare.all { s -> s.missing.any { it.element == "led_1064" && "ZM-PBM-TRI" in it.candidateParts } })
        val everywhere = bare.associate { it.socket to 3 }
        assertEquals(emptyList(), ModuleFit.check(blob, everywhere))
        assertEquals(bare.size, ModuleFit.check(blob, bare.associate { it.socket to 1 }).size)
    }
}
