package life.neurone.core.setup

import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.models.ZoneModuleConfiguration
import life.neurone.core.models.ZoneModuleStatus
import life.neurone.core.models.ZoneModuleType
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** Port of the state-machine assertions from iOS HardwareSetupManagerTests. */
class SetupFlowTests {

    private fun flow() = SetupFlow(InMemoryKeyValueStore())

    @Test
    fun startsAtWelcome() {
        assertEquals(SetupStep.WELCOME, flow().currentStep)
    }

    @Test
    fun advanceProgressesThroughStepsInOrder() {
        val f = flow()
        val order = mutableListOf(f.currentStep)
        // Acknowledge safety when we reach it so the wizard can proceed.
        repeat(SetupStep.entries.size) {
            if (f.currentStep == SetupStep.SAFETY_ACKNOWLEDGEMENT) f.acknowledgeSafety()
            if (f.advance() is SetupFlow.AdvanceResult.Advanced) order.add(f.currentStep)
        }
        assertEquals(SetupStep.entries.toList(), order)
    }

    @Test
    fun safetyStepBlocksAdvanceUntilAcknowledged() {
        val g = flow()
        while (g.currentStep != SetupStep.SAFETY_ACKNOWLEDGEMENT) g.advance()
        assertEquals(SetupFlow.AdvanceResult.BlockedBySafety, g.advance())
        assertEquals(SetupStep.SAFETY_ACKNOWLEDGEMENT, g.currentStep)
        g.acknowledgeSafety()
        assertEquals(SetupFlow.AdvanceResult.Advanced, g.advance())
        assertEquals(SetupStep.PROTOCOL_SELECTION, g.currentStep)
    }

    @Test
    fun reachingCompletePersistsFirstSetup() {
        val store = InMemoryKeyValueStore()
        val f = SetupFlow(store)
        while (f.currentStep != SetupStep.COMPLETE) {
            if (f.currentStep == SetupStep.SAFETY_ACKNOWLEDGEMENT) f.acknowledgeSafety()
            f.advance()
        }
        assertTrue(store.getBoolean(SetupFlow.FIRST_SETUP_KEY))
        assertTrue(f.isFirstSetupComplete)
    }

    @Test
    fun backReEntersSafetyAndClearsAcknowledgement() {
        val f = flow()
        while (f.currentStep != SetupStep.SAFETY_ACKNOWLEDGEMENT) f.advance()
        f.acknowledgeSafety()
        f.advance() // → PROTOCOL_SELECTION
        f.back()    // → SAFETY_ACKNOWLEDGEMENT
        assertEquals(SetupStep.SAFETY_ACKNOWLEDGEMENT, f.currentStep)
        assertFalse(f.safetyAcknowledged, "re-entering the safety step must clear the acknowledgement")
    }

    @Test
    fun impedanceEvaluationMeetsThresholdAtSixOfEight() {
        val f = flow()
        // 6 of 8 electrodes pass (bits 0–5 set) → passes.
        val sixPass = f.evaluateImpedance(0b00111111)
        assertTrue(sixPass.passed)
        assertEquals(6, sixPass.passCount)
        assertEquals(listOf(6, 7), sixPass.failedElectrodes)

        // 5 of 8 pass → fails, with the three unset electrodes reported.
        val fivePass = f.evaluateImpedance(0b00011111)
        assertFalse(fivePass.passed)
        assertEquals(5, fivePass.passCount)
        assertEquals(listOf(5, 6, 7), fivePass.failedElectrodes)
    }

    @Test
    fun hardwareConfirmationFlagsAreCorrect() {
        assertTrue(SetupStep.BLE_CONFIRMATION.requiresHardwareConfirmation)
        assertTrue(SetupStep.IMPEDANCE_CHECK.requiresHardwareConfirmation)
        assertTrue(SetupStep.ADS1299_CALIBRATION.requiresHardwareConfirmation)
        assertTrue(SetupStep.ZONE_MODULES.requiresHardwareConfirmation)
        assertFalse(SetupStep.BOA_DIAL.requiresHardwareConfirmation)
        assertTrue(SetupStep.SAFETY_ACKNOWLEDGEMENT.requiresSafetyAcknowledgement)
    }

    private fun socket(id: Int, present: Boolean, fault: Boolean = false) = ZoneModuleStatus(
        socketId = id,
        moduleType = if (present) ZoneModuleType.PBM_BASE else if (fault) ZoneModuleType.UNKNOWN else ZoneModuleType.ABSENT,
        isPresent = present,
        hasFault = fault,
    )

    private fun config(vararg s: ZoneModuleStatus) = ZoneModuleConfiguration(s.associateBy { it.socketId })

    @Test
    fun zoneModulesPassWithAnyPopulatedSocketAndNoFault() {
        // Not "all five slots": one module among many reported sockets is a valid build.
        val r = flow().evaluateZoneModules(config(socket(12, true), socket(40, false), socket(77, false)))
        assertEquals(SetupFlow.ZoneModuleResult.Passed, r)
    }

    @Test
    fun zoneModulesFaultedSocketBlocksEvenWhenOthersPresent() {
        val r = flow().evaluateZoneModules(
            config(socket(3, true), socket(61, false, fault = true), socket(9, false, fault = true)),
        )
        assertEquals(SetupFlow.ZoneModuleResult.Faulted(listOf(9, 61)), r)
    }

    @Test
    fun zoneModulesNoneDetectedWhenEmptyOrAllEmptySockets() {
        assertEquals(SetupFlow.ZoneModuleResult.NoneDetected, flow().evaluateZoneModules(ZoneModuleConfiguration.EMPTY))
        assertEquals(
            SetupFlow.ZoneModuleResult.NoneDetected,
            flow().evaluateZoneModules(config(socket(1, false), socket(2, false))),
        )
    }
}
