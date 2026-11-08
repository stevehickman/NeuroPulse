package life.neurone.core.models

// SESSION_HOLD (SWR-SS-09, REQ-FWHUB-72): why the helmet is holding a session paused, one byte
// (firmware/hub_control/include/np_session_hold.h). It lets the app tell a pause the person asked for from a halt the helmet
// made on its own, and put the Stop / Resume question to the person for the second. A byte it does not know is not believed.

enum class HoldCause(val rawValue: Int) {
    /** Not held. */
    NONE(0),

    /** The person paused the session. */
    USER_PAUSE(1),

    /** The person pressed the panic control. */
    USER_PANIC(2),

    /** The photoparoxysmal detector found a discharge and the helmet halted on its own. */
    DETECTOR_DISCHARGE(3),

    /** The detector could no longer see the EEG signal and the helmet halted on its own. */
    DETECTOR_BLIND(4);

    /** The helmet halted without the person asking: the question is the app's to put (SWR-SS-09). */
    val isAutomated: Boolean get() = this == DETECTOR_DISCHARGE || this == DETECTOR_BLIND

    companion object {
        const val LENGTH = 1

        fun from(rawValue: Int): HoldCause? = entries.firstOrNull { it.rawValue == rawValue }

        /** Null for a frame that is not exactly one byte or holds a cause this app does not know. */
        fun fromWire(data: ByteArray): HoldCause? {
            if (data.size != LENGTH) return null
            return from(data[0].toInt() and 0xFF)
        }
    }
}
