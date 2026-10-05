package life.neurone.core.session

import life.neurone.core.protocol.*
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * OI-AND-WIRE-01: the Android compiler writes the descriptor of NP-FW-HUB-001 §4, byte for byte
 * what app/web/src/lib/hubCompiler.ts writes. hub-descriptor-golden.json is the web compiler's
 * output (bun scripts/gen-hub-descriptor-golden.ts), with the session UUID pinned to 0xAB and the
 * compile time to 1700000000.
 */
class HubDescriptorCompilerTests {

    private val zeroSig = ProtocolSigner { SignatureResult(ByteArray(64), "00") }
    private val compiler = HubDescriptorCompiler(zeroSig, clockSeconds = { 1_700_000_000L }, randomBytes = { ByteArray(it) { 0xAB.toByte() } })

    private val golden: Map<String, String> by lazy {
        val text = javaClass.getResourceAsStream("/hub-descriptor-golden.json")!!.bufferedReader().readText()
        Json.parseToJsonElement(text).jsonObject.mapValues { it.value.jsonPrimitive.content }
    }

    private fun hex(b: ByteArray) = b.joinToString("") { "%02x".format(it) }

    private fun def(secs: Int, vararg m: NPProtocolModality) =
        NPProtocolDefinition(name = "g", timingMode = NPTimingMode.Duration(secs), modalities = m.toList())

    private fun mod(p: NPModalityParams, i: NPIntervalConfig = NPIntervalConfig.CONTINUOUS) =
        NPProtocolModality(params = p, interval = i)

    private fun pbm(wl: String, irr: Double) = mod(NPModalityParams.PbmTranscranial(NPPBMTranscranialParams(
        target = NPPBMTarget.Named(listOf("Frontal")), wavelength = NPPBMTranscranialParams.Wavelength(wl),
        irradianceMWcm2 = irr, frequencyHz = 40.0, dutyCyclePercent = 25)))

    private fun check(name: String, d: NPProtocolDefinition) =
        assertEquals(golden.getValue(name), hex(compiler.build(d, null, listOf(7, 3)).blob), name)

    @Test fun pbmChannels() {
        check("pbm660", def(1200, pbm("660nm", 200.0)))
        check("pbm808", def(1200, pbm("808nm", 322.0)))
        check("pbm1064", def(1200, pbm("1064nm", 28.0)))
    }

    @Test fun parallelWavelengthsMergeIntoOneTileCommand() = check("pbmMerged", def(600, pbm("660nm", 100.0), pbm("808nm", 200.0)))

    @Test fun clinicianSelectedSockets() = check("pbmClin", def(600, mod(NPModalityParams.PbmTranscranial(NPPBMTranscranialParams(
        target = NPPBMTarget.ClinicianSelected, wavelength = NPPBMTranscranialParams.Wavelength.NM_808,
        irradianceMWcm2 = 100.0, frequencyHz = 0.0, dutyCyclePercent = 100)))))

    @Test fun t1Modalities() {
        check("nasal", def(600, mod(NPModalityParams.PbmIntranasal(NPPBMIntranasalParams(
            wavelength = NPPBMTranscranialParams.Wavelength.NM_808, irradianceMWcm2 = 60.0, frequencyHz = 40.0, dutyCyclePercent = 25)))))
        check("eegAll", def(600, mod(NPModalityParams.EegNeurofeedback(NPEEGNeurofeedbackParams()))))
        check("eegCustom", def(600, mod(NPModalityParams.EegNeurofeedback(NPEEGNeurofeedbackParams(
            channels = NPEEGNeurofeedbackParams.ChannelSelection.CUSTOM, customChannels = listOf("Fp1", "P4"), closedLoopEnabled = false)))))
        check("bes", def(600, mod(NPModalityParams.BesTacs(NPBESTacsParams(frequencyHz = 10.0, intensityMilliamps = 0.8)))))
        check("tdcs", def(600, mod(NPModalityParams.Tdcs(NPTDCSParams(
            intensityMilliamps = 1.0, electrodePairs = listOf(listOf("P3", "P4")), electrodeAreaCm2 = 25.5)))))
        check("vns", def(600, mod(NPModalityParams.VnsHRV(NPVNSHRVParams(hrvProtocol = NPVNSHRVParams.HRVProtocol.TAVNS_SYNC)))))
        check("audioBin", def(600, mod(NPModalityParams.AudioEntrainment(NPAudioEntrainmentParams(
            binauralBeatsHz = 20.0, isochronicTonesHz = null, noiseType = null, volumeDb = 75.0, boneConductionPacer = false)))))
        check("audioPink", def(600, mod(NPModalityParams.AudioEntrainment(NPAudioEntrainmentParams(
            binauralBeatsHz = null, isochronicTonesHz = null, noiseType = NPAudioEntrainmentParams.NoiseType.PINK,
            volumeDb = 60.0, eegAdaptive = false)))))
        check("visual", def(600, mod(NPModalityParams.VisualStimulation(NPVisualStimParams(emdrCadenceHz = 1.5)))))
    }

