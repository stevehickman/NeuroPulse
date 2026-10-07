package life.neurone.core.protocol

import life.neurone.core.session.HubDescriptorCompiler
import life.neurone.core.session.ProtocolSigner
import life.neurone.core.session.SignatureResult
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * NP-NPPS-REF-001 Rev 18 §4.1b, §4.7: PBM irradiance and audio level are absolute, and every
 * PBM wavelength is its own block. Mirrors app/web/src/lib/npps.test.ts and the shared
 * fixtures in npps/fixtures (the five error_* files and absolute_quantities).
 */
class NPPSAbsoluteQuantityTests {
    private val compiler = HubDescriptorCompiler({ life.neurone.core.session.SignatureResult(ByteArray(64), "00") })


    private fun parse(text: String): List<NPProtocolEntry> =
        NPPSParser.parse(text)

    private fun proto(body: String) = "protocol \"T\" {\n    duration: 5m\n$body\n}\n"

    private fun single(text: String): NPProtocolDefinition =
        (parse(text).single() as NPProtocolEntry.Single).protocol

    private fun refused(body: String): String =
        assertFailsWith<NPPSError> { parse(proto(body)) }.message.orEmpty()

    private fun pbm(fields: String) = "    pbm_transcranial {\n$fields\n    }"

    @Test
    fun readsIrradianceWithItsUnit() {
        val p = single(proto(pbm("wavelength: \"808nm\"\nirradiance: 36mW_cm2")))
        val params = (p.modalities.single().params as NPModalityParams.PbmTranscranial).params
        assertEquals(36.0, params.irradianceMWcm2)
        assertEquals("808nm", params.wavelength.rawValue)
    }

    @Test
    fun acceptsTheCanonicalNameWithABareNumber() {
        val p = single(proto(pbm("wavelength: \"808nm\"\nirradiance_mw_cm2: 36")))
        val params = (p.modalities.single().params as NPModalityParams.PbmTranscranial).params
        assertEquals(36.0, params.irradianceMWcm2)
    }

    @Test
    fun refusesPercentagesAndUnitlessDoses() {
        assertTrue(refused(pbm("intensity: 80%\nwavelength: \"808nm\"")).contains("percentage of a baseline"))
        assertTrue(refused(pbm("intensity_percent: 80\nwavelength: \"808nm\"")).contains("percentage of a baseline"))
        assertTrue(refused(pbm("wavelength: \"808nm\"\nirradiance: 80%")).contains("in % is refused"))
        assertTrue(refused(pbm("wavelength: \"808nm\"\nirradiance: 80")).contains("needs its unit written"))
        assertTrue(refused(pbm("wavelength: \"808nm\"\nirradiance: 80mA")).contains("takes mW_cm2, not mA"))
    }

    @Test
    fun requiresIrradianceAndWavelengthRatherThanDefaultingThem() {
        assertTrue(refused(pbm("wavelength: \"808nm\"")).contains("irradiance (e.g. irradiance: 300mW_cm2) is required"))
        assertTrue(refused(pbm("irradiance: 300mW_cm2")).contains("wavelength is required"))
    }

    @Test
    fun refusesTheRetiredCombinedWavelengthsAndNamesTheReplacement() {
        for (wl in listOf("660_808nm", "660_808_1064nm")) {
            val msg = refused(pbm("wavelength: \"$wl\"\nirradiance: 300mW_cm2"))
            assertTrue(msg.contains("retired") && msg.contains("one block per wavelength"), msg)
        }
    }

    @Test
    fun intranasalBlocksCarryAWavelengthAndAnIrradiance() {
        val p = single(proto("    pbm_intranasal {\n        wavelength: \"660nm\"\n        irradiance: 25mW_cm2\n    }"))
        val params = (p.modalities.single().params as NPModalityParams.PbmIntranasal).params
        assertEquals(25.0, params.irradianceMWcm2)
        assertEquals("660nm", params.wavelength.rawValue)
        assertTrue(refused("    pbm_intranasal {\n        intensity: 60%\n    }").contains("percentage of a baseline"))
    }

    @Test
    fun readsAudioInDbAndRefusesAPercentage() {
        fun audio(body: String) = "    audio_entrainment {\n        carrier_hz: 440Hz\n        $body\n    }"
        val p = single(proto(audio("volume: 72.5dB")))
        assertEquals(72.5, (p.modalities.single().params as NPModalityParams.AudioEntrainment).params.volumeDb)
        assertTrue(refused(audio("volume: 70%")).contains("in % is refused"))
        assertTrue(refused(audio("volume: 70")).contains("needs its unit written"))
        assertTrue(refused(audio("volume_percent: 70")).contains("percentage of a baseline"))
        assertTrue(refused(audio("binaural_hz: 10Hz")).contains("is required and must be positive"))
    }

    @Test
    fun refusesAPercentageCeilingInLimitsInsteadOfSkippingIt() {
        for (mod in listOf("pbm_transcranial", "pbm_intranasal", "audio_entrainment")) {
            val e = assertFailsWith<NPPSError> {
                parse("limits \"L\" {\n    level: global\n    $mod {\n        max_intensity: 80\n    }\n}\n")
            }
            assertTrue(e.message.orEmpty().contains("percentage ceiling and is retired"), e.message)
        }
        val lim = (parse("limits \"L\" {\n    level: global\n    pbm_transcranial {\n        max_irradiance_mw_cm2: 250\n    }\n}\n")
            .single() as NPProtocolEntry.Limits).limits
        assertEquals(250.0, lim.pbmTranscranial?.maxIrradianceMWcm2)
    }

