package life.neurone.shared.setup

import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.withTimeoutOrNull
import life.neurone.core.ble.CalibrationOpcode
import life.neurone.core.models.ZoneModuleConfiguration
import life.neurone.core.setup.SetupFlow
import life.neurone.core.setup.SetupStep
import life.neurone.shared.ble.ConnectionState

// Port of the hardware-gated half of app/ios/NeurOne/Setup/HardwareSetupManager.swift (SWR REQ-SW03-05). `SetupFlow` (in :core) is
// the step order and the thresholds; this is what waits on the hub. A hardware step advances only on the hub's own answer to the
// command just sent:
//   • the impedance step sends the check and waits for the NEXT IMPEDANCE_RESULT notification (a result is told apart by a
//     sequence number, because an all-fail result 0x00 repeats the previous value and a flow of values would swallow it). It never
//     reads flags left over from an earlier check, which is the defect this replaces: the screen sent the command and judged the
//     flags already in memory, so a good earlier reading could carry a poor contact through the gate;
//   • the calibration step sends the self-calibration, waits for it to run, and advances only if the hub is still there.
// The safety acknowledgement is required before its step advances, is never persisted and resets on going back (SetupFlow).

/** One IMPEDANCE_RESULT notification: [sequence] counts them, [flags] is the per-electrode pass bitmask (bit n = electrode n passed). */
data class ImpedanceReading(val sequence: Long, val flags: Int)

/** The part of the hub link the wizard needs: [NeurOneGattManager] is the production one, a fake is the test's. */
interface SetupLink {
    val connectionState: StateFlow<ConnectionState>
    val impedanceReading: StateFlow<ImpedanceReading?>
    val zoneModules: StateFlow<ZoneModuleConfiguration>
    fun sendCalibration(opcode: CalibrationOpcode)
}

/** Why a step did not advance. Data, not text: the words are read in composition (CLAUDE.md §17). */
sealed interface SetupError {
    data object NotConnected : SetupError
    data object Timeout : SetupError
    data object CalibrationFailed : SetupError
    /** Electrodes (0-based) that did not pass. */
    data class ImpedanceFailed(val failedElectrodes: List<Int>) : SetupError
    /** Sockets (1-based) where a module is seated but could not be identified. */
    data class ZoneModulesMissing(val sockets: List<Int>) : SetupError
    data object NoZoneModules : SetupError
    data object SafetyAcknowledgementRequired : SetupError
}

class SetupCoordinator(
    private val flow: SetupFlow,
    private val link: SetupLink,
    private val impedanceTimeoutMs: Long = IMPEDANCE_TIMEOUT_MS,
    private val calibrationWaitMs: Long = CALIBRATION_WAIT_MS,
) {
    private val _step = MutableStateFlow(flow.currentStep)
    val step: StateFlow<SetupStep> = _step

    private val _isProcessing = MutableStateFlow(false)
    val isProcessing: StateFlow<Boolean> = _isProcessing

    private val _error = MutableStateFlow<SetupError?>(null)
    val error: StateFlow<SetupError?> = _error

    /** The result of this wizard's last impedance check, for the contact display; dropped on leaving the step (UHDR-class). */
    private val _impedanceFlags = MutableStateFlow(0)
    val impedanceFlags: StateFlow<Int> = _impedanceFlags

    private val _safetyAcknowledged = MutableStateFlow(false)
    val safetyAcknowledged: StateFlow<Boolean> = _safetyAcknowledged

    val isFirstSetupComplete: Boolean get() = flow.isFirstSetupComplete

    /** Go to the next step. The safety step does not advance until the acknowledgement is given (ISC-118/119). */
    fun advance() {
        if (_step.value == SetupStep.SAFETY_ACKNOWLEDGEMENT && !flow.safetyAcknowledged) {
            _error.value = SetupError.SafetyAcknowledgementRequired
            return
        }
        // Contact results do not outlive the step that asked for them.
        if (_step.value == SetupStep.IMPEDANCE_CHECK) _impedanceFlags.value = 0
        if (flow.advance() == SetupFlow.AdvanceResult.Advanced) {
            _step.value = flow.currentStep
            _error.value = null
        }
    }

    fun back() {
        if (_step.value == SetupStep.IMPEDANCE_CHECK) _impedanceFlags.value = 0
        flow.back()
        _step.value = flow.currentStep
        _safetyAcknowledged.value = flow.safetyAcknowledged
        _error.value = null
    }

    fun acknowledgeSafety() {
        flow.acknowledgeSafety()
        _safetyAcknowledged.value = true
    }

    fun clearError() {
        _error.value = null
    }

    /** The hub link is the confirmation of pairing: connected, the step is confirmed. */
    fun confirmBlePairing() {
        if (!connected()) {
            _error.value = SetupError.NotConnected
            return
        }
        _error.value = null
        advance()
    }

    /** Send the impedance check and judge the hub's answer to it (ISC-116: at least six of eight electrodes). */
    suspend fun confirmImpedanceCheck() {
        if (!connected()) {
            _error.value = SetupError.NotConnected
            return
        }
        _isProcessing.value = true
        _error.value = null
        _impedanceFlags.value = 0
        try {
            val before = link.impedanceReading.value?.sequence ?: 0L
            link.sendCalibration(CalibrationOpcode.IMPEDANCE_CHECK)
            val reading = withTimeoutOrNull(impedanceTimeoutMs) {
                link.impedanceReading.first { it != null && it.sequence > before }
            }
            if (reading == null) {
                _error.value = SetupError.Timeout
                return
            }
            _impedanceFlags.value = reading.flags
            val result = flow.evaluateImpedance(reading.flags)
            if (result.passed) advance() else _error.value = SetupError.ImpedanceFailed(result.failedElectrodes)
        } finally {
            _isProcessing.value = false
        }
    }

    /** Send the ADS1299 self-calibration, give it time to run, and advance only if the hub is still connected. */
    suspend fun confirmCalibration() {
        if (!connected()) {
            _error.value = SetupError.NotConnected
            return
        }
        _isProcessing.value = true
        _error.value = null
        try {
            link.sendCalibration(CalibrationOpcode.ADS1299_SELF_CAL)
            delay(calibrationWaitMs)
            if (!connected()) _error.value = SetupError.CalibrationFailed else advance()
        } finally {
            _isProcessing.value = false
        }
    }

    /**
     * Any populated socket passes and a faulted one blocks (NP-HEX-ZM-001 §4a: a build is a configuration choice, so there is no module
     * count to check; whether a protocol has the modules it needs is the firmware placement check's question).
     */
    fun confirmZoneModules() {
        when (val r = flow.evaluateZoneModules(link.zoneModules.value)) {
            SetupFlow.ZoneModuleResult.Passed -> advance()
            SetupFlow.ZoneModuleResult.NoneDetected -> _error.value = SetupError.NoZoneModules
            is SetupFlow.ZoneModuleResult.Faulted -> _error.value = SetupError.ZoneModulesMissing(r.socketIds)
        }
    }

    private fun connected() = link.connectionState.value == ConnectionState.CONNECTED

    companion object {
        /** How long the hub is given to answer an impedance check. The Swift app's figure (`UC-104`). */
        const val IMPEDANCE_TIMEOUT_MS = 10_000L

        /** How long the amplifier's self-calibration is given to run. The Swift app's figure (`UC-104`). */
        const val CALIBRATION_WAIT_MS = 3_000L
    }
}
