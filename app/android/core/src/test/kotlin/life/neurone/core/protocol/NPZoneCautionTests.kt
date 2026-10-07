package life.neurone.core.protocol

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonArray
import life.neurone.core.session.HubDescriptorCompiler
import life.neurone.core.session.ProtocolSigner
import life.neurone.core.session.SignatureResult
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * The caution list behind the acknowledgement screen (docs/reference/safety-zones.md): one entry per axis in its
 * caution band, carrying the id the compiler checks, and the ids it lists are what lets the protocol compile.
 */
class NPZoneCautionTests {

    private val unlimited = NPProtocolValidator(NPLimitsSet(name = "unlimited"))
    private val compiler = HubDescriptorCompiler(ProtocolSigner { SignatureResult(ByteArray(64), "00") })

    // 1.5 mA over 35 cm² for 2400 s is 102.857 mC/cm²: the caution band of the 150 mC/cm² session ceiling.
    private fun tdcs(milliamps: Double) = NPProtocolDefinition(
        name = "g",
        timingMode = NPTimingMode.Duration(2400),
        modalities = listOf(
            NPProtocolModality(
                params = NPModalityParams.Tdcs(NPTDCSParams(intensityMilliamps = milliamps, electrodeAreaCm2 = 35.0)),
                interval = NPIntervalConfig.CONTINUOUS,
            ),
        ),
    )

    @Test fun parseListsOnlyAnAxisInItsCautionBand() {
        val zones = Json.parseToJsonElement(
            """[{"index":1,"modality":"tdcs","zone":"caution","axes":[
                 {"id":"s","nameKey":"ZONE_AXIS_SESSION_CHARGE_DENSITY","unit":"mC/cm²","value":102.857,"caution":100.0,
                  "danger":150.0,"zone":"caution","ackId":"1:tdcs:sessionChargeDensity=102.857"},
                 {"id":"o","nameKey":"ZONE_AXIS_SHANNON_K","unit":"","value":0.2,"caution":1.0,"danger":null,
                  "zone":"safe","ackId":null}]}]""",
        ).jsonArray
        val cautions = NPZoneCaution.parse(zones)
        assertEquals(1, cautions.size)
        assertEquals("1:tdcs:sessionChargeDensity=102.857", cautions[0].ackId)
        assertEquals(NPModalityType.TDCS, cautions[0].modality)
        assertEquals("ZONE_AXIS_SESSION_CHARGE_DENSITY", cautions[0].axisNameKey)
        assertEquals(100.0, cautions[0].caution)
        assertTrue(NPZoneCaution.parse(null).isEmpty())
    }

    @Test fun aSafeProtocolHasNoCautions() {
        assertTrue(unlimited.validate(tdcs(0.5)).zoneCautions.isEmpty())
    }

    @Test fun theIdsItListsAreWhatLetTheProtocolCompile() {
        val def = tdcs(1.5)
        val cautions = unlimited.validate(def).zoneCautions
        assertEquals(listOf("0:tdcs:sessionChargeDensity=102.857"), cautions.map { it.ackId })
        assertFailsWith<IllegalArgumentException> { compiler.compile(def) }
        compiler.compile(def, acknowledgedCautions = cautions.map { it.ackId })
    }
}