    @Test
    fun roundTripsTheUnitBearingSpelling() {
        val src = proto(pbm("wavelength: \"660nm\"\nirradiance: 12.5mW_cm2"))
        val out = NPPSSerializer().serialize(parse(src).single())
        assertTrue(out.contains("irradiance: 12.5mW_cm2"), out)
        assertTrue(out.contains("wavelength: \"660nm\""), out)
        val again = (parse(out).single() as NPProtocolEntry.Single).protocol
        assertEquals(12.5, ((again.modalities.single().params) as NPModalityParams.PbmTranscranial).params.irradianceMWcm2)
    }

    @Test
    fun theValidatorChecksThe400PeakDirectly() {
        val params = NPPBMTranscranialParams(irradianceMWcm2 = 450.0)
        val def = NPProtocolDefinition(
            name = "hot",
            modalities = listOf(NPProtocolModality(params = NPModalityParams.PbmTranscranial(params))),
        )
        val result = NPProtocolValidator(NPLimitsSet(name = "none", level = NPLimitsSet.LimitLevel.GLOBAL)).validate(def)
        assertTrue(result.errors.any { it.parameterKey == "irradianceMWcm2" })
    }

    @Test
    fun theSessionBuilderCarriesAbsoluteValuesAndRefusesRetiredNames() {
        val def = NPProtocolDefinition(
            name = "abs",
            modalities = listOf(
                NPProtocolModality(params = NPModalityParams.PbmTranscranial(NPPBMTranscranialParams(irradianceMWcm2 = 250.0))),
                NPProtocolModality(params = NPModalityParams.PbmIntranasal(NPPBMIntranasalParams(irradianceMWcm2 = 40.0))),
                NPProtocolModality(params = NPModalityParams.AudioEntrainment(NPAudioEntrainmentParams(volumeDb = 70.0))),
            ),
        )
        // 250 mW/cm² at 808 nm: round(250 / 403 × 255) = 158 (0x9E) in the second current byte.
        // The wire carries a drive register, which the hub cannot read back as a percentage.
        val blob = compiler.compile(def.copy(modalities = def.modalities.take(1))).blob
        val params = blob.copyOfRange(64 + 14 + 16, 64 + 14 + 16 + 4)
        assertEquals(listOf<Byte>(20, 50, 0, 158.toByte()), params.toList())
        // Audio: 70 dB SPL → (70 − 40) / 0.5 = 60 % volume register.
        val audio = compiler.compile(def.copy(modalities = def.modalities.drop(2))).blob
        assertEquals(60.toByte(), audio[64 + 14 + 5])

        val bad = NPProtocolDefinition(
            name = "bad",
            modalities = listOf(NPProtocolModality(params = NPModalityParams.PbmTranscranial(
                NPPBMTranscranialParams(wavelength = NPPBMTranscranialParams.Wavelength("660_808nm"))))),
        )
        val e = assertFailsWith<IllegalArgumentException> { compiler.compile(bad) }
        assertTrue(e.message.orEmpty().contains("retired"))
        val nasal1064 = NPProtocolDefinition(
            name = "nasal",
            modalities = listOf(NPProtocolModality(params = NPModalityParams.PbmIntranasal(
                NPPBMIntranasalParams(wavelength = NPPBMTranscranialParams.Wavelength.NM_1064)))),
        )
        assertFailsWith<IllegalArgumentException> { compiler.compile(nasal1064) }
    }

    // OI-SESPWR-03: `frequency: 0` is CW and CW has no duty cycle.
    @Test
    fun refusesCwWithADutyOtherThan100() {
        val dose = mapOf(
            "pbm_transcranial" to "wavelength: \"808nm\"\nirradiance: 30mW_cm2",
            "pbm_intranasal" to "wavelength: \"660nm\"\nirradiance: 30mW_cm2",
            "pbm_deep_1170nm" to "intensity_mw_cm2: 500",
        )
        for ((modality, fields) in dose) {
            val msg = refused("    $modality {\n$fields\nfrequency: 0Hz\nduty_cycle: 25%\n    }")
            assertTrue(msg.contains("continuous wave, which has no duty cycle"), "$modality: $msg")
        }
    }

    @Test
    fun readsCwAs100PercentWhetherTheDutyIsWrittenOrNot() {
        for (extra in listOf("", "\nduty_cycle: 100%")) {
            val p = single(proto(pbm("wavelength: \"808nm\"\nirradiance: 30mW_cm2\nfrequency: 0Hz$extra")))
            val params = (p.modalities.single().params as NPModalityParams.PbmTranscranial).params
            assertEquals(100, params.dutyCyclePercent)
        }
    }

    @Test
    fun leavesAPulsedBlockAlone() {
        val p = single(proto(pbm("wavelength: \"808nm\"\nirradiance: 30mW_cm2\nfrequency: 40Hz\nduty_cycle: 25%")))
        val params = (p.modalities.single().params as NPModalityParams.PbmTranscranial).params
        assertEquals(40.0, params.frequencyHz)
        assertEquals(25, params.dutyCyclePercent)
    }
}

