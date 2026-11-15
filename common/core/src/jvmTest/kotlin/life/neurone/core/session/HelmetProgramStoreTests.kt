package life.neurone.core.session

import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.*
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/**
 * `SWR-HV-03` (`OI-WRV-01`): what this install programmed onto a helmet is kept in the sealed UHDR store, and the core says whether the rules in
 * force still map the stored protocol the way it was compiled. The verdicts themselves are the core's (`common/npps-core/tests/rules_drift.rs`);
 * this holds the app side: what is kept, what is not, and when the record goes.
 */
class HelmetProgramStoreTests {

    private val zeroSig = ProtocolSigner { SignatureResult(ByteArray(64), "00") }
    private fun defaultsWith(max808: Double): NPWavelengthRules {
        val d = NPWavelengthRulesEngine.DEFAULT
        return d.copy(channels = d.channels.map { if (it.element == NPPBMChannelElement.LED_808) it.copy(maxNm = max808) else it })
    }

    private fun pbm(wl: String) = NPProtocolModality(NPModalityParams.PbmTranscranial(NPPBMTranscranialParams(
        target = NPPBMTarget.Named(listOf("Frontal")), wavelength = NPPBMTranscranialParams.Wavelength(wl),
        irradianceMWcm2 = 100.0, frequencyHz = 40.0, dutyCyclePercent = 25)))

    private fun def(vararg wl: String) =
        NPProtocolDefinition(name = "Evening 850", timingMode = NPTimingMode.Duration(1200), modalities = wl.map(::pbm))

    /** Compiled the way a Mode 3 programming is, under [rules]. */
    private fun descriptor(d: NPProtocolDefinition, rules: NPWavelengthRules): ByteArray =
        HubDescriptorCompiler(zeroSig, wavelengthRules = { rules }, clockSeconds = { 1_700_000_000L }, randomBytes = { ByteArray(it) { 0xAB.toByte() } })
            .build(d, null, listOf(7, 3), autonomous = true).blob

    // @verifies SWR-HV-03
    @Test fun nothingProgrammedSaysNothing() {
        val s = HelmetProgramStore(InMemoryKeyValueStore())
        assertNull(s.record())
        assertNull(s.verdict(NPWavelengthRulesEngine.DEFAULT))
    }

    // @verifies SWR-HV-03
    @Test fun aProgrammingKeepsTheNameTheWavelengthsAndTheStampAndNothingElse() {
        val kv = InMemoryKeyValueStore()
        val s = HelmetProgramStore(kv)
        s.remember(def("850nm", "810nm", "850nm"), descriptor(def("850nm", "810nm"), defaultsWith(860.0)))
        val r = assertNotNull(s.record())
        assertEquals("Evening 850", r.protocolName)
        assertEquals(listOf("850nm", "810nm"), r.wavelengths, "as written, once each")
        assertEquals(78, r.stampHex.length, "the 39-byte stamp")
        // The stored text is exactly these three fields: no device id, serial, hash of the descriptor, protocol id or time.
        val stored = kv.getString(HelmetProgramStore.KEY)!!
        assertEquals(setOf("protocolName", "stampHex", "wavelengths"),
            kotlinx.serialization.json.Json.parseToJsonElement(stored).let { (it as kotlinx.serialization.json.JsonObject).keys })
        assertEquals("np.helmet.programmed", HelmetProgramStore.KEY)
    }

    // @verifies SWR-HV-03
    @Test fun theVerdictIsTheCoresAndNamesTheWavelengthsThatMove() {
        val s = HelmetProgramStore(InMemoryKeyValueStore())
        s.remember(def("850nm", "810nm"), descriptor(def("850nm", "810nm"), defaultsWith(860.0)))
        // Under the windows it was compiled with: nothing to say.
        assertEquals(HelmetProgramStore.Verdict.Current, s.verdict(defaultsWith(860.0)))
        // The shipped defaults refuse 850 nm (and still deliver 810 nm on the 808 channel).
        val v = s.verdict(NPWavelengthRulesEngine.DEFAULT)
        assertEquals(HelmetProgramStore.Verdict.Stale(listOf(HelmetProgramStore.Change("850nm", "led_808", null))), v)
        // Other windows that map this protocol the same way are said, not offered.
        val s2 = HelmetProgramStore(InMemoryKeyValueStore())
        s2.remember(def("810nm"), descriptor(def("810nm"), defaultsWith(860.0)))
        assertEquals(HelmetProgramStore.Verdict.Differs, s2.verdict(NPWavelengthRulesEngine.DEFAULT))
    }

    // @verifies SWR-HV-03
    @Test fun aLaterProgrammingReplacesTheRecordAndForgettingTheHubRemovesIt() {
        val kv = InMemoryKeyValueStore()
        val s = HelmetProgramStore(kv)
        s.remember(def("850nm"), descriptor(def("850nm"), defaultsWith(860.0)))
        s.remember(def("660nm"), descriptor(def("660nm"), NPWavelengthRulesEngine.DEFAULT))
        assertEquals(listOf("660nm"), s.record()!!.wavelengths)
        assertEquals(HelmetProgramStore.Verdict.Current, s.verdict(NPWavelengthRulesEngine.DEFAULT))
        s.forget()
        assertNull(s.record())
        assertNull(kv.getString(HelmetProgramStore.KEY))
    }

    // @verifies SWR-HV-03
    @Test fun aDescriptorWithNoReadableStampKeepsNoRecordAndAnUnreadableRecordIsNoRecord() {
        val kv = InMemoryKeyValueStore()
        val s = HelmetProgramStore(kv)
        s.remember(def("850nm"), descriptor(def("850nm"), defaultsWith(860.0)))
        assertNotNull(s.record())
        s.remember(def("850nm"), ByteArray(10))   // not a descriptor: the earlier record is dropped, not kept as if it were still true
        assertNull(s.record())
        kv.putString(HelmetProgramStore.KEY, "{not json")
        assertNull(s.record())
        assertNull(s.verdict(NPWavelengthRulesEngine.DEFAULT), "never a guess about what the helmet holds")
    }
}
