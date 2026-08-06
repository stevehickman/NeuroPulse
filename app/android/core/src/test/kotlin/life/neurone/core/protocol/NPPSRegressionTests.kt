package life.neurone.core.protocol

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue
import kotlin.test.fail

/**
 * NPPS engine regression tests — Kotlin port of the four iOS tests
 * (NPProtocolLibraryTests) that lock in the five NPPS lexer/serializer bug
 * fixes (project docs/status/completed-decisions.md), plus additional round-trip coverage.
 */
class NPPSRegressionTests {

    // Helpers -----------------------------------------------------------------

    private fun lex(text: String): List<NPPSLexeme> = NPPSLexer(text).tokenize()

    private fun parse(text: String): List<NPProtocolEntry> =
        NPPSParser(NPPSLexer(text).tokenize()).parse()

    private fun singleProtocol(text: String): NPProtocolDefinition {
        val entries = parse(text)
        assertEquals(1, entries.size, "expected exactly one entry")
        val entry = entries[0]
        assertTrue(entry is NPProtocolEntry.Single, "expected a single protocol entry")
        return entry.protocol
    }

    private fun pbmTranscranial(proto: NPProtocolDefinition): NPPBMTranscranialParams {
        val mod = proto.modalities.firstOrNull {
            it.modalityType == NPModalityType.PBM_TRANSCRANIAL
        } ?: fail("no pbm_transcranial modality present")
        val params = mod.params
        assertTrue(params is NPModalityParams.PbmTranscranial)
        return params.params
    }

    // MARK: - Mirrors of the four iOS tests -----------------------------------

    /** testLexerCompoundDigitIdentRoundTrip — 660_808nm + mW_cm2 in a pbm block. */
    @Test
    fun testLexerCompoundDigitIdentRoundTrip() {
        val script = """
            protocol "Compound Idents" {
                version: "1.0"
                pbm_transcranial {
                    intensity: 75%
                    frequency: 40Hz
                    duty_cycle: 25%
                    wavelength: 660_808nm
                }
            }
        """.trimIndent()

        // Bug fix (1): 660_808nm must lex as a single ident, not number + field.
        // Bug fix (3): mW_cm2 must lex as a recognized unit on a number.
        val proto = singleProtocol(script)
        val p = pbmTranscranial(proto)
        assertEquals(
            NPPBMTranscranialParams.Wavelength.BASE_660_808NM,
            p.wavelength,
            "660_808nm should resolve to the base wavelength",
        )

        // Verify the unit token exists in the lexer output for a mW_cm2 value.
        val tokens = lex("300mW_cm2")
        val unitTok = tokens.map { it.token }.firstOrNull { it is NPPSToken.NumberWithUnit }
        assertNotNull(unitTok, "300mW_cm2 should produce a NumberWithUnit token")
        unitTok as NPPSToken.NumberWithUnit
        assertEquals(300.0, unitTok.value)
        assertEquals("mW_cm2", unitTok.unit)

        // 660_808nm on its own lexes as a single Ident.
        val identTokens = lex("660_808nm")
        val ident = identTokens.map { it.token }.firstOrNull { it is NPPSToken.Ident }
        assertNotNull(ident, "660_808nm should produce a single Ident token")
        assertEquals("660_808nm", (ident as NPPSToken.Ident).value)
    }

    /** testLexerTriWavelengthCompoundIdent — 660_808_1064nm. */
    @Test
    fun testLexerTriWavelengthCompoundIdent() {
        val script = """
            protocol "Tri Wavelength" {
                version: "1.0"
                pbm_transcranial {
                    intensity: 80%
                    frequency: 40Hz
                    duty_cycle: 25%
                    wavelength: 660_808_1064nm
                }
            }
        """.trimIndent()

        val proto = singleProtocol(script)
        val p = pbmTranscranial(proto)
        assertEquals(
            NPPBMTranscranialParams.Wavelength.TRI_660_808_1064,
            p.wavelength,
            "660_808_1064nm should resolve to the tri wavelength",
        )

        val ident = lex("660_808_1064nm").map { it.token }
            .firstOrNull { it is NPPSToken.Ident }
        assertNotNull(ident, "660_808_1064nm should produce a single Ident token")
        assertEquals("660_808_1064nm", (ident as NPPSToken.Ident).value)
    }

