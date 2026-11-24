package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.core.protocol.ProfileTagState
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.NeurOneGattManager
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/**
 * The shared GATT manager names the per-helmet active-user tag (`REQ-FAULTMSG-01`, `OI-PRIV-29` (NP-PRIV-ANALYSIS-004)): only
 * once the helmet's serial has been read, the old tag until a valid report says nobody on the helmet is held, and the new
 * one only after the move is recorded. The link is a fake: no radio and no hub; the derivation is the shared Rust core's.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class ActiveUserTagWiringTest {
    private class FakeLink : BleCentral {
        override val adapterState = AdapterState.ON
        val written = mutableListOf<Pair<UUID, ByteArray>>()
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) {}
        override fun write(uuid: UUID, value: ByteArray) { written += uuid to value }
    }

    private val activeUserId = UUID.fromString(GattUuidStrings.ACTIVE_USER_ID)
    private val serialId = UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID)
    private val faultId = UUID.fromString(GattUuidStrings.CVNS_FAULT_STATUS_ID)
    private val profileId = "04030201-0000-4000-8000-000000000000"
    private val legacyTag = 0x0102_0304L
    private val helmetSerial = ByteArray(32) { 7 }

    private fun tagsNamed(link: FakeLink): List<Long> = link.written.filter { it.first == activeUserId }.map { (_, v) ->
        (0..3).fold(0L) { acc, i -> acc or ((v[i].toLong() and 0xFF) shl (8 * i)) }
    }

    private fun linked(link: FakeLink, scope: TestScope, user: NeurOneGattManager.ActiveUser, record: (String, Long) -> Boolean) =
        NeurOneGattManager(link, scope).also {
            it.recordTagMigration = record
            it.activeUser = user
            it.onDeviceFound("hub")
            it.onConnected("hub")
            it.onCharacteristicsDiscovered(setOf(activeUserId, serialId, faultId))
        }

    /** A fault-status frame with no records and the flags byte [flags] (version 2). */
    private fun frame(flags: Int) = byteArrayOf(0x02, 0x00, 0x00, flags.toByte())

    // @verifies REQ-FAULTMSG-01#c
    @Test fun a_legacy_profile_is_named_by_its_old_tag_until_a_valid_report_says_nobody_is_held_then_moves_for_good() = runTest {
        val link = FakeLink()
        val recorded = mutableListOf<Long>()
        val state = ProfileTagState(secret = "11".repeat(32), legacy = true)
        val m = linked(link, TestScope(testScheduler), NeurOneGattManager.ActiveUser(profileId, state)) { _, tag -> recorded += tag; true }
        assertEquals(emptyList(), tagsNamed(link), "nothing is named before the helmet's serial is read")

        m.onCharacteristicRead(serialId, helmetSerial)
        assertEquals(listOf(legacyTag), tagsNamed(link), "no status yet: the old tag")

        m.applyCervicalFaultStatus(frame(0x00))
        m.applyCervicalFaultStatus(frame(0x82))
        assertEquals(listOf(legacyTag), tagsNamed(link), "no valid report, then someone held: the old tag stays")
        assertEquals(emptyList(), recorded)

        m.applyCervicalFaultStatus(frame(0x80))
        val derived = m.activeUserTag!!
        assertEquals(listOf(derived), recorded, "the move is recorded before the new tag is named")
        assertEquals(listOf(legacyTag, derived), tagsNamed(link))

        m.applyCervicalFaultStatus(frame(0x83))
        assertEquals(listOf(legacyTag, derived), tagsNamed(link), "held again later: the old tag is never named again")
    }

    // @verifies REQ-FAULTMSG-01#c
    @Test fun a_move_that_cannot_be_recorded_keeps_the_old_tag() = runTest {
        val link = FakeLink()
        val state = ProfileTagState(secret = "11".repeat(32), legacy = true)
        val m = linked(link, TestScope(testScheduler), NeurOneGattManager.ActiveUser(profileId, state)) { _, _ -> false }
        m.onCharacteristicRead(serialId, helmetSerial)
        m.applyCervicalFaultStatus(frame(0x80))
        assertEquals(listOf(legacyTag), tagsNamed(link))
        assertEquals(legacyTag, m.activeUserTag)
    }

    // @verifies REQ-FAULTMSG-01#c
    @Test fun a_profile_made_since_the_change_is_named_by_its_per_helmet_tag_at_once_and_a_disconnect_forgets_it() = runTest {
        val link = FakeLink()
        val state = ProfileTagState(secret = "11".repeat(32), legacy = false)
        val m = linked(link, TestScope(testScheduler), NeurOneGattManager.ActiveUser(profileId, state)) { _, _ -> error("nothing to record") }
        m.onCharacteristicRead(serialId, helmetSerial)
        val named = tagsNamed(link).single()
        assertEquals(m.activeUserTag, named)
        assertEquals(false, named == legacyTag)
        m.onDisconnected("hub")
        assertNull(m.activeUserTag, "the next link may be another helmet")
    }
}
