package life.neurone.core.protocol

import life.neurone.core.session.HubDescriptorCompiler
import life.neurone.core.session.SignatureResult
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * NP-NPPS-REF-001 Rev 17 §4.1a / §7a on Android. Mirrors the web reference tests
 * (common/lib/wavelengthRules.test.ts) for the cases this runtime acts on.
 */
class NPWavelengthRulesTests {

    private val rules = NPWavelengthRulesEngine.DEFAULT

    private fun parse(text: String): List<NPProtocolEntry> =
        NPPSParser.parse(text)

    private val SERIES = """
        protocol "Series" {
            duration: 8m
            pbm_transcranial {
                wavelength: "810nm"
                irradiance: 250mW_cm2
                frequency: 0Hz
                duty_cycle: 100%
                zones: ["Frontal Right"]
                start: 4m
                interval_on: 4m
                interval_off: 0s
                repeat: 1
            }
        }
        wavelength_rules "Lab" {
            level: user
            channel "led_808" { nominal_nm: 808  min_nm: 798  max_nm: 860 }
        }
    """.trimIndent()

    @Test
    fun defaultsMapTheSourceWavelengths() {
        assertEquals(NPPBMChannelElement.LED_660, NPWavelengthRulesEngine.map(660.0, rules))
        assertEquals(NPPBMChannelElement.LED_808, NPWavelengthRulesEngine.map(810.0, rules))
        assertEquals(NPPBMChannelElement.LED_808, NPWavelengthRulesEngine.map(830.0, rules))
        assertEquals(NPPBMChannelElement.LED_1064, NPWavelengthRulesEngine.map(1070.0, rules))
        for (nm in listOf(640.0, 850.0, 1080.0)) assertNull(NPWavelengthRulesEngine.map(nm, rules), "$nm")
    }

    @Test
    fun unmappedAndInvalidAreRefused() {
        val r = NPWavelengthRulesEngine.resolveChannels("850nm")
        assertTrue(r is NPPbmChannelResolution.Refused && r.reason == "unmapped")
        for (bad in listOf("red", "810", "810 nm", ".5nm", "0nm")) {
            val b = NPWavelengthRulesEngine.resolveChannels(bad)
            assertTrue(b is NPPbmChannelResolution.Refused && b.reason == "invalid", bad)
        }
    }

    @Test
    fun aUserRuleReplacesOnlyItsChannel() {
        val user = NPWavelengthRules(
            name = "Lab", level = "user",
            channels = listOf(NPWavelengthChannelRule(NPPBMChannelElement.LED_808, 808.0, 798.0, 860.0)),
        )
        val resolved = NPWavelengthRulesEngine.resolve(rules, user)
        assertEquals(NPPBMChannelElement.LED_808, NPWavelengthRulesEngine.map(850.0, resolved))
        assertEquals(rules.channels[0], resolved.channels[0])
        assertEquals(rules.channels[2], resolved.channels[2])
    }

    @Test
    fun scriptWavelengthIsCarriedExactlyAndStartRoundTrips() {
        val entries = parse(SERIES)
        assertEquals(1, entries.size, "wavelength_rules is accepted and is not an entry")
        val proto = (entries[0] as NPProtocolEntry.Single).protocol
        val mod = proto.modalities[0]
        val p = (mod.params as NPModalityParams.PbmTranscranial).params
        assertEquals("810nm", p.wavelength.rawValue, "never replaced by a default")
        assertEquals(240, mod.interval.startOffsetSeconds)
        assertFalse(p.wavelength.requiresSmartModule)
        assertTrue(NPPBMTranscranialParams.Wavelength("1070nm").requiresSmartModule)

        val text = NPPSSerializer().serialize(entries[0])
        assertTrue(text.contains("start: 4m"), text)
        val again = (parse(text)[0] as NPProtocolEntry.Single).protocol
        assertEquals(240, again.modalities[0].interval.startOffsetSeconds)
    }

    @Test
    fun negativeStartIsAnError() {
        assertFailsWith<NPPSError> { parse(SERIES.replace("start: 4m", "start: -4m")) }
    }

    private fun compile(d: NPProtocolDefinition) =
        HubDescriptorCompiler({ SignatureResult(ByteArray(64), "00") }).compile(d)

    @Test
    fun theSessionWireCarriesBlockStartAndRefusesTheUnmapped() {
        // `start` is on the wire now: the binary descriptor has a start_ms per command (§4.1).
        val proto = (parse(SERIES)[0] as NPProtocolEntry.Single).protocol
        assertNotNull(compile(proto))

        val unmapped = (parse(SERIES.replace("start: 4m\n", "").replace("810nm", "850nm"))[0]
            as NPProtocolEntry.Single).protocol
        assertFailsWith<IllegalArgumentException> { compile(unmapped) }

        val ok = (parse(SERIES.replace("start: 4m\n", ""))[0] as NPProtocolEntry.Single).protocol
        assertNotNull(compile(ok))
    }

    @Test
    fun defaultsMatchTheShippedRulesFile() {
        val text = assertNotNull(
            NPBundledProtocols.readResource("/protocols/predefined/00-wavelength-rules.npps"),
            "00-wavelength-rules.npps is bundled",
        )
        val channel = Regex(
            """channel\s+"(\w+)"\s*\{\s*nominal_nm:\s*([0-9.]+)\s*min_nm:\s*([0-9.]+)\s*max_nm:\s*([0-9.]+)\s*}""",
        )
        val fromFile = channel.findAll(text).map { m ->
            NPWavelengthChannelRule(
                assertNotNull(NPPBMChannelElement.fromRaw(m.groupValues[1])),
                m.groupValues[2].toDouble(), m.groupValues[3].toDouble(), m.groupValues[4].toDouble(),
            )
        }.toList()
        assertEquals(rules.channels, fromFile)
    }
}
