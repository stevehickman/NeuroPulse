package com.neurone.core.setup

import com.neurone.core.common.KeyValueStore

// Port of the state-machine core of iOS HardwareSetupManager (the pure, testable part —
// GATT-async confirmation stays in the :app ViewModel). Guides first-session hardware setup:
// BLE pairing → fit → pods → zone modules → impedance → ADS1299 cal → hydration caps →
// safety acknowledgement → first protocol → complete. Hardware-gated steps require hub
// confirmation before the caller advances; the safety step cannot be bypassed.

enum class SetupStep(val index: Int) {
    WELCOME(0),
    BLE_CONFIRMATION(1),        // Hub BLE pairing confirmation
    BOA_DIAL(2),                // Fit — 52–62 cm head range
    ELECTRODE_PODS(3),          // Spring-decoupled pods, 80–120 g contact force
    ZONE_MODULES(4),            // Zone module insertion + bone-conduction confirmation
    IMPEDANCE_CHECK(5),         // ADS1299 electrode impedance
    ADS1299_CALIBRATION(6),     // ADS1299 internal reference self-calibration
    HYDRATION_CAPS(7),          // Remove moisture-barrier hydration caps before use
    SAFETY_ACKNOWLEDGEMENT(8),  // T1 contraindications — not pre-ticked, cannot be bypassed
    PROTOCOL_SELECTION(9),      // First protocol selection
    COMPLETE(10);

    val requiresHardwareConfirmation: Boolean
        get() = this == BLE_CONFIRMATION || this == ZONE_MODULES ||
            this == IMPEDANCE_CHECK || this == ADS1299_CALIBRATION

    val requiresSafetyAcknowledgement: Boolean get() = this == SAFETY_ACKNOWLEDGEMENT

    companion object {
        fun from(index: Int): SetupStep? = entries.firstOrNull { it.index == index }
    }
}

class SetupFlow(private val store: KeyValueStore) {

    companion object {
        const val FIRST_SETUP_KEY = "np.setup.first-complete"
        /** Electrodes (of 8) that must pass impedance — a safety threshold, not configurable. */
        const val MINIMUM_IMPEDANCE_PASS_COUNT = 6
        const val EXPECTED_ELECTRODE_COUNT = 8
    }

    var currentStep: SetupStep = SetupStep.WELCOME
        private set

    // PRIVACY: never persisted — an acknowledgement of contraindications is inferred health
    // status (GDPR Art. 9 / BIPA). Lives only in memory for the duration of the wizard.
    var safetyAcknowledged: Boolean = false
        private set

    val isFirstSetupComplete: Boolean get() = store.getBoolean(FIRST_SETUP_KEY)

    sealed interface AdvanceResult {
        data object Advanced : AdvanceResult
        data object BlockedBySafety : AdvanceResult
        data object AtEnd : AdvanceResult
    }

    fun advance(): AdvanceResult {
        if (currentStep == SetupStep.SAFETY_ACKNOWLEDGEMENT && !safetyAcknowledged) {
            return AdvanceResult.BlockedBySafety
        }
        val next = SetupStep.from(currentStep.index + 1) ?: return AdvanceResult.AtEnd
        currentStep = next
        if (currentStep == SetupStep.COMPLETE) store.putBoolean(FIRST_SETUP_KEY, true)
        return AdvanceResult.Advanced
    }

    fun back() {
        val prev = SetupStep.from(currentStep.index - 1) ?: return
        currentStep = prev
        // Re-entering the safety step clears the acknowledgement so it must be given again.
        if (prev == SetupStep.SAFETY_ACKNOWLEDGEMENT) safetyAcknowledged = false
    }

    fun acknowledgeSafety() {
        safetyAcknowledged = true
    }

    data class ImpedanceResult(
        val passed: Boolean,
        val passCount: Int,
        val failedElectrodes: List<Int>,
    )

    /** Evaluate an 8-electrode impedance pass-flags bitmask against the safety threshold. */
    fun evaluateImpedance(flags: Int): ImpedanceResult {
        val passCount = (0 until EXPECTED_ELECTRODE_COUNT).count { (flags and (1 shl it)) != 0 }
        val failed = (0 until EXPECTED_ELECTRODE_COUNT).filter { (flags and (1 shl it)) == 0 }
        return ImpedanceResult(
            passed = passCount >= MINIMUM_IMPEDANCE_PASS_COUNT,
            passCount = passCount,
            failedElectrodes = failed,
        )
    }
}