    /** testHyphenatedTagsDoNotThrow — wind-down, all-modalities parse cleanly. */
    @Test
    fun testHyphenatedTagsDoNotThrow() {
        val script = """
            protocol "Hyphen Tags" {
                version: "1.0"
                tags: ["wind-down", "all-modalities"]
                pbm_transcranial {
                    intensity: 60%
                    frequency: 10Hz
                    duty_cycle: 25%
                }
            }
        """.trimIndent()

        // Bug fix (2): a '-' not followed by a digit must not enter the number
        // branch (would throw "Invalid number: -"). Quoted here, they are strings.
        val proto = singleProtocol(script)
        assertTrue(proto.tags.contains("wind-down"))
        assertTrue(proto.tags.contains("all-modalities"))

        // Also confirm the bare (unquoted) form does not throw during lexing —
        // the hyphen is skipped as an unknown char rather than a bad number.
        val bareTokens = lex("wind-down all-modalities")
        assertTrue(
            bareTokens.any { it.token is NPPSToken.Ident },
            "bare hyphenated text should lex without throwing",
        )
    }

    /** testHyphenatedTagRoundTripPreservesValue — serialize→reparse keeps one tag. */
    @Test
    fun testHyphenatedTagRoundTripPreservesValue() {
        val script = """
            protocol "Round Trip" {
                version: "1.0"
                tags: ["wind-down"]
                pbm_transcranial {
                    intensity: 60%
                    frequency: 10Hz
                    duty_cycle: 25%
                }
            }
        """.trimIndent()

        val first = parse(script)
        val serialized = NPPSSerializer().serialize(first[0])

        // Bug fix (5): the serializer must quote "wind-down" so it survives a
        // reparse as a single tag rather than splitting on the hyphen.
        assertTrue(
            serialized.contains("\"wind-down\""),
            "serialized output should quote the hyphenated tag:\n$serialized",
        )

        val reparsed = parse(serialized)
        assertEquals(1, reparsed.size)
        val entry = reparsed[0]
        assertTrue(entry is NPProtocolEntry.Single)
        assertEquals(
            listOf("wind-down"),
            entry.protocol.tags,
            "wind-down must survive round-trip as exactly one tag",
        )
    }

    // MARK: - Additional round-trip / fidelity tests --------------------------

    /** Bug fix (4): a quoted wavelength value resolves via asIdent. */
    @Test
    fun testQuotedWavelengthResolvesViaAsIdent() {
        val script = """
            protocol "Quoted Wavelength" {
                version: "1.0"
                pbm_transcranial {
                    intensity: 75%
                    frequency: 40Hz
                    duty_cycle: 25%
                    wavelength: "660_808nm"
                }
            }
        """.trimIndent()

        val p = pbmTranscranial(singleProtocol(script))
        assertEquals(NPPBMTranscranialParams.Wavelength.BASE_660_808NM, p.wavelength)
    }

    /** Units, percents, and time values parse into the expected fields. */
    @Test
    fun testUnitsAndTimeParse() {
        val script = """
            protocol "Units" {
                version: "1.0"
                duration: 20m
                bes_tacs {
                    frequency: 10Hz
                    intensity: 0.8mA
                    waveform: square
                }
                vns_hrv {
                    frequency: 25Hz
                    intensity: 1.5mA
                    hrv_protocol: tavns_sync
                    breathing_rate: 6
                }
            }
        """.trimIndent()

        val proto = singleProtocol(script)
        val timing = proto.timingMode
        assertTrue(timing is NPTimingMode.Duration)
        assertEquals(1200, timing.seconds, "20m should be 1200 seconds")

        val bes = proto.modalities.first { it.modalityType == NPModalityType.BES_TACS }.params
        assertTrue(bes is NPModalityParams.BesTacs)
        assertEquals(10.0, bes.params.frequencyHz)
        assertEquals(0.8, bes.params.intensityMilliamps)
        assertEquals(NPBESTacsParams.Waveform.SQUARE, bes.params.waveform)

        val vns = proto.modalities.first { it.modalityType == NPModalityType.VNS_HRV }.params
        assertTrue(vns is NPModalityParams.VnsHRV)
        assertEquals(NPVNSHRVParams.HRVProtocol.TAVNS_SYNC, vns.params.hrvProtocol)
    }

    /** Custom zones are 1-indexed in script and stored 0-indexed. */
    @Test
    fun testCustomZonesAreReindexed() {
        val script = """
            protocol "Custom Zones" {
                version: "1.0"
                pbm_transcranial {
                    intensity: 75%
                    frequency: 40Hz
                    duty_cycle: 25%
                    zones: [1, 3, 5]
                }
            }
        """.trimIndent()

        val p = pbmTranscranial(singleProtocol(script))
        assertEquals(NPPBMTranscranialParams.ZoneSelection.CUSTOM, p.zones)
        assertEquals(listOf(0, 2, 4), p.customZones, "script zones are 1-indexed, stored 0-indexed")

        // Round-trip: serialize back to 1-indexed [1, 3, 5].
        val out = NPPSSerializer().serialize(NPProtocolEntry.Single(singleProtocol(script)))
        assertTrue(out.contains("zones: [1, 3, 5]"), "serialized zones should be 1-indexed:\n$out")
    }

