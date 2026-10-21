package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.async
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.test.advanceTimeBy
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.runTest
import life.neurone.core.ble.CalibrationOpcode
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.models.ZoneModuleConfiguration
import life.neurone.core.models.ZoneModuleStatus
import life.neurone.core.models.ZoneModuleType
import life.neurone.core.setup.SetupFlow
import life.neurone.core.setup.SetupStep
import life.neurone.shared.ble.ConnectionState
import life.neurone.shared.setup.ImpedanceReading
import life.neurone.shared.setup.SetupCoordinator
import life.neurone.shared.setup.SetupError
import life.neurone.shared.setup.SetupLink
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** What the first-run hardware setup waits for from the hub before a step advances (REQ-SW03-05). */
@OptIn(ExperimentalCoroutinesApi::class)
class SetupCoordinatorTest {

    private class Link(connected: Boolean = true) : SetupLink {
        override val connectionState = MutableStateFlow(if (connected) ConnectionState.CONNECTED else ConnectionState.DISCONNECTED)
        override val impedanceReading = MutableStateFlow<ImpedanceReading?>(null)
        override val zoneModules = MutableStateFlow(ZoneModuleConfiguration.EMPTY)
        val sent = mutableListOf<CalibrationOpcode>()
        /** What the hub does in answer to a command. */
        var hub: (CalibrationOpcode, Link) -> Unit = { _, _ -> }

        override fun sendCalibration(opcode: CalibrationOpcode) {
            sent += opcode
            hub(opcode, this)
        }

        fun result(flags: Int) {
            impedanceReading.value = ImpedanceReading((impedanceReading.value?.sequence ?: 0L) + 1, flags)
        }
    }

    private fun coordinator(link: Link, at: SetupStep = SetupStep.WELCOME): SetupCoordinator {
        val flow = SetupFlow(InMemoryKeyValueStore())
        repeat(at.index) { if (flow.currentStep == SetupStep.SAFETY_ACKNOWLEDGEMENT) flow.acknowledgeSafety(); flow.advance() }
        return SetupCoordinator(flow, link)
    }

    private fun socket(id: Int, present: Boolean = true, fault: Boolean = false) =
        ZoneModuleStatus(id, if (present) ZoneModuleType.PBM_BASE else ZoneModuleType.ABSENT, isPresent = present && !fault, hasFault = fault)

    // @verifies REQ-SW03-05
    @Test fun an_impedance_reading_left_from_before_does_not_pass_the_check() = runTest {
        val link = Link()
        link.result(0xFF) // a perfect reading from an earlier check
        val c = coordinator(link, SetupStep.IMPEDANCE_CHECK)
        link.hub = { _, l -> l.result(0x03) } // the hub's answer to THIS check: two electrodes
        c.confirmImpedanceCheck()
        assertEquals(SetupStep.IMPEDANCE_CHECK, c.step.value)
        assertEquals(SetupError.ImpedanceFailed(listOf(2, 3, 4, 5, 6, 7)), c.error.value)
    }

    // @verifies REQ-SW03-05
    @Test fun a_check_the_hub_never_answers_times_out_and_does_not_advance_on_old_flags() = runTest {
        val link = Link()
        link.result(0xFF)
        val c = coordinator(link, SetupStep.IMPEDANCE_CHECK)
        c.confirmImpedanceCheck()
        assertEquals(SetupStep.IMPEDANCE_CHECK, c.step.value)
        assertEquals(SetupError.Timeout, c.error.value)
        assertFalse(c.isProcessing.value)
    }

    // @verifies REQ-SW03-05
    @Test fun six_of_eight_electrodes_pass_the_check_and_five_do_not() = runTest {
        val good = Link().apply { hub = { _, l -> l.result(0b0011_1111) } }
        val c = coordinator(good, SetupStep.IMPEDANCE_CHECK)
        c.confirmImpedanceCheck()
        assertEquals(SetupStep.ADS1299_CALIBRATION, c.step.value)
        assertNull(c.error.value)
        assertEquals(listOf(CalibrationOpcode.IMPEDANCE_CHECK), good.sent)

        val weak = Link().apply { hub = { _, l -> l.result(0b0001_1111) } }
        val w = coordinator(weak, SetupStep.IMPEDANCE_CHECK)
        w.confirmImpedanceCheck()
        assertEquals(SetupStep.IMPEDANCE_CHECK, w.step.value)
        assertEquals(SetupError.ImpedanceFailed(listOf(5, 6, 7)), w.error.value)
    }

    // @verifies REQ-SW03-05
    @Test fun a_result_of_no_electrodes_is_an_answer_and_a_failure_not_a_wait() = runTest {
        val link = Link()
        link.result(0x00) // the previous check also found nothing: the same value must still count as a new answer
        val c = coordinator(link, SetupStep.IMPEDANCE_CHECK)
        link.hub = { _, l -> l.result(0x00) }
        c.confirmImpedanceCheck()
        assertEquals(SetupError.ImpedanceFailed((0..7).toList()), c.error.value)
    }

