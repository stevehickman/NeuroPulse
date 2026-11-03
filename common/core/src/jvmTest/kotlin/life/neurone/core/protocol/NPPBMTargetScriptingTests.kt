package life.neurone.core.protocol

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFails
import kotlin.test.assertIs
import kotlin.test.assertTrue

/**
 * Parsing the `zones:` field. The bug these guard (iOS `NPPBMTargetScriptingTests`): a parser that mapped every selector it did not
 * recognise onto "every module", so `zones: ["Frontal Right (excl. midline)"]` silently became a whole-vault session with no error.
 * A target the parser does not understand must stop the file, not become a second way to express a target (wrong-site stimulation).
 */
class NPPBMTargetScriptingTests {

    private fun target(zonesLine: String, extra: String = ""): NPPBMTarget {
        val source = """
            protocol "Target Test" {
                id: "AABB0100-0000-0000-0000-000000000000"
                description: "Target parsing"
                author: "Test"
                version: "1.0"
                readonly: false
                duration: 10m
                pbm_transcranial {
                    irradiance: 322mW_cm2
                    frequency: 40Hz
                    duty_cycle: 25%
                    $zonesLine
            $extra
                    wavelength: "808nm"
                }
            }
        """.trimIndent()
        val entry = NPPSParser.parse(source).first()
        val protocol = assertIs<NPProtocolEntry.Single>(entry).protocol
        return assertIs<NPModalityParams.PbmTranscranial>(protocol.modalities.first().params).params.target
    }

    // @verifies REQ-NPPS-08
    @Test fun namedZonesParseAsNamedReferences() {
        assertEquals(NPPBMTarget.Named(listOf("Frontal Right (excl. midline)")), target("""zones: ["Frontal Right (excl. midline)"]"""))
        assertEquals(NPPBMTarget.Named(listOf("Frontal Left", "Frontal Right")), target("""zones: ["Frontal Left", "Frontal Right"]"""))
        assertEquals(NPPBMTarget.ClinicianSelected, target("zones: clinician_selected"))
    }

    // @verifies REQ-NPPS-08
    @Test fun aLateralizedZoneDoesNotBecomeEveryModule() {
        val sockets = target("""zones: ["Frontal Right (excl. midline)"]""").resolveSockets()
        // Frontal Right minus the midline sockets 2, 13 and 29: every one of these has x > 0.
        assertEquals(listOf(3, 7, 8, 9, 14, 15, 16, 21, 22, 23, 24, 30, 31, 32, 38, 39, 40), sockets)
        assertTrue(sockets.size < SocketLattice.NP_SOCKET_COUNT, "a lateralized target must not light the whole vault")
    }

    // @verifies REQ-NPPS-08
    @Test fun theFiveSlotSelectorsNoLongerParse() {
        for (selector in listOf("all", "front", "rear", "custom")) assertFails("'$selector' must not parse") { target("zones: $selector") }
        assertFails { target("zones: custom", extra = "            custom_zones: [3, 4, 5]") }
    }

    // @verifies REQ-NPPS-08
    @Test fun aNumericOrMixedOrEmptyZoneListIsRefused() {
        assertFails { target("zones: [1, 2]") }
        assertFails { target("zones: [0]") }
        assertFails { target("""zones: ["Frontal", 2]""") }
        assertFails { target("zones: []") }
    }

    // @verifies REQ-NPPS-08
    @Test fun anUnrecognisedSelectorIsRefusedAndNamedNotDefaultedToAll() {
        val e = assertFails { target("zones: whole_head") }
        assertTrue("whole_head" in (e.message ?: ""), "the error must name the unrecognised selector: ${e.message}")
    }
}
