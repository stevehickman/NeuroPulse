package com.neuropulse.core.setup

import com.neuropulse.core.common.InMemoryKeyValueStore
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
}