    // @verifies REQ-SW03-05
    @Test fun the_contact_result_is_dropped_when_the_step_is_left() = runTest {
        val link = Link().apply { hub = { _, l -> l.result(0xFF) } }
        val c = coordinator(link, SetupStep.IMPEDANCE_CHECK)
        c.confirmImpedanceCheck()
        assertEquals(SetupStep.ADS1299_CALIBRATION, c.step.value)
        assertEquals(0, c.impedanceFlags.value)
    }

    // @verifies REQ-SW03-05
    @Test fun a_hub_that_is_not_connected_fails_every_hardware_step_without_sending() = runTest {
        val link = Link(connected = false)
        val z = coordinator(link, SetupStep.IMPEDANCE_CHECK)
        z.confirmImpedanceCheck()
        assertEquals(SetupError.NotConnected, z.error.value)
        val cal = coordinator(link, SetupStep.ADS1299_CALIBRATION)
        cal.confirmCalibration()
        assertEquals(SetupError.NotConnected, cal.error.value)
        val ble = coordinator(link, SetupStep.BLE_CONFIRMATION)
        ble.confirmBlePairing()
        assertEquals(SetupError.NotConnected, ble.error.value)
        assertEquals(SetupStep.BLE_CONFIRMATION, ble.step.value)
        assertTrue(link.sent.isEmpty())
    }

    // @verifies REQ-SW03-05
    @Test fun calibration_gives_the_amplifier_time_and_then_advances() = runTest {
        val link = Link()
        val c = coordinator(link, SetupStep.ADS1299_CALIBRATION)
        val job = async { c.confirmCalibration() }
        runCurrent()
        assertEquals(listOf(CalibrationOpcode.ADS1299_SELF_CAL), link.sent)
        assertTrue(c.isProcessing.value)
        advanceTimeBy(SetupCoordinator.CALIBRATION_WAIT_MS - 1)
        runCurrent()
        assertEquals(SetupStep.ADS1299_CALIBRATION, c.step.value)
        advanceTimeBy(2)
        job.await()
        assertEquals(SetupStep.HYDRATION_CAPS, c.step.value)
        assertFalse(c.isProcessing.value)
    }

    // @verifies REQ-SW03-05
    @Test fun a_hub_lost_during_calibration_does_not_advance() = runTest {
        val link = Link()
        val c = coordinator(link, SetupStep.ADS1299_CALIBRATION)
        link.hub = { _, l -> l.connectionState.value = ConnectionState.DISCONNECTED }
        c.confirmCalibration()
        assertEquals(SetupStep.ADS1299_CALIBRATION, c.step.value)
        assertEquals(SetupError.CalibrationFailed, c.error.value)
    }

    // @verifies REQ-SW03-05
    @Test fun zone_modules_pass_with_any_populated_socket_and_fail_on_a_faulted_one_or_none() = runTest {
        val link = Link()
        val c = coordinator(link, SetupStep.ZONE_MODULES)
        c.confirmZoneModules()
        assertEquals(SetupError.NoZoneModules, c.error.value)
        link.zoneModules.value = ZoneModuleConfiguration(mapOf(3 to socket(3), 9 to socket(9, fault = true)))
        c.confirmZoneModules()
        assertEquals(SetupError.ZoneModulesMissing(listOf(9)), c.error.value)
        assertEquals(SetupStep.ZONE_MODULES, c.step.value)
        link.zoneModules.value = ZoneModuleConfiguration(mapOf(3 to socket(3)))
        c.confirmZoneModules()
        assertEquals(SetupStep.IMPEDANCE_CHECK, c.step.value)
        assertNull(c.error.value)
    }

    // @verifies REQ-SW03-05
    @Test fun the_safety_step_does_not_advance_until_it_is_acknowledged_and_going_back_clears_it() = runTest {
        val c = coordinator(Link(), SetupStep.SAFETY_ACKNOWLEDGEMENT)
        c.advance()
        assertEquals(SetupStep.SAFETY_ACKNOWLEDGEMENT, c.step.value)
        assertEquals(SetupError.SafetyAcknowledgementRequired, c.error.value)
        c.acknowledgeSafety()
        assertTrue(c.safetyAcknowledged.value)
        c.back()
        assertFalse(c.safetyAcknowledged.value)
        c.advance()
        assertEquals(SetupStep.SAFETY_ACKNOWLEDGEMENT, c.step.value)
    }

    // @verifies REQ-SW03-05
    @Test fun finishing_the_last_step_marks_setup_complete_once() = runTest {
        val store = InMemoryKeyValueStore()
        val flow = SetupFlow(store)
        val c = SetupCoordinator(flow, Link())
        assertFalse(c.isFirstSetupComplete)
        repeat(SetupStep.COMPLETE.index) {
            if (c.step.value == SetupStep.SAFETY_ACKNOWLEDGEMENT) c.acknowledgeSafety()
            c.advance()
        }
        assertEquals(SetupStep.COMPLETE, c.step.value)
        assertTrue(c.isFirstSetupComplete)
    }
}