    @Test fun t2Modalities() {
        check("qeeg", def(600, mod(NPModalityParams.Qeeg21ch(NPqEEG21chParams(reference = NPqEEG21chParams.Reference.AVERAGE)))))
        check("tms", def(600, mod(NPModalityParams.Tms(NPTMSParams(
            tmsProtocol = NPTMSParams.TMSProtocol.ITBS, target = NPTMSParams.TMSTarget.M1_R, pulseCount = 600)))))
        check("deep", def(600, mod(NPModalityParams.PbmDeep1170nm(NPDeepPBM1170Params()))))
        check("ctacs", def(600, mod(NPModalityParams.ClinicalTacs(NPClinicalTacsParams(
            channelCount = 21, waveform = NPBESTacsParams.Waveform.SQUARE)))))
        check("hdtdcs", def(600, mod(NPModalityParams.HdTdcs(NPHDTdcsParams(
            target = NPTMSParams.TMSTarget.DLPFC_R, montage = NPHDTdcsParams.Montage.BILATERAL_4X1)))))
        check("cvns", def(600, mod(NPModalityParams.CervicalVns(NPCervicalVnsParams()))))
        check("vibro", def(600, mod(NPModalityParams.Vibrotactile40hz(NPVibrotactileParams(syncToVisual = false)))))
        assertTrue(compiler.build(def(600, mod(NPModalityParams.CervicalVns(NPCervicalVnsParams()))), null, null).isT2)
    }

    @Test fun intervalsExpandToOnAndStopAndBlockStartShiftsTheSchedule() = check("interval", def(100,
        mod(NPModalityParams.BesTacs(NPBESTacsParams(frequencyHz = 10.0, intensityMilliamps = 0.8, waveform = NPBESTacsParams.Waveform.SQUARE)),
            NPIntervalConfig(20, 10, null)),
        mod(NPModalityParams.Tdcs(NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = listOf(listOf("F3", "F4")),
            rampSeconds = 45, electrodeAreaCm2 = 10.0)), NPIntervalConfig(0, 0, null, startOffsetSeconds = 30))))

    @Test fun signatureCoversTheRawRegionAndLandsInTheLast64Bytes() {
        var seen: ByteArray? = null
        val c = HubDescriptorCompiler({ m -> seen = m; SignatureResult(ByteArray(64) { 7 }, "fp") }, { 1L }, { ByteArray(it) })
        val d = c.compile(def(600, pbm("808nm", 100.0)))
        assertEquals(d.blob.size - 64, seen!!.size)
        assertContentEquals(seen, d.blob.copyOfRange(0, d.blob.size - 64))
        assertTrue(d.blob.takeLast(64).all { it == 7.toByte() })
        assertEquals("fp", d.publicKeyFingerprint)
    }

    @Test fun deviceSerialIsTheReplayGuard() {
        val serial = ByteArray(32) { (it + 1).toByte() }
        val blob = compiler.build(def(600, pbm("808nm", 100.0)), serial, null).blob
        assertContentEquals(serial, blob.copyOfRange(28, 60))
    }

    @Test fun refusalsNameTheProblemAndNeverReshape() {
        assertFailsWith<IllegalArgumentException> { compiler.build(def(600, pbm("808nm", 500.0)), null, null) } // above 403 full scale
        assertFailsWith<IllegalArgumentException> { compiler.build(def(600, pbm("1064nm", 100.0)), null, null) } // above 28
        assertFailsWith<IllegalArgumentException> { compiler.build(def(600, pbm("660_808nm", 100.0)), null, null) }
        assertFailsWith<IllegalArgumentException> { compiler.build(def(600), null, null) } // nothing enabled
        assertFailsWith<IllegalArgumentException> { // a block that starts after the session ends
            compiler.build(def(60, mod(NPModalityParams.BesTacs(NPBESTacsParams()), NPIntervalConfig(0, 0, null, startOffsetSeconds = 60))), null, null)
        }
        assertFailsWith<NPPSError> { compiler.build(def(600, mod(NPModalityParams.PbmTranscranial(NPPBMTranscranialParams(
            target = NPPBMTarget.Named(listOf("No Such Zone")))))), null, null) }
        // Two PBM blocks on the same tile that differ in duty are not one stimulus.
        val other = mod(NPModalityParams.PbmTranscranial(NPPBMTranscranialParams(
            target = NPPBMTarget.Named(listOf("Frontal")), wavelength = NPPBMTranscranialParams.Wavelength.NM_660,
            irradianceMWcm2 = 100.0, frequencyHz = 40.0, dutyCyclePercent = 10)))
        assertFailsWith<IllegalArgumentException> { compiler.build(def(600, pbm("808nm", 100.0), other), null, null) }
        // More than 64 commands is refused, not truncated: the tail of an interval protocol is its STOPs.
        assertFailsWith<IllegalArgumentException> {
            compiler.build(def(3600, mod(NPModalityParams.BesTacs(NPBESTacsParams()), NPIntervalConfig(10, 10, null))), null, null)
        }
    }

    @Test fun descriptorChunksAndReassembles() {
        val blob = compiler.compile(def(600, pbm("808nm", 100.0))).blob
        val chunks = ProtocolChunker.chunk(blob)
        assertTrue(chunks.all { it.size <= ProtocolChunker.MAX_WRITE_SIZE })
        val reassembled = chunks.flatMap { c ->
            if (c[0] == ProtocolChunker.FRAME_START) c.drop(3) else c.drop(1)
        }.toByteArray()
        assertContentEquals(blob, reassembled)
    }
}
