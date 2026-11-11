package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.protocol.NPIntervalConfig
import life.neurone.core.protocol.NPModalityParams
import life.neurone.core.protocol.NPPBMTarget
import life.neurone.core.protocol.NPPBMTranscranialParams
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPProtocolModality
import life.neurone.core.protocol.NPTimingMode
import life.neurone.core.session.HubDescriptorCompiler
import life.neurone.core.session.ProtocolSigner
import life.neurone.core.session.SignatureResult
import life.neurone.shared.ble.ConnectionState
import life.neurone.shared.ble.HubBridge
import life.neurone.shared.ble.HubBridgeCommand
import life.neurone.shared.ble.HubBridgeEvent
import life.neurone.shared.ble.NeurOneGattManager
import life.neurone.shared.ble.SimulatedBleCentral
import life.neurone.shared.session.ProtocolUploader
import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

/**
 * The app's own `ProtocolUploader`, with the real `ProtocolChunker`, writes through `SimulatedBleCentral` (OI-SIMFW-64,
 * `REQ-SIMFW-19` (c), `REQ-FWHUB-80` (d)). What it writes to the upload characteristic is pinned to
 * `common/testdata/upload-stream-golden.json`, the file the simulator's end-to-end test replays against the real firmware
 * (`fw-bridge.test.js`: the hub takes those writes and the twin accepts the TEST-key-signed protocol). Neither test reaches the other's
 * process: the JVM cannot start the firmware here, so the two halves meet at the fixture.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class UploadThroughCentralTest {

    private class FakeBridge : HubBridge {
        var onEvent: (HubBridgeEvent) -> Unit = {}
        val sent = mutableListOf<HubBridgeCommand>()
        override fun setReceiver(receiver: (HubBridgeEvent) -> Unit) { onEvent = receiver }
        override fun send(command: HubBridgeCommand): Boolean { sent += command; return true }
        fun report(vararg events: HubBridgeEvent) = events.forEach(onEvent)
    }

    // The characteristics the twin publishes that the manager looks for, with the protocol upload (0x08) among them.
    private val published = listOf(0x07, 0x08, 0x10, 0x17, 0x14, 0x15, 0x16, 0x18, 0x0f, 0x1a, 0x1c, 0x1d, 0x1b, 0x1e, 0x20, 0x21)

    private fun pbm(wl: String, irr: Double) = NPProtocolModality(
        params = NPModalityParams.PbmTranscranial(
            NPPBMTranscranialParams(
                target = NPPBMTarget.Named(listOf("Frontal")), wavelength = NPPBMTranscranialParams.Wavelength(wl),
                irradianceMWcm2 = irr, frequencyHz = 40.0, dutyCyclePercent = 25,
            ),
        ),
        interval = NPIntervalConfig.CONTINUOUS,
    )

    /** The fixture's writes for a case, at the write size `size` (the hub's 512-byte ceiling is the case's `chunks`). */
    private fun fixtureChunks(name: String, size: Int = 512): List<String> {
        val file = File("../../../common/testdata/upload-stream-golden.json")
        assertTrue(file.exists(), "the shared fixture is at ${file.absolutePath}")
        val cases = Json.parseToJsonElement(file.readText()).jsonObject["cases"]!!.jsonArray
        val case = cases.first { it.jsonObject["name"]!!.jsonPrimitive.content == name }.jsonObject
        return (if (size == 512) case["chunks"]!! else case["chunks_by_write_size"]!!.jsonObject[size.toString()]!!).jsonArray.map { it.jsonPrimitive.content }
    }

    /** The app's uploader, with the real chunker, on a link that negotiated `attMtu`: the writes it makes to the upload characteristic. */
    private fun TestScope.writesAt(attMtu: Int?): List<HubBridgeCommand.Write> {
        val bridge = FakeBridge()
        val central = if (attMtu == null) SimulatedBleCentral(bridge) else SimulatedBleCentral(bridge, attMtu)
        val manager = NeurOneGattManager(central, this)
        bridge.report(HubBridgeEvent.Opened)
        published.forEachIndexed { i, id -> bridge.report(HubBridgeEvent.Characteristic(index = i, count = published.size, id = id, props = 1)) }
        assertEquals(ConnectionState.CONNECTED, manager.connectionState.value)

        // The compiler of the core's golden test: a fixed clock and session id and a zeroed signature slot, as the hub
        // golden is made (`hub-descriptor-golden.json`); the twin's signing service fills the slot (REQ-SIMFW-17).
        val zeroSigner = ProtocolSigner { SignatureResult(ByteArray(64), "00") }
        val compiler = HubDescriptorCompiler(zeroSigner, clockSeconds = { 1_700_000_000L }, randomBytes = { n -> ByteArray(n) { 0xAB.toByte() } })
        val uploader = ProtocolUploader(manager, compiler)
        val definition = NPProtocolDefinition(name = "g", timingMode = NPTimingMode.Duration(1200), modalities = listOf(pbm("660nm", 200.0)))
        assertIs<ProtocolUploader.Result.Success>(uploader.upload(definition, deviceSerial = ByteArray(32), clinicianSockets = listOf(7, 3)))
        return bridge.sent.filterIsInstance<HubBridgeCommand.Write>()
    }

    // @verifies REQ-SIMFW-19#c
    // @verifies REQ-FWHUB-80#d
    @Test fun the_uploader_writes_the_fixtures_exact_bytes_to_the_upload_characteristic_through_the_simulated_central() = runTest {
        val writes = writesAt(null)   // the simulated central's default MTU of 247: a write of 244
        assertTrue(writes.isNotEmpty() && writes.all { it.id == 0x08 }, "every write is to the protocol upload characteristic")
        assertEquals(fixtureChunks("golden_pbm660", 244), writes.map { it.hex },
            "the uploader's writes through the central are exactly the fixture's, the bytes the twin's test replays")
    }

    // @verifies REQ-FWHUB-80#d
    @Test fun the_uploader_cuts_at_the_links_mtu_less_three_and_the_default_mtu_of_23_gives_twenty_byte_writes() = runTest {
        // ATT_MTU 23 (no MTU negotiated), 188 (an iOS link), 247 (the data length extension), 515 (the hub's 512 ceiling).
        for ((mtu, size) in listOf(23 to 20, 188 to 185, 247 to 244, 515 to 512)) {
            val writes = writesAt(mtu)
            assertTrue(writes.all { it.id == 0x08 && it.hex.length / 2 <= size }, "no write over $size at an MTU of $mtu")
            assertEquals(fixtureChunks("golden_pbm660", size), writes.map { it.hex }, "the fixture's writes at an MTU of $mtu")
        }
        // An MTU above the ceiling's is cut at the hub's 512, not at the MTU less 3.
        assertTrue(writesAt(517).all { it.hex.length / 2 <= 512 })
    }
}
