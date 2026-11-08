package life.neurone.core.models

// PPX_CONFIG (REQ-FWHUB-71): the photoparoxysmal detector's two clinician switches, as the helmet holds them
// (firmware/hub_control/include/np_ppx_config.h). Read: two bytes, the flags and then the evidence level the detector has.
// Write: one byte, the flags. A frame that is not exactly that, with a flag bit this app does not know or an evidence level it
// does not know, is not believed at all.

/** How far the detector has been validated (REQ-FWHUB-70): decides what the override can do. */
enum class PpxEvidence(val rawValue: Int) {
    /** No validation: the helmet refuses flashing at 3 to 30 Hz whatever the switches say. */
    NONE(0),

    /** Bench validation only: the refusal stands unless the clinician's override is on. */
    BENCH(1),

    /** A clinical study supports it: the refusal is replaced. */
    CLINICAL(2);

    companion object {
        fun from(rawValue: Int): PpxEvidence? = entries.firstOrNull { it.rawValue == rawValue }
    }
}

data class PpxConfig(
    /** Halt on a discharge on an occipital channel alone. On by default. */
    val stopOnOccipitalOnly: Boolean,
    /** Lift the refusal where the evidence is bench only. Off by default. */
    val manualOverride: Boolean,
    val evidence: PpxEvidence,
) {
    companion object {
        const val READ_LENGTH = 2
        const val WRITE_LENGTH = 1
        const val FLAG_STOP_OCCIPITAL = 0x01
        const val FLAG_OVERRIDE = 0x02
        private const val KNOWN_FLAGS = FLAG_STOP_OCCIPITAL or FLAG_OVERRIDE

        /** Null for anything the helmet would not send: no part of it is believed. */
        fun fromWire(data: ByteArray): PpxConfig? {
            if (data.size != READ_LENGTH) return null
            val flags = data[0].toInt() and 0xFF
            if (flags and KNOWN_FLAGS.inv() != 0) return null
            val evidence = PpxEvidence.from(data[1].toInt() and 0xFF) ?: return null
            return PpxConfig((flags and FLAG_STOP_OCCIPITAL) != 0, (flags and FLAG_OVERRIDE) != 0, evidence)
        }

        /** The write for the two switches. */
        fun toWire(stopOnOccipitalOnly: Boolean, manualOverride: Boolean): ByteArray {
            var flags = 0
            if (stopOnOccipitalOnly) flags = flags or FLAG_STOP_OCCIPITAL
            if (manualOverride) flags = flags or FLAG_OVERRIDE
            return byteArrayOf(flags.toByte())
        }
    }
}