    /** A limits block round-trips through parse→serialize→parse. */
    @Test
    fun testLimitsBlockRoundTrip() {
        val script = """
            limits "Clinic Default" {
                level: global
                description: "House ceiling"

                pbm_transcranial {
                    max_intensity: 90%
                    max_frequency: 40Hz
                    max_duty_cycle: 25%
                }
                bes_tacs {
                    max_intensity: 1.0mA
                    max_frequency: 40Hz
                    min_frequency: 0.5Hz
                }
            }
        """.trimIndent()

        val entry = parse(script).single()
        assertTrue(entry is NPProtocolEntry.Limits)
        val serialized = NPPSSerializer().serialize(entry)

        val reparsed = parse(serialized).single()
        assertTrue(reparsed is NPProtocolEntry.Limits)
        val lim = reparsed.limits
        assertEquals("Clinic Default", lim.name)
        assertEquals(NPLimitsSet.LimitLevel.GLOBAL, lim.level)
        assertEquals(90.0, lim.pbmTranscranial?.maxIntensityPercent)
        assertEquals(40.0, lim.pbmTranscranial?.maxFrequencyHz)
        assertEquals(1.0, lim.besTacs?.maxIntensityMilliamps)
        assertEquals(0.5, lim.besTacs?.minFrequencyHz)
    }

    /** A composite with layers round-trips, including start/duration/scale. */
    @Test
    fun testCompositeRoundTrip() {
        val script = """
            composite "Evening" {
                version: "1.0"
                conflict_resolution: sequential
                layer "Alpha Calm" {
                    start: 0s
                    duration: 10m
                }
                layer "Sleep Deep" {
                    start: 10m
                    duration: 20m
                    intensity_scale: 0.5
                }
            }
        """.trimIndent()

        val entry = parse(script).single()
        assertTrue(entry is NPProtocolEntry.Composite)
        val serialized = NPPSSerializer().serialize(entry)
        val reparsed = parse(serialized).single()
        assertTrue(reparsed is NPProtocolEntry.Composite)
        val comp = reparsed.composite
        assertEquals(NPCompositeProtocol.ConflictResolution.SEQUENTIAL, comp.conflictResolution)
        assertEquals(2, comp.layers.size)
        assertEquals(0, comp.layers[0].startOffsetSeconds)
        assertEquals(600, comp.layers[0].durationSeconds)
        assertEquals(600, comp.layers[1].startOffsetSeconds)
        assertEquals(1200, comp.layers[1].durationSeconds)
        assertEquals(0.5, comp.layers[1].intensityScale)
    }

    /** CW frequency (0Hz) serializes with the "# CW" comment and reparses to 0. */
    @Test
    fun testCwFrequencyRoundTrip() {
        val script = """
            protocol "Vascular" {
                version: "1.0"
                pbm_transcranial {
                    intensity: 50%
                    frequency: 0Hz
                }
            }
        """.trimIndent()

        val serialized = NPPSSerializer().serialize(NPProtocolEntry.Single(singleProtocol(script)))
        assertTrue(serialized.contains("frequency: 0Hz"), "CW frequency should serialize as 0Hz")
        val p = pbmTranscranial(singleProtocol(serialized))
        assertEquals(0.0, p.frequencyHz)
    }

    /** An unknown modality inside a protocol throws NPPSError; unknown limits sub-blocks are ignored. */
    @Test
    fun testUnknownModalityThrowsButUnknownLimitsIgnored() {
        val badProtocol = """
            protocol "Bad" {
                version: "1.0"
                not_a_real_modality {
                    intensity: 50%
                }
            }
        """.trimIndent()
        try {
            parse(badProtocol)
            fail("unknown modality should throw NPPSError")
        } catch (e: NPPSError) {
            assertTrue(e.messageText.contains("Unknown modality"))
        }

        val forwardCompatLimits = """
            limits "Forward" {
                level: global
                future_modality {
                    max_intensity: 10mA
                }
                tdcs {
                    max_intensity: 2.0mA
                }
            }
        """.trimIndent()
        val entry = parse(forwardCompatLimits).single()
        assertTrue(entry is NPProtocolEntry.Limits)
        assertEquals(2.0, entry.limits.tdcs?.maxIntensityMilliamps)
    }

    /** An empty protocol name throws with the correct message. */
    @Test
    fun testEmptyNameThrows() {
        try {
            parse("protocol \"\" { version: \"1.0\" }")
            fail("empty protocol name should throw")
        } catch (e: NPPSError) {
            assertTrue(e.messageText.contains("cannot be empty"))
        }
    }
}
